import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ProviderSelection } from '../src/core/providerSelection.ts';

test('enables every provider for an identity that never chose', () => {
  assert.deepEqual(new ProviderSelection().enabledFor('default'), ['claude', 'codex', 'cursor']);
});

test('returns the chosen providers in the canonical order', () => {
  const selection = new ProviderSelection();
  selection.set('default', ['codex', 'claude']);
  assert.deepEqual(selection.enabledFor('default'), ['claude', 'codex']);
});

test('keeps an empty choice instead of falling back to every provider', () => {
  const selection = new ProviderSelection();
  selection.set('default', []);
  assert.deepEqual(selection.enabledFor('default'), []);
});

test('round-trips through JSON and drops unknown provider names', () => {
  const selection = new ProviderSelection();
  selection.set('default', ['claude']);
  const restored = ProviderSelection.fromJSON({ ...selection.toJSON(), other: ['codex', 'gemini'] });
  assert.deepEqual(restored.enabledFor('default'), ['claude']);
  assert.deepEqual(restored.enabledFor('other'), ['codex']);
});
