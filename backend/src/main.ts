import { PairingStore } from './core/pairing.ts';
import { SnapshotStore } from './core/snapshots.ts';
import { TokenRegistry } from './core/identity.ts';
import { buildServer } from './http/server.ts';
import { loadJson, saveJson } from './store/jsonFile.ts';
import { logger } from './logger.ts';

type PersistedState = {
  // Unknown on purpose: SnapshotStore.fromJSON also migrates the pre-provider format.
  snapshots: Record<string, unknown>;
  deviceTokens: Record<string, string>;
};

const PAIRING_TTL_SECONDS = 300;
const SAVE_INTERVAL_MS = 10_000;

const required = (name: string): string => {
  const value = process.env[name];
  if (value === undefined || value.length === 0) {
    logger.error('missing required environment variable', { name });
    process.exit(1);
  }
  return value;
};

const start = async (): Promise<void> => {
  const stateFile = process.env.TOKESP_STATE_FILE ?? './state.json';
  const state = await loadJson<PersistedState>(stateFile, { snapshots: {}, deviceTokens: {} });

  const snapshots = SnapshotStore.fromJSON(state.snapshots);
  const tokens = TokenRegistry.fromJSON(required('TOKESP_COLLECTOR_TOKEN'), state.deviceTokens);
  const pairing = new PairingStore(PAIRING_TTL_SECONDS);

  const server = buildServer({
    snapshots,
    tokens,
    pairing,
    now: () => Math.floor(Date.now() / 1000),
    staleAfterSeconds: Number(process.env.TOKESP_STALE_AFTER_SECONDS ?? 900),
    verificationUri: required('TOKESP_VERIFICATION_URI'),
  });

  const persist = (): Promise<void> =>
    saveJson(stateFile, { snapshots: snapshots.toJSON(), deviceTokens: tokens.toJSON() });

  setInterval(() => void persist(), SAVE_INTERVAL_MS).unref();
  for (const signal of ['SIGINT', 'SIGTERM'] as const) {
    process.on(signal, () => void persist().then(() => process.exit(0)));
  }

  const port = Number(process.env.PORT ?? 8080);
  await server.listen({ port, host: '0.0.0.0' });
  logger.info('backend listening', { port });
};

void start();
