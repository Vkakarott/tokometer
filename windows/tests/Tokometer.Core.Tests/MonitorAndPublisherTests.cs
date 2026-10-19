using System.Net;
using Microsoft.Extensions.Time.Testing;

namespace Tokometer.Core.Tests;

public class MonitorAndPublisherTests
{
    private sealed class ScriptedProvider(ProviderId id, params Func<DateTimeOffset, ProviderSnapshot>[] steps) : IUsageProvider
    {
        private int _calls;

        public ProviderId Id => id;
        public TimeSpan RefreshInterval => TimeSpan.FromMinutes(5);
        public int Calls => _calls;

        public Task<ProviderSnapshot> FetchAsync(DateTimeOffset now, CancellationToken cancellation) =>
            Task.FromResult(steps[Math.Min(_calls++, steps.Length - 1)](now));
    }

    private static readonly UsageWindow FiveHour = new("five_hour", "5 horas", 0.4, Times.Now.AddHours(2));

    [Fact]
    public async Task Failure_keeps_the_last_good_reading_as_stale()
    {
        var clock = new FakeTimeProvider(Times.Now);
        var provider = new ScriptedProvider(ProviderId.Codex,
            now => ProviderSnapshot.Fresh(ProviderId.Codex, [FiveHour], now),
            _ => throw ProviderFetchException.Unavailable("Codex indisponível (HTTP 503)"));
        var monitor = new UsageMonitor([provider], null, clock);

        await monitor.RefreshAsync(force: false, default);
        clock.Advance(TimeSpan.FromMinutes(5));
        await monitor.RefreshAsync(force: false, default);

        var codex = monitor.Snapshots.Single(s => s.Id == ProviderId.Codex);
        Assert.Equal(StatusKind.Stale, codex.Status);
        Assert.Equal([FiveHour], codex.Windows);
        Assert.Equal("Codex indisponível (HTTP 503)", codex.Message);
    }

    [Fact]
    public async Task Respects_the_refresh_interval_unless_forced()
    {
        var clock = new FakeTimeProvider(Times.Now);
        var provider = new ScriptedProvider(ProviderId.Codex, now => ProviderSnapshot.Fresh(ProviderId.Codex, [FiveHour], now));
        var monitor = new UsageMonitor([provider], null, clock);

        await monitor.RefreshAsync(force: false, default);
        await monitor.RefreshAsync(force: false, default);
        Assert.Equal(1, provider.Calls);
        await monitor.RefreshAsync(force: true, default);
        Assert.Equal(2, provider.Calls);
    }

    [Fact]
    public async Task Rate_limit_blocks_even_a_forced_refresh()
    {
        var clock = new FakeTimeProvider(Times.Now);
        var provider = new ScriptedProvider(ProviderId.Claude, _ => throw ProviderFetchException.RateLimited(TimeSpan.FromMinutes(10)));
        var monitor = new UsageMonitor([provider], null, clock);

        await monitor.RefreshAsync(force: false, default);
        await monitor.RefreshAsync(force: true, default);

        Assert.Equal(1, provider.Calls);
        Assert.Equal(StatusKind.Error, monitor.Snapshots.Single(s => s.Id == ProviderId.Claude).Status);
    }

    [Fact]
    public void Backend_config_reads_quoted_exports_and_ignores_comments()
    {
        var config = BackendConfig.Parse("# tokesp\nexport TOKESP_URL=\"http://pc.local:43110\"\nexport TOKESP_TOKEN='secret'\r\n");

        Assert.Equal(new Uri("http://pc.local:43110"), config?.Url);
        Assert.Equal("secret", config?.Token);
        Assert.Null(BackendConfig.Parse("export TOKESP_URL=http://pc.local"));
    }

    [Fact]
    public async Task Publisher_sends_nothing_without_a_backend_config()
    {
        var handler = new FakeHandler();
        var publisher = new BackendPublisher(new HttpClient(handler), () => null);

        await publisher.PublishAsync(ProviderSnapshot.Fresh(ProviderId.Codex, [FiveHour], Times.Now), default);

        Assert.Empty(handler.Requests);
    }

    [Fact]
    public async Task Publisher_sends_a_reading_once_and_the_selection_in_canonical_order()
    {
        var handler = new FakeHandler().Respond(HttpStatusCode.NoContent).Respond(HttpStatusCode.NoContent);
        var config = new BackendConfig(new Uri("http://pc.local:43110"), "secret");
        var publisher = new BackendPublisher(new HttpClient(handler), () => config);
        var snapshot = ProviderSnapshot.Fresh(ProviderId.Claude, [FiveHour], Times.Now);

        await publisher.PublishAsync(snapshot, default);
        await publisher.PublishAsync(snapshot, default);
        await publisher.PublishProviderSelectionAsync([ProviderId.Cursor, ProviderId.Claude], default);

        Assert.Equal(["http://pc.local:43110/ingest", "http://pc.local:43110/providers"],
            handler.Requests.Select(r => r.RequestUri!.ToString()));
        Assert.Contains("\"provider\":\"claude-code\"", handler.Bodies[0]);
        Assert.Contains("\"usedPercentage\":40", handler.Bodies[0]);
        Assert.Equal("{\"enabled\":[\"claude\",\"cursor\"]}", handler.Bodies[1]);
    }

    [Fact]
    public void Endpoint_keeps_a_path_prefix()
    {
        Assert.Equal("http://pc/api/ingest", BackendPublisher.Endpoint(new Uri("http://pc/api"), "ingest").ToString());
    }
}
