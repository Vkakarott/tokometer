using System.IO;
using System.Windows;
using Tokometer.Core;
using Tokometer.Core.Providers;

namespace Tokometer.App;

public partial class App : Application
{
    // One tray icon per user session; a second launch only exits.
    private Mutex? _instance;
    private readonly CancellationTokenSource _shutdown = new();
    private TrayIcon? _tray;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        _instance = new Mutex(initiallyOwned: true, @"Local\Tokometer", out var isFirst);
        if (!isFirst)
        {
            Shutdown();
            return;
        }
        var paths = AppPaths.FromEnvironment();
        var settingsFile = Path.Combine(paths.StateDirectory, "settings.json");
        var preferences = Preferences.Load(settingsFile);
        StartupRegistration.Apply(preferences.StartWithWindows);
        var http = new HttpClient { Timeout = TimeSpan.FromSeconds(10) };
        var publisher = new BackendPublisher(http, () => BackendConfig.Load(paths.BackendConfig));
        var monitor = new UsageMonitor(CreateProviders(http, paths), publisher, TimeProvider.System);
        _tray = new TrayIcon(monitor, publisher, preferences, settingsFile, () => _shutdown.Cancel());
        _ = monitor.RunAsync(_shutdown.Token).ContinueWith(_ => Dispatcher.Invoke(Shutdown));
        _ = publisher.PublishProviderSelectionAsync(preferences.VisibleProviders, _shutdown.Token);
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _tray?.Dispose();
        _instance?.Dispose();
        base.OnExit(e);
    }

    private static IReadOnlyList<IUsageProvider> CreateProviders(HttpClient http, AppPaths paths) =>
    [
        new ClaudeProvider(
            http,
            () => ClaudeProvider.ReadCredentialsToken(paths.ClaudeCredentials),
            new ClaudeReadingFile(Path.Combine(paths.StateDirectory, "claude-poll.json"), Fidelity.Derived),
            new ClaudePollGate(Path.Combine(paths.StateDirectory, "claude-gate.json"))),
        new CodexProvider(http, paths.CodexAuth),
        new CursorProvider(http, paths.CursorState),
    ];
}
