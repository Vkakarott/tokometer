import Fastify, { type FastifyInstance } from 'fastify';
import type { PairingStore } from '../core/pairing.ts';
import type { SnapshotStore } from '../core/snapshots.ts';
import type { TokenRegistry } from '../core/identity.ts';
import { registerDevice } from './routes/device.ts';
import { registerIngest } from './routes/ingest.ts';
import { registerUsage } from './routes/usage.ts';

export type ServerDeps = {
  snapshots: SnapshotStore;
  tokens: TokenRegistry;
  pairing: PairingStore;
  now: () => number;
  staleAfterSeconds: number;
  verificationUri: string;
};

export function buildServer(deps: ServerDeps): FastifyInstance {
  const server = Fastify({ logger: false });
  registerIngest(server, deps);
  registerUsage(server, deps);
  registerDevice(server, deps);
  return server;
}
