using System.Windows.Threading;
using Tokometer.Core;
using Forms = System.Windows.Forms;

namespace Tokometer.App;

/// <summary>The tray icon, its menu, and the panel and settings it opens.</summary>
internal sealed class TrayIcon : IDisposable
{
    private readonly UsageMonitor _monitor;
    private readonly BackendPublisher _publisher;
    private readonly string _settingsFile;
    private readonly Forms.NotifyIcon _icon;
    private readonly UsagePanel _panel = new();
    // Keeps the reset countdowns moving between two readings.
    private readonly DispatcherTimer _clock = new() { Interval = TimeSpan.FromSeconds(30) };
    private Preferences _preferences;
    private SettingsWindow? _settings;

    public TrayIcon(UsageMonitor monitor, BackendPublisher publisher, Preferences preferences, string settingsFile, Action quit)
    {
        _monitor = monitor;
        _publisher = publisher;
        _preferences = preferences;
        _settingsFile = settingsFile;
        _icon = new Forms.NotifyIcon { Visible = true, ContextMenuStrip = BuildMenu(quit) };
        _icon.MouseClick += (_, e) =>
        {
            if (e.Button == Forms.MouseButtons.Left) TogglePanel();
        };
        _monitor.Changed += () => _panel.Dispatcher.BeginInvoke(Render);
        _clock.Tick += (_, _) => Render();
        _clock.Start();
        Render();
    }

    private Forms.ContextMenuStrip BuildMenu(Action quit)
    {
        var menu = new Forms.ContextMenuStrip();
        menu.Items.Add("Mostrar consumo", null, (_, _) => TogglePanel());
        menu.Items.Add("Atualizar agora", null, (_, _) => _ = _monitor.RefreshAsync(force: true, CancellationToken.None));
        menu.Items.Add("Ajustes…", null, (_, _) => OpenSettings());
        menu.Items.Add(new Forms.ToolStripSeparator());
        menu.Items.Add("Sair", null, (_, _) => quit());
        return menu;
    }

    private void Render()
    {
        var now = DateTimeOffset.Now;
        var cards = UsageCards.Build(_monitor.Snapshots, _preferences, now);
        _panel.Show(cards);
        _icon.Text = UsageCards.Tooltip(cards);
        TrayArt.Apply(_icon, cards);
    }

    private void TogglePanel()
    {
        if (_panel.IsVisible) _panel.Hide();
        else _panel.Open();
    }

    private void OpenSettings()
    {
        if (_settings is not null)
        {
            _settings.Activate();
            return;
        }
        _settings = new SettingsWindow(_preferences, ApplyPreferences);
        _settings.Closed += (_, _) => _settings = null;
        _settings.Show();
    }

    private void ApplyPreferences(Preferences preferences)
    {
        _preferences = preferences;
        preferences.Save(_settingsFile);
        StartupRegistration.Apply(preferences.StartWithWindows);
        _ = _publisher.PublishProviderSelectionAsync(preferences.VisibleProviders, CancellationToken.None);
        Render();
    }

    public void Dispose()
    {
        _clock.Stop();
        _icon.Visible = false;
        _icon.Icon?.Dispose();
        _icon.Dispose();
    }
}
