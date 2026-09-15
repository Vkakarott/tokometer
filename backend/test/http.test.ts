import { test } from 'node:test';
import assert from 'node:assert/strict';
import { buildServer } from '../src/http/server.ts';
import { SnapshotStore } from '../src/core/snapshots.ts';
import { TokenRegistry } from '../src/core/identity.ts';
import { PairingStore } from '../src/core/pairing.ts';

const NOW = 1_800_000_000;

const build = (nowRef = { value: NOW }) =>
  buildServer({
    snapshots: new SnapshotStore(),
    tokens: new TokenRegistry('collector-secret'),
    pairing: new PairingStore(300),
    now: () => nowRef.value,
    staleAfterSeconds: 900,
    verificationUri: 'http://tok.local/pair',
  });

const payload = {
  provider: 'claude-code',
  source: 'laptop',
  observedAt: NOW,
  windows: [{ id: 'five_hour', usedPercentage: 23.5, resetsAt: NOW + 3600 }],
  context: { inputTokens: 12_500, outputTokens: 2_400, windowSize: 200_000, usedPercentage: 7.45 },
};

test('rejects ingest without a token', async () => {
  const server = build();
  const response = await server.inject({ method: 'POST', url: '/ingest', payload });
  assert.equal(response.statusCode, 401);
});

test('rejects ingest with a wrong token', async () => {
  const server = build();
  const response = await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer wrong' },
    payload,
  });
  assert.equal(response.statusCode, 401);
});

test('accepts ingest from the collector', async () => {
  const server = build();
  const response = await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer collector-secret' },
    payload,
  });
  assert.equal(response.statusCode, 204);
});

test('rejects ingest with a malformed body', async () => {
  const server = build();
  const response = await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer collector-secret' },
    payload: { provider: 'claude-code' },
  });
  assert.equal(response.statusCode, 400);
});

test('rejects ingest with a null percentage instead of storing 0', async () => {
  const server = build();
  const response = await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer collector-secret' },
    payload: { ...payload, windows: [{ id: 'five_hour', usedPercentage: null, resetsAt: NOW + 3600 }] },
  });
  assert.equal(response.statusCode, 400);

  const usage = await server.inject({ method: 'GET', url: '/usage/web' });
  assert.equal(usage.json().hasData, false);
});

test('rejects ingest with a numeric string percentage', async () => {
  const server = build();
  const response = await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer collector-secret' },
    payload: { ...payload, windows: [{ id: 'five_hour', usedPercentage: '42', resetsAt: NOW + 3600 }] },
  });
  assert.equal(response.statusCode, 400);
});

test('rejects usage without a device token', async () => {
  const server = build();
  assert.equal((await server.inject({ method: 'GET', url: '/usage' })).statusCode, 401);
});

test('rejects usage when the device token is not a known device', async () => {
  const server = build();
  const response = await server.inject({
    method: 'GET',
    url: '/usage',
    headers: { authorization: 'Bearer collector-secret' },
  });
  assert.equal(response.statusCode, 401, 'the collector token must not read the device endpoint');
});

test('walks the full pairing flow and then serves usage to the device', async () => {
  const server = build();

  const codeResponse = await server.inject({
    method: 'POST',
    url: '/device/code',
    payload: { hardware_id: 'esp32-abc' },
  });
  assert.equal(codeResponse.statusCode, 200);
  const { device_code, user_code, verification_uri, interval } = codeResponse.json();
  assert.equal(user_code.length, 8);
  assert.equal(verification_uri, 'http://tok.local/pair');
  assert.equal(interval, 5);

  const pending = await server.inject({
    method: 'POST',
    url: '/device/token',
    payload: { device_code },
  });
  assert.equal(pending.statusCode, 400);
  assert.equal(pending.json().error, 'authorization_pending');

  const approved = await server.inject({
    method: 'POST',
    url: '/pair/approve',
    payload: { user_code },
  });
  assert.equal(approved.statusCode, 204);

  const tokenResponse = await server.inject({
    method: 'POST',
    url: '/device/token',
    payload: { device_code },
  });
  assert.equal(tokenResponse.statusCode, 200);
  const { access_token } = tokenResponse.json();

  await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer collector-secret' },
    payload,
  });

  const usage = await server.inject({
    method: 'GET',
    url: '/usage',
    headers: { authorization: `Bearer ${access_token}` },
  });
  assert.equal(usage.statusCode, 200);
  const body = usage.json();
  assert.equal(body.hasData, true);
  assert.equal(body.ageSeconds, 0);
  assert.equal(body.windows[0].usedPercentage, 23.5);
  assert.equal(body.context.inputTokens, 12_500);
});

test('reports expired_token once the code TTL passes', async () => {
  const nowRef = { value: NOW };
  const server = build(nowRef);
  const { device_code } = (
    await server.inject({
      method: 'POST',
      url: '/device/code',
      payload: { hardware_id: 'esp32-abc' },
    })
  ).json();

  nowRef.value = NOW + 301;
  const response = await server.inject({
    method: 'POST',
    url: '/device/token',
    payload: { device_code },
  });
  assert.equal(response.statusCode, 400);
  assert.equal(response.json().error, 'expired_token');
});

test('serves the web view without a token', async () => {
  const server = build();
  const response = await server.inject({ method: 'GET', url: '/usage/web' });
  assert.equal(response.statusCode, 200);
  assert.equal(response.json().hasData, false);
});
