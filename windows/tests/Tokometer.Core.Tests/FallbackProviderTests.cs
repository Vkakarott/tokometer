using Tokometer.Core.Providers;

namespace Tokometer.Core.Tests;

public class FallbackProviderTests
{
    private sealed class FixedProvider(Func<ProviderSnapshot> fetch) : IUsageProvider
    {
        public ProviderId Id => ProviderId.Claude;
        public TimeSpan RefreshInterval => TimeSpan.FromMinutes(5);

        public Task<ProviderSnapshot> FetchAsync(DateTimeOffset now, CancellationToken cancellation) => Task.FromResult(fetch());
    }

    private static readonly UsageWindow FiveHour = new("five_hour", "5 horas", 0.3, Times.Now.AddHours(2));
    private static readonly FixedProvider Works = new(() => ProviderSnapshot.Fresh(ProviderId.Claude, [FiveHour], Times.Now));

    private static FixedProvider Fails(ProviderFetchException error) => new(() => throw error);

    [Fact]
    public async Task Without_the_first_login_reads_the_second()
    {
        var provider = new FallbackProvider(Fails(ProviderFetchException.SignedOut("Entre no app Claude")), Works);

        Assert.True((await provider.FetchAsync(Times.Now, default)).HasData);
    }

    [Fact]
    public async Task Is_signed_out_only_when_both_are()
    {
        var provider = new FallbackProvider(Fails(ProviderFetchException.SignedOut("app")), Fails(ProviderFetchException.SignedOut("code")));

        var error = await Assert.ThrowsAsync<ProviderFetchException>(() => provider.FetchAsync(Times.Now, default));

        Assert.Equal(FetchFailure.SignedOut, error.Failure);
    }

    [Fact]
    public async Task A_rejected_first_login_is_what_the_user_sees()
    {
        var provider = new FallbackProvider(
            Fails(ProviderFetchException.NeedsAuth("Entre novamente no app Claude")),
            Fails(ProviderFetchException.SignedOut("Entre no Claude Code")));

        var error = await Assert.ThrowsAsync<ProviderFetchException>(() => provider.FetchAsync(Times.Now, default));

        Assert.Equal("Entre novamente no app Claude", error.Message);
    }

    [Fact]
    public async Task Without_the_second_login_the_first_failure_is_kept()
    {
        var provider = new FallbackProvider(
            Fails(ProviderFetchException.Unavailable("claude.ai indisponível (HTTP 503)")),
            Fails(ProviderFetchException.SignedOut("Entre no Claude Code")));

        var error = await Assert.ThrowsAsync<ProviderFetchException>(() => provider.FetchAsync(Times.Now, default));

        Assert.Equal(FetchFailure.Unavailable, error.Failure);
    }
}
