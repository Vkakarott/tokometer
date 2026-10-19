using System.Net.Http.Headers;
using System.Net.Http.Json;

namespace Tokometer.Core;

/// <summary>Backend address and collector token, read from the same <c>config.sh</c> the macOS collectors use.</summary>
public sealed record BackendConfig(Uri Url, string Token)
{
    public static BackendConfig? Load(string file) => File.Exists(file) ? Parse(File.ReadAllText(file)) : null;

    /// <summary>Reads plain <c>export KEY="value"</c> lines; the file is never executed.</summary>
    public static BackendConfig? Parse(string text)
    {
        var values = text.Split('\n').Select(Assignment).OfType<KeyValuePair<string, string>>()
            .GroupBy(pair => pair.Key).ToDictionary(group => group.Key, group => group.Last().Value);
        return values.TryGetValue("TOKESP_URL", out var raw) && Uri.TryCreate(raw, UriKind.Absolute, out var url)
               && values.TryGetValue("TOKESP_TOKEN", out var token) && token.Length > 0
            ? new BackendConfig(url, token)
            : null;
    }

    private static KeyValuePair<string, string>? Assignment(string line)
    {
        var statement = line.Trim();
        if (statement.StartsWith('#')) return null;
        if (statement.StartsWith("export ")) statement = statement["export ".Length..];
        var equals = statement.IndexOf('=');
        if (equals <= 0) return null;
        var value = statement[(equals + 1)..].Trim();
        if (value.Length >= 2 && value[0] is '"' or '\'' && value[^1] == value[0]) value = value[1..^1];
        return new(statement[..equals].Trim(), value);
    }
}

/// <summary>
/// Optional: forwards readings to a tokometer backend, which feeds the ESP32
/// display. Without a <c>config.sh</c> nothing is sent and the app works alone.
/// </summary>
public sealed class BackendPublisher(HttpClient http, Func<BackendConfig?> loadConfig)
{
    private readonly Dictionary<ProviderId, ProviderSnapshot> _lastSent = [];
    private string[]? _pendingSelection;
    private string[]? _sentSelection;

    public async Task PublishAsync(ProviderSnapshot snapshot, CancellationToken cancellation)
    {
        var payload = IngestPayload(snapshot);
        if (payload is null || _lastSent.GetValueOrDefault(snapshot.Id) == snapshot) return;
        if (await SendAsync(HttpMethod.Post, "ingest", payload, cancellation)) _lastSent[snapshot.Id] = snapshot;
    }

    public async Task PublishProviderSelectionAsync(IEnumerable<ProviderId> visible, CancellationToken cancellation)
    {
        _pendingSelection = Enum.GetValues<ProviderId>().Where(visible.Contains).Select(id => id.ApiName()).ToArray();
        await FlushProviderSelectionAsync(cancellation);
    }

    public async Task FlushProviderSelectionAsync(CancellationToken cancellation)
    {
        if (_pendingSelection is not { } selection || (_sentSelection?.SequenceEqual(selection) ?? false)) return;
        if (await SendAsync(HttpMethod.Put, "providers", new { enabled = selection }, cancellation)) _sentSelection = selection;
    }

    /// <summary>Nil when there is nothing worth sending: an empty reading would erase the backend's data.</summary>
    internal static object? IngestPayload(ProviderSnapshot snapshot)
    {
        var windows = snapshot.Windows
            .Where(w => w.UsedFraction is not null && w.ResetsAt is not null)
            .Select(w => new { id = w.Id, usedPercentage = w.UsedFraction!.Value * 100, resetsAt = w.ResetsAt!.Value.ToUnixTimeSeconds() })
            .ToList();
        if (windows.Count == 0 || snapshot.ObservedAt is not { } observed) return null;
        return new
        {
            provider = snapshot.Id.CollectorName(),
            source = Environment.MachineName.ToLowerInvariant(),
            observedAt = observed.ToUnixTimeSeconds(),
            windows,
        };
    }

    /// <summary>Keeps a path prefix in the configured URL, as the macOS app does.</summary>
    internal static Uri Endpoint(Uri baseUrl, string path) => new(new Uri(baseUrl.ToString().TrimEnd('/') + "/"), path);

    private async Task<bool> SendAsync(HttpMethod method, string path, object body, CancellationToken cancellation)
    {
        if (loadConfig() is not { } config) return false;
        try
        {
            using var request = new HttpRequestMessage(method, Endpoint(config.Url, path)) { Content = JsonContent.Create(body) };
            request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", config.Token);
            using var response = await http.SendAsync(request, cancellation);
            return response.IsSuccessStatusCode;
        }
        catch (HttpRequestException)
        {
            return false;
        }
    }
}
