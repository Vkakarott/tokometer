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

  // No auth today: there is no Anthropic credential to protect, and the /web is
  // LAN-only. This gains a session check when accounts arrive.
  server.get('/usage/web', async (_request, reply) => reply.send(viewFor(deps, DEFAULT_IDENTITY)));
}
