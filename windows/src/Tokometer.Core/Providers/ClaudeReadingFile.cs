using System.Text.Json;

namespace Tokometer.Core.Providers;

public sealed record ClaudeReading(IReadOnlyList<UsageWindow> Windows, DateTimeOffset ObservedAt, Fidelity Fidelity)
{
    public bool IsFresh(DateTimeOffset now) => now - ObservedAt < ClaudePollGate.IntervalFor(Windows);

    public bool HasSubscriptionWindows =>
        Windows.Any(w => w.Id == "five_hour") && Windows.Any(w => w.Id == "seven_day");
}

/// <summary>
/// A saved Claude reading, in the shape the macOS collectors use, so the last
/// poll survives a restart without spending another request.
/// </summary>
public sealed class ClaudeReadingFile(string path, Fidelity fidelity)
{
    private sealed record StoredWindow(string Id, double? UsedPercentage, double? ResetsAt);

    private sealed record Stored(double ObservedAt, List<StoredWindow> Windows);

    private static readonly JsonSerializerOptions Options = new(JsonSerializerDefaults.Web);

    public ClaudeReading? Read()
    {
        try
        {
            var stored = JsonSerializer.Deserialize<Stored>(File.ReadAllText(path), Options);
            if (stored is null || stored.Windows.Count == 0) return null;
            var windows = stored.Windows.Select(ToWindow).ToList();
            return new ClaudeReading(windows, DateTimeOffset.FromUnixTimeSeconds((long)stored.ObservedAt), fidelity);
        }
        catch (Exception error) when (error is IOException or JsonException or UnauthorizedAccessException)
        {
            return null;
        }
    }

    public void Write(ClaudeReading reading)
    {
        var stored = new Stored(
            reading.ObservedAt.ToUnixTimeSeconds(),
            reading.Windows.Select(w => new StoredWindow(w.Id, w.UsedFraction * 100, w.ResetsAt?.ToUnixTimeSeconds())).ToList());
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var temporary = path + ".tmp";
        File.WriteAllText(temporary, JsonSerializer.Serialize(stored, Options));
        File.Move(temporary, path, overwrite: true);
    }

    private static UsageWindow ToWindow(StoredWindow stored) => new(
        stored.Id,
        UsageWindow.DefaultLabel(stored.Id),
        stored.UsedPercentage is { } percentage ? UsageWindow.Clamp(percentage / 100) : null,
        stored.ResetsAt is { } reset ? DateTimeOffset.FromUnixTimeSeconds((long)reset) : null);
}
