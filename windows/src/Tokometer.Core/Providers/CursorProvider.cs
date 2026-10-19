using System.Net;
using System.Text.Json;
using Microsoft.Data.Sqlite;

namespace Tokometer.Core.Providers;

/// <summary>Reads the Cursor session from its local database and asks Cursor for the plan usage.</summary>
public sealed class CursorProvider(HttpClient http, string stateFile) : IUsageProvider
{
    private static readonly Uri Endpoint = new("https://cursor.com/api/usage-summary");

    public ProviderId Id => ProviderId.Cursor;

    public TimeSpan RefreshInterval => TimeSpan.FromMinutes(5);

    public async Task<ProviderSnapshot> FetchAsync(DateTimeOffset now, CancellationToken cancellation)
    {
        var token = ReadToken(stateFile);
        using var request = new HttpRequestMessage(HttpMethod.Get, Endpoint);
        request.Headers.Add("Cookie", $"WorkosCursorSessionToken=::{token}");
        using var response = await http.SendAsync(request, cancellation);
        if (response.StatusCode is HttpStatusCode.Unauthorized or HttpStatusCode.Forbidden)
            throw ProviderFetchException.NeedsAuth("Entre novamente no Cursor");
        if (response.StatusCode != HttpStatusCode.OK)
            throw ProviderFetchException.Unavailable($"Cursor indisponível (HTTP {(int)response.StatusCode})");
        var (windows, unlimited) = ParseUsage(await response.Content.ReadAsStringAsync(cancellation));
        if (windows.Count == 0)
            throw ProviderFetchException.Unsupported(unlimited ? "O plano Cursor é ilimitado" : "O Cursor não informou consumo");
        return ProviderSnapshot.Fresh(Id, windows, now);
    }

    internal static string ReadToken(string stateFile)
    {
        if (!File.Exists(stateFile)) throw ProviderFetchException.SignedOut("Abra o Cursor e entre na sua conta");
        var raw = QueryToken(stateFile)?.Trim()
            ?? throw ProviderFetchException.SignedOut("Entre no Cursor");
        var token = raw.StartsWith('"') ? JsonSerializer.Deserialize<string>(raw) ?? raw : raw;
        if (token.Split('.').Length != 3) throw ProviderFetchException.NeedsAuth("Entre novamente no Cursor");
        return token;
    }

    private static string? QueryToken(string stateFile)
    {
        try
        {
            var source = new SqliteConnectionStringBuilder { DataSource = stateFile, Mode = SqliteOpenMode.ReadOnly };
            using var connection = new SqliteConnection(source.ToString());
            connection.Open();
            using var command = connection.CreateCommand();
            command.CommandText = "SELECT value FROM ItemTable WHERE key = 'cursorAuth/accessToken'";
            return command.ExecuteScalar() as string;
        }
        catch (SqliteException)
        {
            throw ProviderFetchException.Unavailable("Não foi possível ler a sessão do Cursor");
        }
    }

    internal static (IReadOnlyList<UsageWindow> Windows, bool Unlimited) ParseUsage(string body)
    {
        using var document = JsonDocument.Parse(body);
        var root = document.RootElement;
        var reset = Json.Date(Json.String(root, "billingCycleEnd"));
        var usage = Json.Property(root, "individualUsage") ?? default;
        var plan = Json.Property(usage, "plan") ?? default;
        var windows = new List<UsageWindow>();
        if (Json.Number(plan, "totalPercentUsed") is { } total)
            windows.Add(new UsageWindow("included", "Uso incluído", UsageWindow.Clamp(total / 100), reset));
        if (Json.Number(plan, "apiPercentUsed") is > 0 and var api)
            windows.Add(new UsageWindow("api", "Uso de API", UsageWindow.Clamp(api / 100), reset));
        if (OnDemandFraction(Json.Property(usage, "onDemand") ?? default) is { } onDemand)
            windows.Add(new UsageWindow("on_demand", "Sob demanda", onDemand, reset));
        return (windows, Json.Bool(root, "isUnlimited") ?? false);
    }

    private static double? OnDemandFraction(JsonElement spend)
    {
        if (Json.Bool(spend, "enabled") != true) return null;
        var used = Json.Number(spend, "used");
        var limit = Json.Number(spend, "limit");
        return used is null || limit is not > 0 ? null : UsageWindow.Clamp(used.Value / limit.Value);
    }
}
