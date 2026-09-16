export type WindowId = 'five_hour' | 'seven_day';

export type ProviderId = 'claude' | 'codex';

export const PROVIDER_IDS: readonly ProviderId[] = ['claude', 'codex'];

/** Collectors name the tool they read from; views name the provider. */
export const PROVIDER_BY_COLLECTOR = {
  'claude-code': 'claude',
  codex: 'codex',
} as const satisfies Record<string, ProviderId>;

export type CollectorName = keyof typeof PROVIDER_BY_COLLECTOR;

export type UsageWindow = {
  id: WindowId;
  usedPercentage: number;
  resetsAt: number;
};

export type ContextUsage = {
  inputTokens: number;
  outputTokens: number;
  windowSize: number;
  usedPercentage: number;
};

export type Snapshot = {
  provider: CollectorName;
  source: string;
  observedAt: number;
  windows: UsageWindow[];
  context?: ContextUsage;
};

/** usedPercentage is null when the window already reset and the stored value is meaningless. */
export type WindowView = {
  id: WindowId;
  usedPercentage: number | null;
  resetsAt: number;
};

export type UsageView = {
  windows: WindowView[];
  context: ContextUsage | null;
  ageSeconds: number;
  stale: boolean;
  hasData: boolean;
};

export type ProvidersView = {
  providers: Record<ProviderId, UsageView>;
};
