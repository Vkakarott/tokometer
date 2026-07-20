import { test } from 'node:test';
import assert from 'node:assert/strict';
import { generateUserCode, normalizeUserCode } from '../src/core/userCode.ts';

const ALPHABET = 'ABCDEFGHJKMNPQRSTVWXYZ23456789';

test('generates an 8-character code from the unambiguous alphabet', () => {
  for (let i = 0; i < 200; i++) {
    const code = generateUserCode();
    assert.equal(code.length, 8);
    for (const char of code) {
      assert.ok(ALPHABET.includes(char), `"${char}" is not in the alphabet`);
    }
  }
});

test('never emits visually ambiguous characters', () => {
  const codes = Array.from({ length: 500 }, () => generateUserCode()).join('');
  for (const banned of ['I', 'L', 'O', 'U', '0', '1']) {
    assert.ok(!codes.includes(banned), `"${banned}" is ambiguous on the OLED`);
  }
});

test('does not repeat within a small sample', () => {
  const codes = new Set(Array.from({ length: 500 }, () => generateUserCode()));
  assert.equal(codes.size, 500);
});

test('normalizes what a human types back to storage form', () => {
  assert.equal(normalizeUserCode('k7qm-3f9a'), 'K7QM3F9A');
  assert.equal(normalizeUserCode(' K7QM 3F9A '), 'K7QM3F9A');
  assert.equal(normalizeUserCode('K7QM3F9A'), 'K7QM3F9A');
});
