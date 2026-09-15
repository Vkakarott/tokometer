export type WindowId = 'five_hour' | 'seven_day';

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
  provider: string;
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
