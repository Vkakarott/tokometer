export type WindowId = 'five_hour' | 'seven_day';

export type WindowView = {
  id: WindowId;
  usedPercentage: number | null;
  resetsAt: number;
};

export type ContextUsage = {
  inputTokens: number;
  outputTokens: number;
  windowSize: number;
  usedPercentage: number;
};

export type UsageView = {
  windows: WindowView[];
  context: ContextUsage | null;
  ageSeconds: number;
  stale: boolean;
  hasData: boolean;
};

export type ProviderId = 'claude' | 'codex';

export const PROVIDER_ORDER: readonly ProviderId[] = ['claude', 'codex'];

export type ProvidersView = {
  providers: Record<ProviderId, UsageView>;
};

export async function fetchUsage(): Promise<ProvidersView> {
  const response = await fetch('/api/usage/web');
  if (!response.ok) throw new Error(`usage request failed: ${response.status}`);
  return (await response.json()) as ProvidersView;
}

export async function approvePairing(userCode: string): Promise<void> {
  const response = await fetch('/api/pair/approve', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ user_code: userCode }),
  });
  if (!response.ok) {
    const body = (await response.json()) as { error?: string };
    throw new Error(body.error ?? 'unknown');
  }
}
