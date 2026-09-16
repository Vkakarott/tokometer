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

const codexSnapshot: Snapshot = {
  provider: 'codex',
  source: 'laptop',
  observedAt: 1_800_000_030,
  windows: [{ id: 'five_hour', usedPercentage: 38, resetsAt: 1_800_018_000 }],
};

test('returns null for an identity that never reported', () => {
  assert.equal(new SnapshotStore().get('default', 'claude'), null);
});

test('stores and reads back per identity', () => {
  const store = new SnapshotStore();
  store.put('default', 'claude', snapshot);
  assert.deepEqual(store.get('default', 'claude'), snapshot);
  assert.equal(store.get('someone-else', 'claude'), null);
});

test('keeps providers apart: codex never overwrites claude', () => {
  const store = new SnapshotStore();
  store.put('default', 'claude', snapshot);
  store.put('default', 'codex', codexSnapshot);
  assert.deepEqual(store.get('default', 'claude'), snapshot);
  assert.deepEqual(store.get('default', 'codex'), codexSnapshot);
});

test('last write wins: two machines report the same account', () => {
  const store = new SnapshotStore();
  store.put('default', 'claude', snapshot);
  const newer: Snapshot = { ...snapshot, source: 'desktop', observedAt: snapshot.observedAt + 60 };
  store.put('default', 'claude', newer);
  assert.equal(store.get('default', 'claude')?.source, 'desktop');
});

test('survives a round trip through JSON', () => {
  const store = new SnapshotStore();
  store.put('default', 'claude', snapshot);
  store.put('default', 'codex', codexSnapshot);
  const restored = SnapshotStore.fromJSON(JSON.parse(JSON.stringify(store.toJSON())));
  assert.deepEqual(restored.get('default', 'claude'), snapshot);
  assert.deepEqual(restored.get('default', 'codex'), codexSnapshot);
});

test('migrates state saved before providers existed as claude', () => {
  const restored = SnapshotStore.fromJSON({ default: snapshot });
  assert.deepEqual(restored.get('default', 'claude'), snapshot);
  assert.equal(restored.get('default', 'codex'), null);
});
