import {
  PROVIDER_IDS,
  type ProviderId,
  type ProvidersView,
  type Snapshot,
  type UsageView,
  type WindowView,
} from './types.ts';

const viewWindow = (window: Snapshot['windows'][number], nowSeconds: number): WindowView => ({
  id: window.id,
  // The window already reset: the stored percentage describes a window that no
  // longer exists. Report unknown rather than a confidently wrong number.
  usedPercentage: nowSeconds > window.resetsAt ? null : window.usedPercentage,
  resetsAt: window.resetsAt,
});

export function buildUsageView(
  snapshot: Snapshot | null,
  nowSeconds: number,
  staleAfterSeconds: number,
): UsageView {
  if (snapshot === null) {
    return {
      windows: [],
      context: null,
      ageSeconds: 0,
      stale: false,
      hasData: false,
    };
  }

  const ageSeconds = Math.max(0, nowSeconds - snapshot.observedAt);
  const stale = ageSeconds > staleAfterSeconds;

  if (snapshot.windows.length === 0) {
    return {
      windows: [],
      context: snapshot.context ?? null,
      ageSeconds,
      stale,
      hasData: false,
    };
  }

  return {
    windows: snapshot.windows.map((window) => viewWindow(window, nowSeconds)),
    context: snapshot.context ?? null,
    ageSeconds,
    stale,
    hasData: true,
  };
}

/** Every known provider appears, so a missing one reads as "no data", never as absent. */
export function buildProvidersView(
  snapshotFor: (provider: ProviderId) => Snapshot | null,
  nowSeconds: number,
  staleAfterSeconds: number,
): ProvidersView {
  const entries = PROVIDER_IDS.map(
    (provider) => [provider, buildUsageView(snapshotFor(provider), nowSeconds, staleAfterSeconds)] as const,
  );
  return { providers: Object.fromEntries(entries) as Record<ProviderId, UsageView> };
}
