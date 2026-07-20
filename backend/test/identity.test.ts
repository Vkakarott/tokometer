import { test } from 'node:test';
import assert from 'node:assert/strict';
import { TokenRegistry } from '../src/core/identity.ts';

test('resolves a registered device token to its identity', () => {
  const registry = new TokenRegistry('collector-secret');
  registry.registerDevice('device-token-abc', 'default');
  assert.equal(registry.resolveDevice('device-token-abc'), 'default');
});

test('rejects an unknown device token', () => {
  const registry = new TokenRegistry('collector-secret');
  assert.equal(registry.resolveDevice('nope'), null);
});

test('resolves the collector token to its identity', () => {
  const registry = new TokenRegistry('collector-secret');
  assert.equal(registry.resolveCollector('collector-secret'), 'default');
});

test('rejects a wrong collector token', () => {
  const registry = new TokenRegistry('collector-secret');
  assert.equal(registry.resolveCollector('wrong'), null);
});

test('rejects a collector token of a different length without throwing', () => {
  const registry = new TokenRegistry('collector-secret');
  assert.equal(registry.resolveCollector('x'), null);
});

test('survives a round trip through JSON', () => {
  const registry = new TokenRegistry('collector-secret');
  registry.registerDevice('device-token-abc', 'default');
  const restored = TokenRegistry.fromJSON(
    'collector-secret',
    JSON.parse(JSON.stringify(registry.toJSON())),
  );
  assert.equal(restored.resolveDevice('device-token-abc'), 'default');
});
