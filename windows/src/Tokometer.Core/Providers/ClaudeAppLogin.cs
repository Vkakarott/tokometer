using System.Text;
using System.Text.Json;

namespace Tokometer.Core.Providers;

/// <summary>A claude.ai web session cookie.</summary>
public sealed record ClaudeWebLogin(string SessionKey);

/// <summary>
/// The login of the Claude desktop app, taken from its cookies. The last one
/// read is kept, DPAPI protected, for when the app holds its cookies locked.
/// </summary>
public sealed class ClaudeAppLogin(
    Func<IEnumerable<string>> profiles,
    Func<byte[], byte[]> unprotect,
    Func<byte[], byte[]> protect,
    string cacheFile)
{
    private const string Domain = "claude.ai";
    private static readonly string[] CookieNames = ["sessionKey"];

    /// <summary>The current login, or null when the Claude app is not signed in on this PC.</summary>
    public ClaudeWebLogin? Read()
    {
        // A Store install can leave an older installer profile behind; the newest cookies are the live ones.
        var cookies = profiles()
            .Select(profile => new ChromiumCookies(profile, unprotect))
            .Where(c => c.CookiesFile is not null)
            .MaxBy(c => File.GetLastWriteTimeUtc(c.CookiesFile!));
        if (cookies is null) return null;
        try
        {
            var found = cookies.Read(Domain, CookieNames);
            var login = found.TryGetValue("sessionKey", out var session) ? new ClaudeWebLogin(session) : null;
            Save(login);
            return login;
        }
        catch (IOException)
        {
            return Cached() ?? throw ProviderFetchException.Unavailable("Feche o app Claude uma vez para o Tokometer ler o login");
        }
    }

    /// <summary>Drops the kept login after claude.ai rejected it.</summary>
    public void Forget() => Save(null);

    private ClaudeWebLogin? Cached()
    {
        try
        {
            if (!File.Exists(cacheFile)) return null;
            var json = Encoding.UTF8.GetString(unprotect(File.ReadAllBytes(cacheFile)));
            return JsonSerializer.Deserialize<ClaudeWebLogin>(json);
        }
        catch (Exception error) when (error is IOException or JsonException or System.Security.Cryptography.CryptographicException)
        {
            return null;
        }
    }

    private void Save(ClaudeWebLogin? login)
    {
        if (login is null)
        {
            File.Delete(cacheFile);
            return;
        }
        Directory.CreateDirectory(Path.GetDirectoryName(cacheFile)!);
        File.WriteAllBytes(cacheFile, protect(Encoding.UTF8.GetBytes(JsonSerializer.Serialize(login))));
    }
}
