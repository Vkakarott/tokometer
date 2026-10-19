using System.Net;
using System.Net.Http.Headers;
using System.Text.Json;

namespace Tokometer.Core.Providers;

/// <summary>Reads the Codex CLI login and asks ChatGPT for the account's rate limits.</summary>
public sealed class CodexProvider(HttpClient http, string authFile) : IUsageProvider
{
    private static readonly Uri Endpoint = new("https://chatgpt.com/backend-api/wham/usage");

    public ProviderId Id => ProviderId.Codex;

    public TimeSpan RefreshInterval => TimeSpan.FromMinutes(1);

    public async Task<ProviderSnapshot> FetchAsync(DateTimeOffset now, CancellationToken cancellation)
    {
        var (token, account) = ReadLogin(authFile);
        using var request = new HttpRequestMessage(HttpMethod.Get, Endpoint);
        request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", token);
        request.Headers.Add("ChatGPT-Account-Id", account);
        request.Headers.UserAgent.ParseAdd("codex_cli_rs");
        using var response = await http.SendAsync(request, cancellation);
        EnsureSuccess(response.StatusCode);
        var windows = ParseWindows(await response.Content.ReadAsStringAsync(cancellation));
        if (windows.Count == 0)
            throw ProviderFetchException.Unsupported("O Codex não informou limites para esta conta");
        return ProviderSnapshot.Fresh(Id, windows, now);
    }

    internal static (string Token, string Account) ReadLogin(string authFile)
    {
        if (!File.Exists(authFile)) throw ProviderFetchException.NeedsAuth("Entre no Codex CLI");
        using var document = JsonDocument.Parse(File.ReadAllText(authFile));
        var tokens = document.RootElement.TryGetProperty("tokens", out var value) ? value : default;
        var token = Json.String(tokens, "access_token");
        var account = Json.String(tokens, "account_id");
        if (token is null || account is null) throw ProviderFetchException.NeedsAuth("Entre novamente no Codex");
        return (token, account);
    }

    internal static IReadOnlyList<UsageWindow> ParseWindows(string body)
    {
        using var document = JsonDocument.Parse(body);
        if (!document.RootElement.TryGetProperty("rate_limit", out var limits)) return [];
        return new[] { "primary_window", "secondary_window" }
            .Select(name => limits.TryGetProperty(name, out var window) ? ParseWindow(window) : null)
            .OfType<UsageWindow>()
            .ToList();
    }

    private static UsageWindow? ParseWindow(JsonElement window)
    {
        var percent = Json.Number(window, "used_percent");
        var resetAt = Json.Number(window, "reset_at");
        if (percent is null || resetAt is null) return null;
        var id = Json.Number(window, "limit_window_seconds") switch
        {
            18_000 => "five_hour",
            604_800 => "seven_day",
            _ => null,
        };
        if (id is null) return null;
        return new UsageWindow(id, UsageWindow.DefaultLabel(id), UsageWindow.Clamp(percent.Value / 100),
            DateTimeOffset.FromUnixTimeSeconds((long)resetAt.Value));
    }

    private static void EnsureSuccess(HttpStatusCode status)
    {
        if (status is HttpStatusCode.Unauthorized or HttpStatusCode.Forbidden)
            throw ProviderFetchException.NeedsAuth("Entre novamente no Codex");
        if (status != HttpStatusCode.OK)
            throw ProviderFetchException.Unavailable($"Codex indisponível (HTTP {(int)status})");
    }
}
