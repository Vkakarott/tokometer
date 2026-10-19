namespace Tokometer.Core.Providers;

/// <summary>
/// Two ways to read the same provider, such as the Claude app and Claude Code.
/// Uses the second only when the first fails, and reports the failure that
/// says the most: a missing login only when neither has one.
/// </summary>
public sealed class FallbackProvider(IUsageProvider primary, IUsageProvider secondary) : IUsageProvider
{
    public ProviderId Id => primary.Id;

    public TimeSpan RefreshInterval => primary.RefreshInterval;

    public async Task<ProviderSnapshot> FetchAsync(DateTimeOffset now, CancellationToken cancellation)
    {
        try
        {
            return await primary.FetchAsync(now, cancellation);
        }
        catch (ProviderFetchException first)
        {
            try
            {
                return await secondary.FetchAsync(now, cancellation);
            }
            catch (ProviderFetchException second)
            {
                throw first.Failure == FetchFailure.SignedOut ? second : first;
            }
        }
    }
}
