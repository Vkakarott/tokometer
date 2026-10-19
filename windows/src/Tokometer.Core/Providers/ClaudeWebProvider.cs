using System.Net;
using System.Text.Json;

namespace Tokometer.Core.Providers;

/// <summary>
/// Claude usage through claude.ai with the Claude desktop app's session, for
/// people who use the app instead of Claude Code. The route is the one the
/// app's own usage page calls, so it is undocumented and may change.
/// </summary>
public sealed class ClaudeWebProvider(HttpClient http, Func<ClaudeWebLogin?> readLogin, Action forgetLogin) : IUsageProvider
{
    private static readonly Uri Base = new("https://claude.ai/api/");
    private const string BrowserAgent =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36";
    private string? _organization;

    public ProviderId Id => ProviderId.Claude;

    public TimeSpan RefreshInterval => TimeSpan.FromMinutes(5);

    public async Task<ProviderSnapshot> FetchAsync(DateTimeOffset now, CancellationToken cancellation)
    {
        var login = readLogin() ?? throw ProviderFetchException.SignedOut("Entre no app Claude");
        var organization = login.OrganizationId ?? _organization ?? await FirstOrganizationAsync(login, cancellation);
        _organization = organization;
        var body = await GetAsync($"organizations/{organization}/usage", login, cancellation);
        var windows = ClaudeProvider.ParseWindows(body);
        if (windows.Count == 0) throw ProviderFetchException.Unsupported("O Claude não informou limites para esta conta");
        return ProviderSnapshot.Fresh(Id, windows, now);
    }

    private async Task<string> FirstOrganizationAsync(ClaudeWebLogin login, CancellationToken cancellation)
    {
        var body = await GetAsync("organizations", login, cancellation);
        return PickOrganization(body) ?? throw ProviderFetchException.Unsupported("Nenhuma conta Claude encontrada neste login");
    }

    /// <summary>The first organization that can chat; API-only organizations have no plan limits.</summary>
    internal static string? PickOrganization(string body)
    {
        using var document = JsonDocument.Parse(body);
        if (document.RootElement.ValueKind != JsonValueKind.Array) return null;
        var organizations = document.RootElement.EnumerateArray().ToList();
        var chat = organizations.FirstOrDefault(CanChat);
        var picked = chat.ValueKind == JsonValueKind.Object ? chat : organizations.FirstOrDefault();
        return picked.ValueKind == JsonValueKind.Object ? Json.String(picked, "uuid") : null;
    }

    private static bool CanChat(JsonElement organization) =>
        Json.Property(organization, "capabilities") is { ValueKind: JsonValueKind.Array } capabilities
        && capabilities.EnumerateArray().Any(c => c.ValueKind == JsonValueKind.String && c.GetString() == "chat");

    private async Task<string> GetAsync(string path, ClaudeWebLogin login, CancellationToken cancellation)
    {
        using var request = new HttpRequestMessage(HttpMethod.Get, new Uri(Base, path));
        request.Headers.Add("Cookie", $"sessionKey={login.SessionKey}");
        request.Headers.UserAgent.ParseAdd(BrowserAgent);
        request.Headers.Accept.ParseAdd("application/json");
        using var response = await http.SendAsync(request, cancellation);
        EnsureSuccess(response);
        return await response.Content.ReadAsStringAsync(cancellation);
    }

    private void EnsureSuccess(HttpResponseMessage response)
    {
        var json = response.Content.Headers.ContentType?.MediaType?.Contains("json") == true;
        switch (response.StatusCode)
        {
            case HttpStatusCode.OK:
                return;
            // A Cloudflare challenge also answers 403, but with a page, not JSON.
            case HttpStatusCode.Unauthorized:
            case HttpStatusCode.Forbidden when json:
                forgetLogin();
                _organization = null;
                throw ProviderFetchException.NeedsAuth("Entre novamente no app Claude");
            case HttpStatusCode.TooManyRequests:
                throw ProviderFetchException.RateLimited(response.Headers.RetryAfter?.Delta ?? TimeSpan.FromMinutes(10));
            default:
                throw ProviderFetchException.Unavailable($"claude.ai indisponível (HTTP {(int)response.StatusCode})");
        }
    }
}
