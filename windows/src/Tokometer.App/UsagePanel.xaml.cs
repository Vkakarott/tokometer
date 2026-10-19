using System.Windows;
using System.Windows.Media;
using Tokometer.Core;

namespace Tokometer.App;

/// <summary>What one card binds to: the card plus its colours.</summary>
public sealed record CardView(ProviderCard Card, Brush Accent, Brush StatusBrush);

/// <summary>The usage panel that opens above the tray, like the system flyouts.</summary>
public partial class UsagePanel : Window
{
    private static readonly Brush Problem = Frozen(0xEF, 0x44, 0x44);

    public UsagePanel()
    {
        InitializeComponent();
        Deactivated += (_, _) => Hide();
        SizeChanged += (_, _) => PlaceAboveTray();
    }

    public void Show(IReadOnlyList<ProviderCard> cards)
    {
        Cards.ItemsSource = cards.Select(View).ToList();
        EmptyText.Visibility = cards.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
        UpdatedText.Text = $"Atualizado às {DateTime.Now:HH:mm}";
    }

    public void Open()
    {
        Show();
        PlaceAboveTray();
        Activate();
    }

    private void PlaceAboveTray()
    {
        var area = SystemParameters.WorkArea;
        Left = area.Right - ActualWidth - 12;
        Top = area.Bottom - ActualHeight - 12;
    }

    private CardView View(ProviderCard card)
    {
        var accent = card.IsProblem && card.Rows.Count == 0 ? Problem : AccentFor(card.Id);
        var status = card.IsProblem ? (Brush)FindResource("Warning") : (Brush)FindResource("Muted");
        return new CardView(card, accent, status);
    }

    private static Brush AccentFor(ProviderId id) => id switch
    {
        ProviderId.Claude => Frozen(0xED, 0x57, 0x1F),
        ProviderId.Codex => Frozen(0x2E, 0xD1, 0x6E),
        _ => Frozen(0xF2, 0xF4, 0xF7),
    };

    private static Brush Frozen(byte r, byte g, byte b)
    {
        var brush = new SolidColorBrush(Color.FromRgb(r, g, b));
        brush.Freeze();
        return brush;
    }
}
