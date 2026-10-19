namespace Tokometer.Core.Tests;

public class FormatAndPreferencesTests
{
    private static UsageWindow Window(double? fraction, TimeSpan? resetsIn) =>
        new("five_hour", "5 horas", fraction, resetsIn is { } left ? Times.Now + left : null);

    [Theory]
    [InlineData(0.426, "43%")]
    [InlineData(0.995, "100%")]
    [InlineData(0.0, "0%")]
    [InlineData(null, "--%")]
    public void Percentage_never_shows_an_unknown_value_as_zero(double? fraction, string expected)
    {
        Assert.Equal(expected, UsageFormat.Percentage(Window(fraction, null)));
    }

    [Theory]
    [InlineData(134, "2h14")]
    [InlineData(5 * 24 * 60 + 12 * 60, "5d12h")]
    [InlineData(45, "45 min")]
    [InlineData(0, "Reiniciado")]
    public void Reset_countdown_is_compact(int minutes, string expected)
    {
        Assert.Equal(expected, UsageFormat.ResetsIn(Window(0.5, TimeSpan.FromMinutes(minutes)), Times.Now));
    }

    [Fact]
    public void A_window_past_its_reset_has_an_unknown_value()
    {
        var expired = UsageFormat.Current(Window(0.9, TimeSpan.FromMinutes(-1)), Times.Now);

        Assert.Null(expired.UsedFraction);
    }

    [Fact]
    public void Stale_status_says_why_and_how_old()
    {
        var snapshot = ProviderSnapshot.Fresh(ProviderId.Codex, [Window(0.5, null)], Times.Now.AddMinutes(-20))
            with { Status = StatusKind.Stale, Message = "Sem conexão com o serviço" };

        Assert.Equal("Sem conexão com o serviço · dados de há 20 min", UsageFormat.Status(snapshot, Times.Now));
    }

    [Fact]
    public void Preferences_round_trip_and_default_to_every_provider()
    {
        using var temp = new TempDirectory();
        var file = temp.File("settings.json");

        Assert.Equal(3, Preferences.Load(file).VisibleProviders.Count);
        new Preferences().WithVisibility(ProviderId.Cursor, false).Save(file);
        var loaded = Preferences.Load(file);

        Assert.False(loaded.IsVisible(ProviderId.Cursor));
        Assert.True(loaded.IsVisible(ProviderId.Claude));
        Assert.True(loaded.StartWithWindows);
    }
}
