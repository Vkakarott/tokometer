namespace Tokometer.Core;

/// <summary>
/// Where each tool keeps its login on Windows. Every path can be overridden
/// through an environment variable, which the tests and odd setups use.
/// </summary>
public sealed record AppPaths(
    string CodexAuth,
    string ClaudeCredentials,
    string CursorState,
    string StateDirectory,
    string BackendConfig,
    string ClaudeAppDirectory,
    string PackagesDirectory)
{
    public static AppPaths FromEnvironment()
    {
        var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        var roaming = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var codexHome = Variable("CODEX_HOME") ?? Path.Combine(home, ".codex");
        var claudeHome = Variable("CLAUDE_CONFIG_DIR") ?? Path.Combine(home, ".claude");
        return new AppPaths(
            CodexAuth: Path.Combine(codexHome, "auth.json"),
            ClaudeCredentials: Path.Combine(claudeHome, ".credentials.json"),
            CursorState: Path.Combine(roaming, "Cursor", "User", "globalStorage", "state.vscdb"),
            StateDirectory: Variable("TOKOMETER_STATE_DIR") ?? Path.Combine(local, "Tokometer"),
            BackendConfig: Variable("TOKESP_CONFIG") ?? Path.Combine(home, ".config", "tokesp", "config.sh"),
            ClaudeAppDirectory: Variable("CLAUDE_APP_DIR") ?? Path.Combine(roaming, "Claude"),
            PackagesDirectory: Path.Combine(local, "Packages"));
    }

    /// <summary>
    /// Where the Claude desktop app may keep its profile: the installer's
    /// folder, or the Microsoft Store package's virtualized copy of it. Looked
    /// up on every read, so installing the app later needs no restart.
    /// </summary>
    public IEnumerable<string> ClaudeAppProfiles()
    {
        yield return ClaudeAppDirectory;
        if (!Directory.Exists(PackagesDirectory)) yield break;
        foreach (var package in Directory.EnumerateDirectories(PackagesDirectory, "*Claude*"))
        {
            yield return Path.Combine(package, "LocalCache", "Roaming", "Claude");
        }
    }

    private static string? Variable(string name)
    {
        var value = Environment.GetEnvironmentVariable(name);
        return string.IsNullOrWhiteSpace(value) ? null : value;
    }
}
