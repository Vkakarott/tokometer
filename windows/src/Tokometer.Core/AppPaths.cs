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
    string BackendConfig)
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
            BackendConfig: Variable("TOKESP_CONFIG") ?? Path.Combine(home, ".config", "tokesp", "config.sh"));
    }

    private static string? Variable(string name)
    {
        var value = Environment.GetEnvironmentVariable(name);
        return string.IsNullOrWhiteSpace(value) ? null : value;
    }
}
