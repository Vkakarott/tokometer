namespace Tokometer.Core;

public enum FetchFailure
{
    NeedsAuth,
    Unsupported,
    Unavailable,
    RateLimited,
}

/// <summary>A provider could not be read. The message is shown to the user, in pt-BR.</summary>
public sealed class ProviderFetchException(FetchFailure failure, string message, TimeSpan? retryAfter = null)
    : Exception(message)
{
    public FetchFailure Failure { get; } = failure;

    /// <summary>Set only for <see cref="FetchFailure.RateLimited"/>.</summary>
    public TimeSpan? RetryAfter { get; } = retryAfter;

    public static ProviderFetchException NeedsAuth(string message) => new(FetchFailure.NeedsAuth, message);

    public static ProviderFetchException Unsupported(string message) => new(FetchFailure.Unsupported, message);

    public static ProviderFetchException Unavailable(string message) => new(FetchFailure.Unavailable, message);

    public static ProviderFetchException RateLimited(TimeSpan wait) =>
        new(FetchFailure.RateLimited, $"Atualização limitada; nova tentativa em {(int)wait.TotalMinutes} min", wait);
}

public interface IUsageProvider
{
    ProviderId Id { get; }

    /// <summary>Minimum time between two reads of this provider.</summary>
    TimeSpan RefreshInterval { get; }

    Task<ProviderSnapshot> FetchAsync(DateTimeOffset now, CancellationToken cancellation);
}
