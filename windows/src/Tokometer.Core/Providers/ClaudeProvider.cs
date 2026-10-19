using System.Net;
using System.Net.Http.Headers;
using System.Text.Json;

namespace Tokometer.Core.Providers;

/// <summary>
/// Claude usage from the account usage route Claude Code itself uses. The
/// route is undocumented and rate limited per token, so <see cref="ClaudePollGate"/>
/// spaces polls to every 15 minutes, or every minute near the five-hour limit,
/// and the last reading is served in between.
/// </summary>
public sealed class ClaudeProvider(
    HttpClient http,
    Func<string?> readToken,
    ClaudeReadingFile cache,
    ClaudePollGate gate) : IUsageProvider
{
    private static readonly Uri Endpoint = new("https://api.anthropic.com/api/oauth/usage");

    public ProviderId Id => ProviderId.Claude;

    public TimeSpan RefreshInterval => TimeSpan.FromMinutes(1);

    public async Task<ProviderSnapshot> FetchAsync(DateTimeOffset now, CancellationToken cancellation)
    {
        var known = cache.Read();
        if (known is not null && known.IsFresh(now) && known.HasSubscriptionWindows) return Snapshot(known, now);
        try
        {
            return Snapshot(await PollAsync(now, known?.Windows ?? [], cancellation), now);
        }
        catch (ProviderFetchException) when (known is not null)
        {
            return Snapshot(known, now);
        }
    }

    private async Task<ClaudeReading> PollAsync(DateTimeOffset now, IReadOnlyList<UsageWindow> knownWindows,
        CancellationToken cancellation)
    {
        if (!gate.IsOpen(now, knownWindows)) throw ProviderFetchException.Unavailable("Aguardando a próxima consulta do Claude");
        var token = readToken();
        if (string.IsNullOrEmpty(token)) throw ProviderFetchException.SignedOut("Entre no Claude Code");
        var tokenId = ClaudePollGate.TokenId(token);
        if (gate.Rejects(tokenId)) throw ProviderFetchException.NeedsAuth("Entre novamente no Claude Code");
        var windows = await RequestWindowsAsync(token, tokenId, now, cancellation);
        gate.RecordSuccess(now, windows);
        var reading = new ClaudeReading(windows, now, Fidelity.Derived);
        cache.Write(reading);
        return reading;
    }

    private async Task<IReadOnlyList<UsageWindow>> RequestWindowsAsync(string token, string tokenId, DateTimeOffset now,
        CancellationToken cancellation)
    {
        using var request = new HttpRequestMessage(HttpMethod.Get, Endpoint);
        request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", token);
        request.Headers.Add("anthropic-beta", "oauth-2025-04-20");
        using var response = await http.SendAsync(request, cancellation);
        switch (response.StatusCode)
        {
            case HttpStatusCode.OK:
                break;
            case HttpStatusCode.Unauthorized or HttpStatusCode.Forbidden:
                gate.Reject(tokenId);
                throw ProviderFetchException.NeedsAuth("Entre novamente no Claude Code");
            case HttpStatusCode.TooManyRequests:
                throw ProviderFetchException.RateLimited(gate.RecordRateLimit(now, response.Headers.RetryAfter?.Delta));
            default:
                throw ProviderFetchException.Unavailable($"Claude indisponível (HTTP {(int)response.StatusCode})");
        }
        var windows = ParseWindows(await response.Content.ReadAsStringAsync(cancellation));
        if (windows.Count == 0) throw ProviderFetchException.Unsupported("O Claude não informou limites para esta conta");
        return windows;
    }

    internal static IReadOnlyList<UsageWindow> ParseWindows(string body)
    {
        using var document = JsonDocument.Parse(body);
        return new[] { "five_hour", "seven_day" }
            .Select(id => Json.Property(document.RootElement, id) is { } window ? ParseWindow(id, window) : null)
            .OfType<UsageWindow>()
            .ToList();
    }

    private static UsageWindow? ParseWindow(string id, JsonElement window)
    {
        var utilization = Json.Number(window, "utilization");
        var reset = Json.Date(Json.String(window, "resets_at"));
        return utilization is null || reset is null
            ? null
            : new UsageWindow(id, UsageWindow.DefaultLabel(id), UsageWindow.Clamp(utilization.Value / 100), reset);
    }

    private ProviderSnapshot Snapshot(ClaudeReading reading, DateTimeOffset now) => new(
        Id,
        reading.IsFresh(now) ? StatusKind.Ok : StatusKind.Stale,
        null,
        reading.Windows,
        reading.Fidelity,
        reading.ObservedAt);

    /// <summary>Claude Code on Windows keeps its login in <c>~/.claude/.credentials.json</c>.</summary>
    public static string? ReadCredentialsToken(string credentialsFile)
    {
        if (!File.Exists(credentialsFile)) return null;
        using var document = JsonDocument.Parse(File.ReadAllText(credentialsFile));
        return Json.Property(document.RootElement, "claudeAiOauth") is { } oauth ? Json.String(oauth, "accessToken") : null;
    }
}
