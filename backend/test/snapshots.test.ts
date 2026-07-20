import { test } from 'node:test';
import assert from 'node:assert/strict';
import { SnapshotStore } from '../src/core/snapshots.ts';
import type { Snapshot } from '../src/core/types.ts';

const snapshot: Snapshot = {
  provider: 'claude-code',
  source: 'laptop',
  observedAt: 1_800_000_000,
  windows: [{ id: 'five_hour', usedPercentage: 10, resetsAt: 1_800_003_600 }],
};

test('returns null for an identity that never reported', () => {
  assert.equal(new SnapshotStore().get('default'), null);
});

test('stores and reads back per identity', () => {
  const store = new SnapshotStore();
  store.put('default', snapshot);
  assert.deepEqual(store.get('default'), snapshot);
  assert.equal(store.get('someone-else'), null);
});

test('last write wins: two machines report the same account', () => {
  const store = new SnapshotStore();
  store.put('default', snapshot);
  const newer: Snapshot = { ...snapshot, source: 'desktop', observedAt: snapshot.observedAt + 60 };
  store.put('default', newer);
  assert.equal(store.get('default')?.source, 'desktop');
});

test('survives a round trip through JSON', () => {
  const store = new SnapshotStore();
  store.put('default', snapshot);
  const restored = SnapshotStore.fromJSON(JSON.parse(JSON.stringify(store.toJSON())));
  assert.deepEqual(restored.get('default'), snapshot);
});
