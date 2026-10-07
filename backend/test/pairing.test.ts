import { test } from 'node:test';
import assert from 'node:assert/strict';
import { PairingStore } from '../src/core/pairing.ts';

const NOW = 1_800_000_000;
const TTL = 300;

test('redeem stays pending until a human approves', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  assert.equal(store.redeem(record.deviceCode, NOW).status, 'pending');
});

test('redeem returns a token once approved', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  assert.equal(store.approve(record.userCode, NOW).status, 'ok');

  const result = store.redeem(record.deviceCode, NOW);
  assert.equal(result.status, 'approved');
  assert.ok(result.status === 'approved' && result.accessToken.length >= 43);
});

test('a code is single-use: the second redeem no longer knows it', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  store.approve(record.userCode, NOW);
  store.redeem(record.deviceCode, NOW);
  assert.equal(store.redeem(record.deviceCode, NOW).status, 'unknown');
});

test('expires after the TTL', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  assert.equal(store.redeem(record.deviceCode, NOW + TTL + 1).status, 'expired');
});

test('refuses to approve an expired code', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  assert.equal(store.approve(record.userCode, NOW + TTL + 1).status, 'expired');
});

test('refuses to approve a code that does not exist', () => {
  const store = new PairingStore(TTL);
  assert.equal(store.approve('ZZZZZZZZ', NOW).status, 'unknown');
});

test('approve accepts the hyphenated form a human reads off the OLED', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  const typed = `${record.userCode.slice(0, 4)}-${record.userCode.slice(4)}`.toLowerCase();
  assert.equal(store.approve(typed, NOW).status, 'ok');
});

test('redeem reports unknown for an unrecognised device code', () => {
  const store = new PairingStore(TTL);
  assert.equal(store.redeem('nope', NOW).status, 'unknown');
});

test('sweep drops expired records', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  store.sweep(NOW + TTL + 1);
  assert.equal(store.redeem(record.deviceCode, NOW).status, 'unknown');
});

test('issues a distinct token per device', () => {
  const store = new PairingStore(TTL);
  const first = store.create('esp32-a', NOW);
  const second = store.create('esp32-b', NOW);
  store.approve(first.userCode, NOW);
  store.approve(second.userCode, NOW);

  const a = store.redeem(first.deviceCode, NOW);
  const b = store.redeem(second.deviceCode, NOW);
  assert.ok(a.status === 'approved' && b.status === 'approved');
  assert.notEqual(
    a.status === 'approved' ? a.accessToken : '',
    b.status === 'approved' ? b.accessToken : '',
  );
});

test('a preapproved board gets a code that is already approved', () => {
  const store = new PairingStore(TTL);
  store.preapprove('esp32-abc', NOW);
  const record = store.create('esp32-abc', NOW + 10);
  assert.equal(store.redeem(record.deviceCode, NOW + 10).status, 'approved');
});

test('preapproval also covers the code the board already holds', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  store.preapprove('esp32-abc', NOW);
  assert.equal(store.redeem(record.deviceCode, NOW).status, 'approved');
});

test('preapproval is single-use, expires, and never leaks to another board', () => {
  const store = new PairingStore(TTL);
  store.preapprove('esp32-abc', NOW);
  assert.equal(store.redeem(store.create('esp32-other', NOW).deviceCode, NOW).status, 'pending');
  assert.equal(store.redeem(store.create('esp32-abc', NOW).deviceCode, NOW).status, 'approved');
  assert.equal(store.redeem(store.create('esp32-abc', NOW).deviceCode, NOW).status, 'pending');

  store.preapprove('esp32-late', NOW);
  const late = store.create('esp32-late', NOW + TTL + 1);
  assert.equal(store.redeem(late.deviceCode, NOW + TTL + 1).status, 'pending');
});

test('a board counts as paired only after it redeems its code', () => {
  const store = new PairingStore(TTL);
  store.preapprove('esp32-abc', NOW);
  assert.equal(store.isPaired('esp32-abc'), false);
  store.redeem(store.create('esp32-abc', NOW).deviceCode, NOW);
  assert.equal(store.isPaired('esp32-abc'), true);

  store.preapprove('esp32-abc', NOW);
  assert.equal(store.isPaired('esp32-abc'), false, 'a new setup starts from unpaired');
});
