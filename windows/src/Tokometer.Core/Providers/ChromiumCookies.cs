using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using Microsoft.Data.Sqlite;

namespace Tokometer.Core.Providers;

/// <summary>
/// Reads cookies from a Chromium profile, such as the Claude desktop app's.
/// Values are AES-GCM encrypted with a key that Windows DPAPI protects for the
/// signed-in user; <c>unprotect</c> is that DPAPI call, injected so this stays
/// testable off Windows.
/// </summary>
public sealed class ChromiumCookies(string profileDirectory, Func<byte[], byte[]> unprotect)
{
    private const int NonceSize = 12;
    private const int TagSize = 16;
    // From cookie database version 24 on, the plaintext starts with SHA-256(host).
    private const int HostHashVersion = 24;
    private const int HostHashSize = 32;

    public string? CookiesFile =>
        new[] { Path.Combine(profileDirectory, "Network", "Cookies"), Path.Combine(profileDirectory, "Cookies") }
            .FirstOrDefault(File.Exists);

    /// <summary>
    /// The named cookies of <paramref name="domain"/> and its subdomain cookies.
    /// Throws <see cref="IOException"/> when the app holds the database locked.
    /// </summary>
    public IReadOnlyDictionary<string, string> Read(string domain, IReadOnlyCollection<string> names)
    {
        if (CookiesFile is not { } cookies) return new Dictionary<string, string>();
        var key = MasterKey();
        var copy = Path.Combine(Path.GetTempPath(), $"tokometer-{Guid.NewGuid():N}.db");
        try
        {
            CopyShared(cookies, copy);
            return Query(copy, domain, names, key);
        }
        finally
        {
            File.Delete(copy);
        }
    }

    private byte[]? MasterKey()
    {
        var localState = Path.Combine(profileDirectory, "Local State");
        if (!File.Exists(localState)) return null;
        using var document = JsonDocument.Parse(File.ReadAllText(localState));
        var encoded = Json.Property(document.RootElement, "os_crypt") is { } crypt ? Json.String(crypt, "encrypted_key") : null;
        if (encoded is null) return null;
        var blob = Convert.FromBase64String(encoded);
        var prefix = "DPAPI"u8;
        return blob.AsSpan().StartsWith(prefix) ? unprotect(blob[prefix.Length..]) : null;
    }

    /// <summary>The app keeps the database open, so copy it with every share flag and read the copy.</summary>
    private static void CopyShared(string source, string destination)
    {
        using var input = new FileStream(source, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        using var output = File.Create(destination);
        input.CopyTo(output);
    }

    private Dictionary<string, string> Query(string database, string domain, IReadOnlyCollection<string> names, byte[]? key)
    {
        var source = new SqliteConnectionStringBuilder { DataSource = database, Mode = SqliteOpenMode.ReadOnly, Pooling = false };
        using var connection = new SqliteConnection(source.ToString());
        connection.Open();
        var hostHash = Version(connection) >= HostHashVersion;
        using var command = connection.CreateCommand();
        command.CommandText = "SELECT name, value, encrypted_value FROM cookies WHERE host_key IN ($host, $subdomains)";
        command.Parameters.AddWithValue("$host", domain);
        command.Parameters.AddWithValue("$subdomains", "." + domain);
        var found = new Dictionary<string, string>();
        using var reader = command.ExecuteReader();
        while (reader.Read())
        {
            var name = reader.GetString(0);
            if (!names.Contains(name)) continue;
            var value = reader.GetString(1);
            if (value.Length == 0) value = Decrypt((byte[])reader[2], key, hostHash) ?? "";
            if (value.Length > 0) found[name] = value;
        }
        return found;
    }

    private static int Version(SqliteConnection connection)
    {
        using var command = connection.CreateCommand();
        command.CommandText = "SELECT value FROM meta WHERE key = 'version'";
        return int.TryParse(command.ExecuteScalar() as string, out var version) ? version : 0;
    }

    /// <summary>The cookie's text, or null when it cannot be decrypted with this key.</summary>
    internal string? Decrypt(byte[] blob, byte[]? key, bool hostHash)
    {
        try
        {
            return DecryptOrThrow(blob, key, hostHash);
        }
        catch (CryptographicException)
        {
            return null;
        }
    }

    private string? DecryptOrThrow(byte[] blob, byte[]? key, bool hostHash)
    {
        if (blob.Length == 0) return null;
        if (!blob.AsSpan().StartsWith("v10"u8) && !blob.AsSpan().StartsWith("v11"u8))
        {
            // v20 is Chrome's app-bound encryption, which only Chrome itself can undo.
            return blob.AsSpan().StartsWith("v20"u8) ? null : Encoding.UTF8.GetString(unprotect(blob));
        }
        if (key is null || blob.Length < 3 + NonceSize + TagSize) return null;
        var plain = DecryptGcm(blob.AsSpan(3), key);
        var text = hostHash && plain.Length >= HostHashSize ? plain.AsSpan(HostHashSize) : plain;
        return Encoding.UTF8.GetString(text);
    }

    private static byte[] DecryptGcm(ReadOnlySpan<byte> payload, byte[] key)
    {
        var nonce = payload[..NonceSize];
        var cipher = payload[NonceSize..^TagSize];
        var plain = new byte[cipher.Length];
        using var aes = new AesGcm(key, TagSize);
        aes.Decrypt(nonce, cipher, payload[^TagSize..], plain);
        return plain;
    }
}
