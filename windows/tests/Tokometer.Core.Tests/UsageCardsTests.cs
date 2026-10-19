namespace Tokometer.Core.Tests;

public class UsageCardsTests
{
    private static readonly UsageWindow FiveHour = new("five_hour", "5 horas", 0.42, Times.Now.AddMinutes(134));

    [Fact]
    public void Hidden_providers_get_no_card()
    {
        var snapshots = Enum.GetValues<ProviderId>().Select(id => ProviderSnapshot.Fresh(id, [FiveHour], Times.Now));
        var preferences = new Preferences().WithVisibility(ProviderId.Cursor, false);

        var cards = UsageCards.Build(snapshots, preferences, Times.Now);

        Assert.Equal([ProviderId.Claude, ProviderId.Codex], cards.Select(c => c.Id));
    }

    [Fact]
    public void Row_shows_percentage_fill_and_countdown()
    {
        var card = UsageCards.Card(ProviderSnapshot.Fresh(ProviderId.Codex, [FiveHour], Times.Now), Times.Now);

        Assert.Equal(new WindowRow("5 horas", "42%", 0.42, true, "2h14"), card.Rows.Single());
        Assert.False(card.IsProblem);
    }

    [Fact]
    public void Expired_window_shows_an_unknown_value()
    {
        var card = UsageCards.Card(ProviderSnapshot.Fresh(ProviderId.Codex, [FiveHour], Times.Now), Times.Now.AddHours(3));

        Assert.Equal("--%", card.Rows.Single().Percentage);
        Assert.False(card.Rows.Single().Known);
    }

    [Fact]
    public void Tooltip_fits_the_windows_limit()
    {
        var cards = Enum.GetValues<ProviderId>()
            .Select(id => UsageCards.Card(ProviderSnapshot.Fresh(id, [FiveHour], Times.Now), Times.Now)).ToList();

        var tooltip = UsageCards.Tooltip(cards);

        Assert.StartsWith("Tokometer\nClaude: 42% (5 horas)", tooltip);
        Assert.True(tooltip.Length <= 127);
    }
}
