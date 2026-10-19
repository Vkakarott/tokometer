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
        DispatcherUnhandledException += (_, args) =>
        {
            args.Handled = true;
            Fail(args.Exception);
        };
        AppDomain.CurrentDomain.UnhandledException += (_, args) => CrashReport.Show(args.ExceptionObject as Exception);
        _instance = new Mutex(initiallyOwned: true, @"Local\Tokometer", out var isFirst);
        if (!isFirst)
        {
            MessageBox.Show(
                "O Tokometer já está aberto.\n\nO ícone fica na bandeja, perto do relógio. Se não aparecer, clique na seta ^ ao lado dele.",
                "Tokometer", MessageBoxButton.OK, MessageBoxImage.Information);
            Shutdown();
            return;
        }
        try
        {
            Start();
        }
        catch (Exception error)
        {
            Fail(error);
        }
    }

    private void Start()
    {
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
        _tray.Announce();
    }

    private void Fail(Exception error)
    {
        CrashReport.Show(error);
        _shutdown.Cancel();
        Shutdown(1);
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _tray?.Dispose();
        _instance?.Dispose();
        base.OnExit(e);
    }

    private static IReadOnlyList<IUsageProvider> CreateProviders(HttpClient http, AppPaths paths) =>
    [
        new FallbackProvider(ClaudeApp(http, paths), ClaudeCode(http, paths)),
        new CodexProvider(http, paths.CodexAuth),
        new CursorProvider(http, paths.CursorState),
    ];

    // Most people use the Claude app; Claude Code is the fallback.
    private static ClaudeWebProvider ClaudeApp(HttpClient http, AppPaths paths)
    {
        var login = new ClaudeAppLogin(paths.ClaudeAppProfiles, Dpapi.Unprotect, Dpapi.Protect,
            Path.Combine(paths.StateDirectory, "claude-app-login.bin"));
        return new ClaudeWebProvider(http, login.Read, login.Forget);
    }

    private static ClaudeProvider ClaudeCode(HttpClient http, AppPaths paths) => new(
        http,
        () => ClaudeProvider.ReadCredentialsToken(paths.ClaudeCredentials),
        new ClaudeReadingFile(Path.Combine(paths.StateDirectory, "claude-poll.json"), Fidelity.Derived),
        new ClaudePollGate(Path.Combine(paths.StateDirectory, "claude-gate.json")));
}
