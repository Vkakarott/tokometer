using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace Tokometer.Core.Providers;

/// <summary>
/// Keeps the undocumented usage route from being hammered: one poll per
/// interval, a wait after a rate limit, and no retry with a token the server
/// already rejected. Saved to disk, so restarting the app does not reset it.
/// </summary>
public sealed class ClaudePollGate(string stateFile)
{
    public static readonly TimeSpan StandardInterval = TimeSpan.FromMinutes(15);
    public static readonly TimeSpan NearLimitInterval = TimeSpan.FromMinutes(1);
    public const double NearLimitThreshold = 0.9;
    public static readonly TimeSpan MinimumBackoff = TimeSpan.FromMinutes(5);
    public static readonly TimeSpan MaximumBackoff = TimeSpan.FromHours(1);

    private sealed record State(long LastPoll = 0, long NextPoll = 0, long BackoffUntil = 0, string? RejectedToken = null);

    public static TimeSpan IntervalFor(IEnumerable<UsageWindow> windows) =>
        windows.Any(w => w.Id == "five_hour" && (w.UsedFraction ?? 0) >= NearLimitThreshold)
            ? NearLimitInterval
            : StandardInterval;

    public bool IsOpen(DateTimeOffset now, IReadOnlyList<UsageWindow> knownWindows)
    {
        var state = Load();
        var seconds = now.ToUnixTimeSeconds();
        if (seconds < state.BackoffUntil) return false;
        if (IntervalFor(knownWindows) == NearLimitInterval)
            return seconds >= state.LastPoll + (long)NearLimitInterval.TotalSeconds;
        return seconds >= state.NextPoll;
    }

    public bool Rejects(string tokenId) => Load().RejectedToken == tokenId;

    public void RecordSuccess(DateTimeOffset now, IReadOnlyList<UsageWindow> windows) =>
        Save(new State(now.ToUnixTimeSeconds(), (now + IntervalFor(windows)).ToUnixTimeSeconds()));

    /// <summary>
    /// Anthropic answers with <c>Retry-After: 0</c> and keeps refusing, so a
    /// short wait is never honoured as is. Returns the wait actually applied.
    /// </summary>
    public TimeSpan RecordRateLimit(DateTimeOffset now, TimeSpan? retryAfter)
    {
        var wait = TimeSpan.FromTicks(Math.Clamp((retryAfter ?? TimeSpan.Zero).Ticks, MinimumBackoff.Ticks, MaximumBackoff.Ticks));
        Save(Load() with { BackoffUntil = (now + wait).ToUnixTimeSeconds() });
        return wait;
    }

    public void Reject(string tokenId) => Save(Load() with { RejectedToken = tokenId });

    /// <summary>Identifies a token without keeping it around.</summary>
    public static string TokenId(string token) =>
        Convert.ToHexStringLower(SHA256.HashData(Encoding.UTF8.GetBytes(token)));

    private State Load()
    {
        try
        {
            return JsonSerializer.Deserialize<State>(File.ReadAllText(stateFile)) ?? new State();
        }
        catch (Exception error) when (error is IOException or JsonException or UnauthorizedAccessException)
        {
            return new State();
        }
    }

    private void Save(State state)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(stateFile)!);
        File.WriteAllText(stateFile, JsonSerializer.Serialize(state));
    }
}
