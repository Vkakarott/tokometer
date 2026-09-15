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
  // No type coercion: Ajv's default would turn a null percentage into 0 and let
  // clients show a confident 0% for a window that is actually unknown.
  const server = Fastify({ logger: false, ajv: { customOptions: { coerceTypes: false } } });
  registerIngest(server, deps);
  registerUsage(server, deps);
  registerDevice(server, deps);
  return server;
}
