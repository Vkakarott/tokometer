using System.Text.Json;
using System.Text.Json.Serialization;

namespace Tokometer.Core;

/// <summary>User choices, saved as JSON next to the app's other state.</summary>
public sealed record Preferences
{
    private static readonly JsonSerializerOptions Options = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true,
        Converters = { new JsonStringEnumConverter(JsonNamingPolicy.CamelCase) },
    };

    public HashSet<ProviderId> VisibleProviders { get; init; } = Enum.GetValues<ProviderId>().ToHashSet();

    public bool StartWithWindows { get; init; } = true;

    public bool IsVisible(ProviderId id) => VisibleProviders.Contains(id);

    public Preferences WithVisibility(ProviderId id, bool visible)
    {
        var providers = VisibleProviders.ToHashSet();
        if (visible) providers.Add(id);
        else providers.Remove(id);
        return this with { VisibleProviders = providers };
    }

    public static Preferences Load(string file)
    {
        try
        {
            return JsonSerializer.Deserialize<Preferences>(File.ReadAllText(file), Options) ?? new Preferences();
        }
        catch (Exception error) when (error is IOException or JsonException or UnauthorizedAccessException)
        {
            return new Preferences();
        }
    }

    public void Save(string file)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(file)!);
        File.WriteAllText(file, JsonSerializer.Serialize(this, Options));
    }
}
