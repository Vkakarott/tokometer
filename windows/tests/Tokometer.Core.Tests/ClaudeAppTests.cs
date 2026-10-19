using System.Net;
using System.Security.Cryptography;
using System.Text;
using Microsoft.Data.Sqlite;
using Tokometer.Core.Providers;

namespace Tokometer.Core.Tests;

public class ClaudeAppTests
{
    private const string Organization = "5f0e1f6a-3c1e-4c5b-9d7a-2f1b8e6c4a10";
    private static readonly byte[] Key = RandomNumberGenerator.GetBytes(32);
    // Stands in for DPAPI, which only exists on Windows.
    private static byte[] Identity(byte[] data) => data;

    [Fact]
    public void Reads_encrypted_cookies_of_the_domain()
    {
        using var temp = new TempDirectory();
        CreateProfile(temp.Path, version: 24, ("claude.ai", "lastActiveOrg", Organization), (".claude.ai", "sessionKey", "sk-ant-sid01-abc"),
            (".anthropic.com", "sessionKey", "other"));

        var cookies = new ChromiumCookies(temp.Path, Identity).Read("claude.ai", ["sessionKey", "lastActiveOrg"]);

        Assert.Equal("sk-ant-sid01-abc", cookies["sessionKey"]);
        Assert.Equal(Organization, cookies["lastActiveOrg"]);
    }

    [Fact]
    public void Older_databases_have_no_host_hash()
    {
        using var temp = new TempDirectory();
        CreateProfile(temp.Path, version: 23, (".claude.ai", "sessionKey", "sk-ant-sid01-abc"));

        var cookies = new ChromiumCookies(temp.Path, Identity).Read("claude.ai", ["sessionKey"]);

        Assert.Equal("sk-ant-sid01-abc", cookies["sessionKey"]);
    }

    [Fact]
    public void Login_is_kept_and_dropped_when_the_app_signs_out()
    {
        using var temp = new TempDirectory();
        var cache = temp.File("login.bin");
        CreateProfile(temp.Path, version: 24, (".claude.ai", "sessionKey", "sk-ant-sid01-abc"));
        var login = new ClaudeAppLogin(() => [temp.Path], Identity, Identity, cache);

        Assert.Equal(new ClaudeWebLogin("sk-ant-sid01-abc"), login.Read());
        Assert.True(File.Exists(cache));

        CreateProfile(temp.Path, version: 24);
        Assert.Null(login.Read());
        Assert.False(File.Exists(cache));
    }

    [Fact]
    public void Without_the_app_there_is_no_login()
    {
        using var temp = new TempDirectory();

        Assert.Null(new ClaudeAppLogin(() => [temp.Path], Identity, Identity, temp.File("login.bin")).Read());
    }

    [Fact]
    public async Task Usage_uses_the_organization_that_can_chat_and_the_session_cookie()
    {
        var organizations = $$"""[{"uuid":"api-only","capabilities":["api"]},{"uuid":"{{Organization}}","capabilities":["chat","claude_max"]}]""";
        var handler = new FakeHandler().Respond(HttpStatusCode.OK, organizations).Respond(HttpStatusCode.OK, Usage).Respond(HttpStatusCode.OK, Usage);
        var provider = new ClaudeWebProvider(new HttpClient(handler), () => new ClaudeWebLogin("sk-1"), () => { });

        var snapshot = await provider.FetchAsync(Times.Now, default);
        await provider.FetchAsync(Times.Now, default);

        Assert.Equal($"https://claude.ai/api/organizations/{Organization}/usage", handler.Requests[1].RequestUri!.ToString());
        Assert.Equal("sessionKey=sk-1", handler.Requests[1].Headers.GetValues("Cookie").Single());
        Assert.Equal(3, handler.Requests.Count);
        Assert.Equal(["five_hour", "seven_day"], snapshot.Windows.Select(w => w.Id));
        Assert.Equal(0.25, snapshot.Windows[0].UsedFraction);
    }

    [Fact]
    public void The_newest_app_profile_wins()
    {
        using var stale = new TempDirectory();
        using var live = new TempDirectory();
        CreateProfile(stale.Path, version: 24, (".claude.ai", "sessionKey", "old"));
        CreateProfile(live.Path, version: 24, (".claude.ai", "sessionKey", "new"));
        File.SetLastWriteTimeUtc(Path.Combine(stale.Path, "Network", "Cookies"), DateTime.UtcNow.AddDays(-30));

        var login = new ClaudeAppLogin(() => [stale.Path, live.Path], Identity, Identity, live.File("login.bin")).Read();

        Assert.Equal("new", login?.SessionKey);
    }

    [Fact]
    public async Task Rejected_session_is_forgotten()
    {
        var forgotten = false;
        var handler = new FakeHandler().Respond(HttpStatusCode.OK, OneOrganization).Respond(HttpStatusCode.Unauthorized);
        var provider = new ClaudeWebProvider(new HttpClient(handler), () => new ClaudeWebLogin("sk-1"), () => forgotten = true);

        var error = await Assert.ThrowsAsync<ProviderFetchException>(() => provider.FetchAsync(Times.Now, default));

        Assert.Equal(FetchFailure.NeedsAuth, error.Failure);
        Assert.True(forgotten);
    }

    [Fact]
    public async Task A_challenge_page_is_not_a_rejected_login()
    {
        var handler = new FakeHandler().Respond(HttpStatusCode.OK, OneOrganization).Respond(HttpStatusCode.Forbidden, "<html></html>",
            response => response.Content = new StringContent("<html></html>", Encoding.UTF8, "text/html"));
        var provider = new ClaudeWebProvider(new HttpClient(handler), () => new ClaudeWebLogin("sk-1"), () => { });

        var error = await Assert.ThrowsAsync<ProviderFetchException>(() => provider.FetchAsync(Times.Now, default));

        Assert.Equal(FetchFailure.Unavailable, error.Failure);
    }

    [Fact]
    public async Task No_app_login_counts_as_signed_out()
    {
        var provider = new ClaudeWebProvider(new HttpClient(new FakeHandler()), () => null, () => { });

        var error = await Assert.ThrowsAsync<ProviderFetchException>(() => provider.FetchAsync(Times.Now, default));

        Assert.Equal(FetchFailure.SignedOut, error.Failure);
    }

    private const string OneOrganization = $$"""[{"uuid":"{{Organization}}","capabilities":["chat"]}]""";

    private const string Usage = """
        {"five_hour":{"utilization":25.0,"resets_at":"2026-10-19T20:00:00.123456+00:00"},
         "seven_day":{"utilization":61.0,"resets_at":"2026-10-23T12:00:00+00:00"},"seven_day_opus":null}
        """;

    /// <summary>A Chromium profile: the DPAPI-wrapped key in Local State and AES-GCM cookies.</summary>
    private static void CreateProfile(string profile, int version, params (string Host, string Name, string Value)[] cookies)
    {
        File.WriteAllText(Path.Combine(profile, "Local State"),
            $$$"""{"os_crypt":{"encrypted_key":"{{{Convert.ToBase64String([.. "DPAPI"u8, .. Key])}}}"}}""");
        Directory.CreateDirectory(Path.Combine(profile, "Network"));
        var file = Path.Combine(profile, "Network", "Cookies");
        File.Delete(file);
        using var connection = new SqliteConnection(new SqliteConnectionStringBuilder { DataSource = file, Pooling = false }.ToString());
        connection.Open();
        Execute(connection, "CREATE TABLE meta (key TEXT, value TEXT); CREATE TABLE cookies (host_key TEXT, name TEXT, value TEXT, encrypted_value BLOB)");
        Execute(connection, $"INSERT INTO meta VALUES ('version', '{version}')");
        foreach (var (host, name, value) in cookies)
        {
            using var insert = connection.CreateCommand();
            insert.CommandText = "INSERT INTO cookies VALUES ($host, $name, '', $value)";
            insert.Parameters.AddWithValue("$host", host);
            insert.Parameters.AddWithValue("$name", name);
            insert.Parameters.AddWithValue("$value", Encrypt(host, value, hostHash: version >= 24));
            insert.ExecuteNonQuery();
        }
    }

    private static byte[] Encrypt(string host, string value, bool hostHash)
    {
        byte[] plain = hostHash ? [.. SHA256.HashData(Encoding.UTF8.GetBytes(host)), .. Encoding.UTF8.GetBytes(value)] : Encoding.UTF8.GetBytes(value);
        var nonce = RandomNumberGenerator.GetBytes(12);
        var cipher = new byte[plain.Length];
        var tag = new byte[16];
        using var aes = new AesGcm(Key, 16);
        aes.Encrypt(nonce, plain, cipher, tag);
        return [.. "v10"u8, .. nonce, .. cipher, .. tag];
    }

    private static void Execute(SqliteConnection connection, string sql)
    {
        using var command = connection.CreateCommand();
        command.CommandText = sql;
        command.ExecuteNonQuery();
    }
}
