namespace Tokometer.Core;

public sealed record WindowRow(string Label, string Percentage, double Fill, bool Known, string ResetsIn);

public sealed record ProviderCard(ProviderId Id, string Name, string Status, bool IsProblem, IReadOnlyList<WindowRow> Rows);

/// <summary>Turns snapshots into what the tray panel shows, so the WPF side only lays it out.</summary>
public static class UsageCards
{
    public static IReadOnlyList<ProviderCard> Build(IEnumerable<ProviderSnapshot> snapshots, Preferences preferences,
        DateTimeOffset now) =>
        snapshots.Where(s => preferences.IsVisible(s.Id)).Select(s => Card(s, now)).ToList();

    public static ProviderCard Card(ProviderSnapshot snapshot, DateTimeOffset now) => new(
        snapshot.Id,
        snapshot.Id.DisplayName(),
        UsageFormat.Status(snapshot, now),
        snapshot.Status != StatusKind.Ok,
        snapshot.Windows.Select(window => Row(UsageFormat.Current(window, now), now)).ToList());

    /// <summary>The tray tooltip: one line per visible provider, with its first window.</summary>
    public static string Tooltip(IEnumerable<ProviderCard> cards)
    {
        var lines = cards.Select(card => card.Rows.Count > 0
            ? $"{card.Name}: {card.Rows[0].Percentage} ({card.Rows[0].Label})"
            : $"{card.Name}: sem dados");
        var text = string.Join("\n", lines.Prepend("Tokometer"));
        // Windows truncates tray tooltips past 127 characters.
        return text.Length <= 127 ? text : text[..127];
    }

    private static WindowRow Row(UsageWindow window, DateTimeOffset now) => new(
        window.Label,
        UsageFormat.Percentage(window),
        window.UsedFraction ?? 0,
        window.UsedFraction is not null,
        UsageFormat.ResetsIn(window, now));
}
