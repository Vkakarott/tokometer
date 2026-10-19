namespace Tokometer.Core;

/// <summary>
/// Polls every provider on its own interval and keeps the latest snapshot of
/// each. A failed read keeps the last good data, marked stale, instead of
/// blanking the provider.
/// </summary>
public sealed class UsageMonitor(IReadOnlyList<IUsageProvider> providers, BackendPublisher? publisher, TimeProvider clock)
{
    private readonly Dictionary<ProviderId, ProviderSnapshot> _snapshots =
        Enum.GetValues<ProviderId>().ToDictionary(id => id, ProviderSnapshot.Waiting);
    private readonly Dictionary<ProviderId, DateTimeOffset> _lastAttempt = [];
    private readonly Dictionary<ProviderId, DateTimeOffset> _retryAfter = [];
    private readonly SemaphoreSlim _refreshing = new(1, 1);

    public static readonly TimeSpan TickInterval = TimeSpan.FromMinutes(1);

    /// <summary>Raised after any snapshot changes, on the thread that refreshed.</summary>
    public event Action? Changed;

    public IReadOnlyList<ProviderSnapshot> Snapshots
    {
        get
        {
            lock (_snapshots) return Enum.GetValues<ProviderId>().Select(id => _snapshots[id]).ToList();
        }
    }

    /// <summary>Reads every provider that is due. <paramref name="force"/> ignores the intervals but not rate limits.</summary>
    public async Task RefreshAsync(bool force, CancellationToken cancellation)
    {
        if (!await _refreshing.WaitAsync(0, cancellation)) return;
        try
        {
            if (publisher is not null) await publisher.FlushProviderSelectionAsync(cancellation);
            foreach (var provider in providers.Where(p => IsDue(p, force)))
            {
                await RefreshOneAsync(provider, cancellation);
            }
        }
        finally
        {
            _refreshing.Release();
        }
    }

    public async Task RunAsync(CancellationToken cancellation)
    {
        using var timer = new PeriodicTimer(TickInterval, clock);
        do
        {
            await RefreshAsync(force: false, cancellation);
        } while (await timer.WaitForNextTickAsync(cancellation));
    }

    private bool IsDue(IUsageProvider provider, bool force)
    {
        var now = clock.GetUtcNow();
        if (_retryAfter.TryGetValue(provider.Id, out var next) && next > now) return false;
        if (force) return true;
        return !_lastAttempt.TryGetValue(provider.Id, out var previous) || now - previous >= provider.RefreshInterval;
    }

    private async Task RefreshOneAsync(IUsageProvider provider, CancellationToken cancellation)
    {
        var now = clock.GetUtcNow();
        _lastAttempt[provider.Id] = now;
        try
        {
            var snapshot = await provider.FetchAsync(now, cancellation);
            Store(snapshot);
            if (publisher is not null) await publisher.PublishAsync(snapshot, cancellation);
        }
        // Anything a provider throws is shown on its card; it must never stop the loop.
        catch (Exception error) when (!cancellation.IsCancellationRequested)
        {
            if (error is ProviderFetchException { RetryAfter: { } wait }) _retryAfter[provider.Id] = now + wait;
            Store(KeepLastGood(provider.Id, Describe(error)));
        }
    }

    private ProviderSnapshot KeepLastGood(ProviderId id, string message)
    {
        ProviderSnapshot previous;
        lock (_snapshots) previous = _snapshots[id];
        return previous.HasData
            ? previous with { Status = StatusKind.Stale, Message = message }
            : previous with { Status = StatusKind.Error, Message = message };
    }

    private static string Describe(Exception error) => error switch
    {
        ProviderFetchException fetch => fetch.Message,
        HttpRequestException or TaskCanceledException => "Sem conexão com o serviço",
        _ => "Resposta inesperada do serviço",
    };

    private void Store(ProviderSnapshot snapshot)
    {
        lock (_snapshots) _snapshots[snapshot.Id] = snapshot;
        Changed?.Invoke();
    }
}
