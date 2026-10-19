namespace Tokometer.Core;

/// <summary>Texts the tray shows, in pt-BR.</summary>
public static class UsageFormat
{
    public static string Percentage(UsageWindow window) =>
        window.UsedFraction is { } fraction ? $"{Math.Round(fraction * 100, MidpointRounding.AwayFromZero):0}%" : "--%";

    /// <summary>"2h14", "3d12h", "45 min"; empty when the reset time is unknown.</summary>
    public static string ResetsIn(UsageWindow window, DateTimeOffset now)
    {
        if (window.ResetsAt is not { } reset) return "";
        var left = reset - now;
        if (left <= TimeSpan.Zero) return "Reiniciado";
        if (left.TotalDays >= 1) return $"{(int)left.TotalDays}d{left.Hours:00}h";
        if (left.TotalHours >= 1) return $"{(int)left.TotalHours}h{left.Minutes:00}";
        return $"{Math.Max(1, (int)left.TotalMinutes)} min";
    }

    /// <summary>A window past its reset holds a value for a period that no longer exists.</summary>
    public static UsageWindow Current(UsageWindow window, DateTimeOffset now) =>
        window.ResetsAt is { } reset && reset <= now ? window with { UsedFraction = null } : window;

    public static string Status(ProviderSnapshot snapshot, DateTimeOffset now) => snapshot.Status switch
    {
        StatusKind.Ok => $"Atualizado {Age(snapshot.ObservedAt, now)}",
        StatusKind.Stale => snapshot.Message is { } message
            ? $"{message} · dados de {Age(snapshot.ObservedAt, now)}"
            : $"Dados de {Age(snapshot.ObservedAt, now)}",
        _ => snapshot.Message ?? "Sem dados",
    };

    public static string Age(DateTimeOffset? observedAt, DateTimeOffset now)
    {
        if (observedAt is not { } observed) return "";
        var age = now - observed;
        if (age < TimeSpan.FromMinutes(1)) return "agora";
        if (age < TimeSpan.FromHours(1)) return $"há {(int)age.TotalMinutes} min";
        if (age < TimeSpan.FromDays(1)) return $"há {(int)age.TotalHours} h";
        return $"há {(int)age.TotalDays} d";
    }
}
