import { test } from 'node:test';
import assert from 'node:assert/strict';
import { buildServer } from '../src/http/server.ts';
import { SnapshotStore } from '../src/core/snapshots.ts';
import { ProviderSelection } from '../src/core/providerSelection.ts';
import { TokenRegistry } from '../src/core/identity.ts';
import { PairingStore } from '../src/core/pairing.ts';

const NOW = 1_800_000_000;

const build = (nowRef = { value: NOW }) =>
  buildServer({
    snapshots: new SnapshotStore(),
    providerSelection: new ProviderSelection(),
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

  const usage = await server.inject({ method: 'GET', url: '/usage/local' });
  assert.equal(usage.json().providers.claude.hasData, false);
});

test('rejects ingest from an unknown collector', async () => {
  const server = build();
  const response = await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer collector-secret' },
    payload: { ...payload, provider: 'gemini' },
  });
  assert.equal(response.statusCode, 400);
});

test('serves Claude, Codex, and Cursor side by side without mixing them', async () => {
  const server = build();
  const ingest = (body: object) =>
    server.inject({
      method: 'POST',
      url: '/ingest',
      headers: { authorization: 'Bearer collector-secret' },
      payload: body,
    });

  await ingest(payload);
  const codex = await ingest({
    provider: 'codex',
    source: 'laptop',
    observedAt: NOW,
    windows: [{ id: 'five_hour', usedPercentage: 38, resetsAt: NOW + 18_000 }],
  });
  assert.equal(codex.statusCode, 204);
  const cursor = await ingest({
    provider: 'cursor',
    source: 'laptop',
    observedAt: NOW,
    windows: [{ id: 'included', usedPercentage: 12, resetsAt: NOW + 864_000 }],
  });
  assert.equal(cursor.statusCode, 204);

  const { providers } = (await server.inject({ method: 'GET', url: '/usage/local' })).json();
  assert.equal(providers.claude.windows[0].usedPercentage, 23.5);
  assert.equal(providers.claude.context.inputTokens, 12_500);
  assert.equal(providers.codex.windows[0].usedPercentage, 38);
  assert.equal(providers.codex.context, null);
  assert.equal(providers.cursor.windows[0].usedPercentage, 12);
  assert.equal(providers.cursor.windows[0].id, 'included');
});

test('preserves Cursor billing windows through ingest, stale data, and reset', async () => {
  const nowRef = { value: NOW };
  const server = build(nowRef);
  const windows = [
    { id: 'included', usedPercentage: 21, resetsAt: NOW + 3600 },
    { id: 'api', usedPercentage: 8, resetsAt: NOW + 3600 },
    { id: 'on_demand', usedPercentage: 12.5, resetsAt: NOW + 3600 },
  ];
  const response = await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer collector-secret' },
    payload: { provider: 'cursor', source: 'test', observedAt: NOW, windows },
  });
  assert.equal(response.statusCode, 204);

  const current = (await server.inject({ method: 'GET', url: '/usage/local' })).json().providers.cursor;
  assert.equal(current.hasData, true);
  assert.equal(current.stale, false);
  assert.deepEqual(current.windows, windows);

  nowRef.value += 1000;
  const stale = (await server.inject({ method: 'GET', url: '/usage/local' })).json().providers.cursor;
  assert.equal(stale.stale, true);
  assert.equal(stale.ageSeconds, 1000);
  assert.deepEqual(stale.windows, windows);

  nowRef.value = NOW + 3601;
  const expired = (await server.inject({ method: 'GET', url: '/usage/local' })).json().providers.cursor;
  assert.deepEqual(expired.windows, windows.map((window) => ({ ...window, usedPercentage: null })));
});

test('does not fabricate absent Cursor billing windows', async () => {
  const server = build();
  const window = { id: 'included', usedPercentage: 0, resetsAt: NOW + 3600 };
  const response = await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer collector-secret' },
    payload: { provider: 'cursor', source: 'test', observedAt: NOW, windows: [window] },
  });
  assert.equal(response.statusCode, 204);
  const cursor = (await server.inject({ method: 'GET', url: '/usage/local' })).json().providers.cursor;
  assert.deepEqual(cursor.windows, [window]);
  assert.equal(cursor.hasData, true, 'known zero is still data');
});

test('rejects unknown Cursor percentages without creating a zero usage snapshot', async () => {
  const server = build();
  for (const id of ['included', 'api', 'on_demand']) {
    const response = await server.inject({
      method: 'POST',
      url: '/ingest',
      headers: { authorization: 'Bearer collector-secret' },
      payload: {
        provider: 'cursor', source: 'test', observedAt: NOW,
        windows: [{ id, usedPercentage: null, resetsAt: NOW + 3600 }],
      },
    });
    assert.equal(response.statusCode, 400);
  }
  const cursor = (await server.inject({ method: 'GET', url: '/usage/local' })).json().providers.cursor;
  assert.equal(cursor.hasData, false);
  assert.deepEqual(cursor.windows, []);
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

const putProviders = (server: ReturnType<typeof build>, body: unknown, token = 'collector-secret') =>
  server.inject({
    method: 'PUT',
    url: '/providers',
    headers: { authorization: `Bearer ${token}` },
    payload: body as Record<string, unknown>,
  });

test('serves only the providers the app enabled', async () => {
  const server = build();
  assert.equal((await putProviders(server, { enabled: ['claude', 'codex'] })).statusCode, 204);
  const { providers } = (await server.inject({ method: 'GET', url: '/usage/local' })).json();
  assert.deepEqual(Object.keys(providers), ['claude', 'codex']);
});

test('serves no provider when the app disabled them all', async () => {
  const server = build();
  await putProviders(server, { enabled: [] });
  const { providers } = (await server.inject({ method: 'GET', url: '/usage/local' })).json();
  assert.deepEqual(providers, {});
});

test('rejects a provider selection without the collector token', async () => {
  const server = build();
  assert.equal((await putProviders(server, { enabled: ['claude'] }, 'wrong')).statusCode, 401);
});

test('rejects a provider selection naming an unknown provider', async () => {
  const server = build();
  assert.equal((await putProviders(server, { enabled: ['gemini'] })).statusCode, 400);
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
  assert.equal(interval, 2);

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
  const { providers } = usage.json();
  assert.equal(providers.claude.hasData, true);
  assert.equal(providers.claude.ageSeconds, 0);
  assert.equal(providers.claude.windows[0].usedPercentage, 23.5);
  assert.equal(providers.claude.context.inputTokens, 12_500);
  assert.equal(providers.codex.hasData, false);
  assert.equal(providers.cursor.hasData, false);
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

test('serves the local diagnostic view without a token', async () => {
  const server = build();
  const response = await server.inject({ method: 'GET', url: '/usage/local' });
  assert.equal(response.statusCode, 200);
  assert.equal(response.json().providers.claude.hasData, false);
  assert.equal(response.json().providers.codex.hasData, false);
  assert.equal(response.json().providers.cursor.hasData, false);
});

test('this machine pairs a board by hardware id without anyone typing a code', async () => {
  const server = build();
  const approved = await server.inject({
    method: 'POST',
    url: '/pair/approve',
    payload: { hardware_id: 'esp32-abc' },
  });
  assert.equal(approved.statusCode, 204);
  const status = () => server.inject({ method: 'GET', url: '/pair/status?hardware_id=esp32-abc' });
  assert.equal((await status()).json().paired, false);

  const { device_code } = (
    await server.inject({ method: 'POST', url: '/device/code', payload: { hardware_id: 'esp32-abc' } })
  ).json();
  const token = await server.inject({ method: 'POST', url: '/device/token', payload: { device_code } });
  assert.equal(token.statusCode, 200);
  assert.equal((await status()).json().paired, true);
});

test('refuses hardware id approval and pairing status from another machine', async () => {
  const server = build();
  const approved = await server.inject({
    method: 'POST',
    url: '/pair/approve',
    payload: { hardware_id: 'esp32-abc' },
    remoteAddress: '192.168.1.50',
  });
  assert.equal(approved.statusCode, 403);
  const status = await server.inject({
    method: 'GET',
    url: '/pair/status?hardware_id=esp32-abc',
    remoteAddress: '192.168.1.50',
  });
  assert.equal(status.statusCode, 403);
});

test('rejects an approval that names neither a code nor a board', async () => {
  const server = build();
  const response = await server.inject({ method: 'POST', url: '/pair/approve', payload: {} });
  assert.equal(response.statusCode, 400);
});
