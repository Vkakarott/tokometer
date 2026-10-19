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
    public void Providers_without_login_or_not_read_yet_get_no_card()
    {
        ProviderSnapshot[] snapshots =
        [
            ProviderSnapshot.Fresh(ProviderId.Claude, [FiveHour], Times.Now),
            new(ProviderId.Codex, StatusKind.SignedOut, "Entre no Codex CLI", [], Fidelity.Official, null),
            ProviderSnapshot.Waiting(ProviderId.Cursor),
        ];

        var cards = UsageCards.Build(snapshots, new Preferences(), Times.Now);

        Assert.Equal([ProviderId.Claude], cards.Select(c => c.Id));
    }

    [Fact]
    public void A_rejected_login_still_gets_a_card()
    {
        var rejected = new ProviderSnapshot(ProviderId.Codex, StatusKind.Error, "Entre novamente no Codex", [], Fidelity.Official, null);

        var card = UsageCards.Build([rejected], new Preferences(), Times.Now).Single();

        Assert.True(card.IsProblem);
        Assert.Equal("Entre novamente no Codex", card.Status);
    }

    [Fact]
    public void Empty_panel_says_why()
    {
        var signedOut = Enum.GetValues<ProviderId>()
            .Select(id => new ProviderSnapshot(id, StatusKind.SignedOut, null, [], Fidelity.Official, null)).ToList();
        var waiting = Enum.GetValues<ProviderId>().Select(ProviderSnapshot.Waiting).ToList();
        var nothingVisible = Enum.GetValues<ProviderId>().Aggregate(new Preferences(), (p, id) => p.WithVisibility(id, false));

        Assert.StartsWith("Nenhum login encontrado", UsageCards.EmptyMessage(signedOut, new Preferences()));
        Assert.StartsWith("Procurando", UsageCards.EmptyMessage(waiting, new Preferences()));
        Assert.StartsWith("Nenhum provedor visível", UsageCards.EmptyMessage(signedOut, nothingVisible));
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
