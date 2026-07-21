import { test } from 'node:test';
import assert from 'node:assert/strict';
import { buildUsageView } from '../src/core/freshness.ts';
import type { Snapshot } from '../src/core/types.ts';

const NOW = 1_800_000_000;

const snapshot = (overrides: Partial<Snapshot> = {}): Snapshot => ({
  provider: 'claude-code',
  source: 'laptop',
  observedAt: NOW,
  windows: [
    { id: 'five_hour', usedPercentage: 23.5, resetsAt: NOW + 3600 },
    { id: 'seven_day', usedPercentage: 41.2, resetsAt: NOW + 86_400 },
  ],
  ...overrides,
});

test('reports fresh data with both windows known', () => {
  const view = buildUsageView(snapshot(), NOW, 900);
  assert.equal(view.hasData, true);
  assert.equal(view.stale, false);
  assert.equal(view.ageSeconds, 0);
  assert.equal(view.windows[0]?.usedPercentage, 23.5);
});

test('marks a rolled-over window as unknown instead of showing the old number', () => {
  const view = buildUsageView(snapshot(), NOW + 7200, 999_999);
  const fiveHour = view.windows.find((w) => w.id === 'five_hour');
  const sevenDay = view.windows.find((w) => w.id === 'seven_day');
  assert.equal(fiveHour?.usedPercentage, null, 'window reset, old value is garbage');
  assert.equal(sevenDay?.usedPercentage, 41.2, 'rollover is per-window, not global');
});

test('flags data older than the stale threshold', () => {
  const view = buildUsageView(snapshot(), NOW + 1000, 900);
  assert.equal(view.stale, true);
  assert.equal(view.ageSeconds, 1000);
});

test('reports no data when no snapshot was ever ingested', () => {
  const view = buildUsageView(null, NOW, 900);
  assert.equal(view.hasData, false);
  assert.deepEqual(view.windows, []);
});

test('reports no data when Claude Code has not answered yet', () => {
  const view = buildUsageView(snapshot({ windows: [] }), NOW, 900);
  assert.equal(view.hasData, false, 'empty windows is not the same as 0%');
});

test('never reports a negative age when clocks disagree', () => {
  const view = buildUsageView(snapshot(), NOW - 50, 900);
  assert.equal(view.ageSeconds, 0);
});

test('at exactly resetsAt the window is not yet rolled over', () => {
  const view = buildUsageView(snapshot(), NOW + 3600, 999_999);
  const fiveHour = view.windows.find((w) => w.id === 'five_hour');
  assert.equal(fiveHour?.usedPercentage, 23.5, 'boundary is exclusive: > not >=');
});
