using System.Net;
using Tokometer.Core.Providers;

namespace Tokometer.Core.Tests;

public sealed class ClaudeProviderTests : IDisposable
{
    private const string Usage = """
        {"five_hour":{"utilization":40,"resets_at":"2026-09-22T21:30:00+00:00"},
         "seven_day":{"utilization":19,"resets_at":"2026-09-27T00:00:00+00:00"}}
        """;

    private readonly TempDirectory _temp = new();
    private readonly FakeHandler _handler = new();
    private string? _token = "token-1";

    public void Dispose() => _temp.Dispose();

    private ClaudePollGate Gate => new(_temp.File("gate.json"));

    private ClaudeProvider Provider() => new(
        new HttpClient(_handler), () => _token, new ClaudeReadingFile(_temp.File("claude-poll.json"), Fidelity.Derived), Gate);

    [Fact]
    public async Task Polls_and_then_serves_the_saved_reading_until_the_interval_ends()
    {
        _handler.Respond(HttpStatusCode.OK, Usage);

        var first = await Provider().FetchAsync(Times.Now, default);
        var second = await Provider().FetchAsync(Times.Now.AddMinutes(5), default);

        Assert.Single(_handler.Requests);
        Assert.Equal(StatusKind.Ok, second.Status);
        Assert.Equal(first.Windows, second.Windows);
    }

    [Fact]
    public async Task Rate_limit_backs_off_at_least_five_minutes_even_with_retry_after_zero()
    {
        _handler.Respond(HttpStatusCode.TooManyRequests, configure: r => r.Headers.Add("Retry-After", "0"));

        var error = await Assert.ThrowsAsync<ProviderFetchException>(() => Provider().FetchAsync(Times.Now, default));

        Assert.Equal(ClaudePollGate.MinimumBackoff, error.RetryAfter);
        Assert.False(Gate.IsOpen(Times.Now.AddMinutes(4), []));
        Assert.True(Gate.IsOpen(Times.Now.AddMinutes(5), []));
    }

    [Fact]
    public async Task Rejected_token_is_not_retried_until_it_changes()
    {
        _handler.Respond(HttpStatusCode.Unauthorized).Respond(HttpStatusCode.OK, Usage);

        await Assert.ThrowsAsync<ProviderFetchException>(() => Provider().FetchAsync(Times.Now, default));
        await Assert.ThrowsAsync<ProviderFetchException>(() => Provider().FetchAsync(Times.Now, default));
        _token = "token-2";
        var snapshot = await Provider().FetchAsync(Times.Now, default);

        Assert.Equal(2, _handler.Requests.Count);
        Assert.True(snapshot.HasData);
    }

    [Fact]
    public async Task Missing_login_asks_to_sign_in()
    {
        _token = null;

        var error = await Assert.ThrowsAsync<ProviderFetchException>(() => Provider().FetchAsync(Times.Now, default));

        Assert.Equal(FetchFailure.NeedsAuth, error.Failure);
        Assert.Empty(_handler.Requests);
    }

    [Fact]
    public void Near_the_five_hour_limit_polls_every_minute()
    {
        var nearLimit = new UsageWindow("five_hour", "5 horas", 0.92, Times.Now.AddHours(1));

        Assert.Equal(ClaudePollGate.NearLimitInterval, ClaudePollGate.IntervalFor([nearLimit]));
        Assert.Equal(ClaudePollGate.StandardInterval, ClaudePollGate.IntervalFor([nearLimit with { UsedFraction = 0.5 }]));
    }
}
