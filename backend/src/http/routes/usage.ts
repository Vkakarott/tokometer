import type { FastifyInstance } from 'fastify';
import { buildProvidersView } from '../../core/freshness.ts';
import { DEFAULT_IDENTITY } from '../../core/identity.ts';
import type { ProvidersView } from '../../core/types.ts';
import { requireBearer } from '../auth.ts';
import type { ServerDeps } from '../server.ts';

const viewFor = (deps: ServerDeps, identity: string): ProvidersView =>
  buildProvidersView(
    (provider) => deps.snapshots.get(identity, provider),
    deps.now(),
    deps.staleAfterSeconds,
  );

export function registerUsage(server: FastifyInstance, deps: ServerDeps): void {
  server.get('/usage', async (request, reply) => {
    const token = requireBearer(request.headers.authorization);
    const identity = token === null ? null : deps.tokens.resolveDevice(token);
    if (identity === null) return reply.code(401).send({ error: 'unauthorized' });

    return reply.send(viewFor(deps, identity));
  });

  server.get('/usage/local', async (request, reply) => {
    if (!isLoopback(request.ip)) return reply.code(403).send({ error: 'local_only' });
    return reply.send(viewFor(deps, DEFAULT_IDENTITY));
  });
}

const isLoopback = (ip: string): boolean => ip === '127.0.0.1' || ip === '::1' || ip === '::ffff:127.0.0.1';
