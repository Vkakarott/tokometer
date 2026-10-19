using System.Windows;
using System.Windows.Controls;
using Tokometer.Core;

namespace Tokometer.App;

/// <summary>Every change applies at once; there is no save button to forget.</summary>
public partial class SettingsWindow : Window
{
    private readonly Action<Preferences> _apply;
    private Preferences _preferences;

    public SettingsWindow(Preferences preferences, Action<Preferences> apply)
    {
        InitializeComponent();
        _preferences = preferences;
        _apply = apply;
        foreach (var id in Enum.GetValues<ProviderId>()) ProviderList.Children.Add(ProviderBox(id));
        StartWithWindows.IsChecked = preferences.StartWithWindows;
        StartWithWindows.Click += (_, _) => Update(_preferences with { StartWithWindows = StartWithWindows.IsChecked == true });
    }

    private CheckBox ProviderBox(ProviderId id)
    {
        var box = new CheckBox { Content = id.DisplayName(), IsChecked = _preferences.IsVisible(id), Margin = new Thickness(0, 2, 0, 2) };
        box.Click += (_, _) => Update(_preferences.WithVisibility(id, box.IsChecked == true));
        return box;
    }

    private void Update(Preferences preferences)
    {
        _preferences = preferences;
        _apply(preferences);
    }
}
