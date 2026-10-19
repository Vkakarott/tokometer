using System.Globalization;
using System.Text.Json;

namespace Tokometer.Core;

/// <summary>Lenient reads: a missing or mistyped field is null, never an exception.</summary>
internal static class Json
{
    public static string? String(JsonElement parent, string name) =>
        Property(parent, name) is { ValueKind: JsonValueKind.String } value ? value.GetString() : null;

    public static double? Number(JsonElement parent, string name) =>
        Property(parent, name) is { ValueKind: JsonValueKind.Number } value ? value.GetDouble() : null;

    public static bool? Bool(JsonElement parent, string name) =>
        Property(parent, name) is { ValueKind: JsonValueKind.True or JsonValueKind.False } value
            ? value.GetBoolean()
            : null;

    public static JsonElement? Property(JsonElement parent, string name) =>
        parent.ValueKind == JsonValueKind.Object && parent.TryGetProperty(name, out var value) ? value : null;

    /// <summary>ISO 8601 with or without fractional seconds, as Claude and Cursor send it.</summary>
    public static DateTimeOffset? Date(string? value) =>
        DateTimeOffset.TryParse(value, CultureInfo.InvariantCulture, DateTimeStyles.AssumeUniversal, out var date)
            ? date
            : null;
}
