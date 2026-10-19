namespace Tokometer.Core;

/// <summary>Providers in the order the app lists them, matching the macOS app.</summary>
public enum ProviderId
{
    Claude,
    Codex,
    Cursor,
}

public static class ProviderNames
{
    public static string DisplayName(this ProviderId id) => id switch
    {
        ProviderId.Claude => "Claude",
        ProviderId.Codex => "Codex",
        _ => "Cursor",
    };

    /// <summary>The name the backend expects in <c>POST /ingest</c>.</summary>
    public static string CollectorName(this ProviderId id) => id switch
    {
        ProviderId.Claude => "claude-code",
        ProviderId.Codex => "codex",
        _ => "cursor",
    };

    /// <summary>The name the backend expects in <c>PUT /providers</c>.</summary>
    public static string ApiName(this ProviderId id) => id.ToString().ToLowerInvariant();
}

/// <summary>
/// One usage window. A null <see cref="UsedFraction"/> means the value is
/// unknown, which is never the same as 0%.
/// </summary>
public sealed record UsageWindow(string Id, string Label, double? UsedFraction, DateTimeOffset? ResetsAt)
{
    public static string DefaultLabel(string id) => id switch
    {
        "five_hour" => "5 horas",
        "seven_day" => "7 dias",
        _ => id,
    };

    public static double Clamp(double fraction) => Math.Clamp(fraction, 0, 1);
}

public enum Fidelity
{
    Official,
    Derived,
}

public enum StatusKind
{
    Ok,
    Stale,
    Error,
    /// <summary>Not read yet; not shown, so a provider without login never flashes in.</summary>
    Waiting,
    /// <summary>No login on this PC; not shown.</summary>
    SignedOut,
}

/// <summary>What the app knows about one provider right now.</summary>
public sealed record ProviderSnapshot(
    ProviderId Id,
    StatusKind Status,
    string? Message,
    IReadOnlyList<UsageWindow> Windows,
    Fidelity Fidelity,
    DateTimeOffset? ObservedAt)
{
    public bool HasData => Windows.Count > 0 && ObservedAt is not null;

    /// <summary>Whether the provider gets a card: it was read and has a login on this PC.</summary>
    public bool IsShown => Status is not (StatusKind.Waiting or StatusKind.SignedOut);

    public static ProviderSnapshot Waiting(ProviderId id) =>
        new(id, StatusKind.Waiting, "Aguardando a primeira leitura", [], Fidelity.Official, null);

    public static ProviderSnapshot Fresh(ProviderId id, IReadOnlyList<UsageWindow> windows, DateTimeOffset now,
        Fidelity fidelity = Fidelity.Official) =>
        new(id, StatusKind.Ok, null, windows, fidelity, now);
}
