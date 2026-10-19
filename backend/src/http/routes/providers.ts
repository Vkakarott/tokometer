import type { FastifyInstance } from 'fastify';
import { PROVIDER_IDS, type ProviderId } from '../../core/types.ts';
import { logger } from '../../logger.ts';
import { requireBearer } from '../auth.ts';
import type { ServerDeps } from '../server.ts';

const bodySchema = {
  type: 'object',
  required: ['enabled'],
  properties: {
    enabled: { type: 'array', uniqueItems: true, items: { type: 'string', enum: [...PROVIDER_IDS] } },
  },
} as const;

/** The macOS app sends which providers the user keeps visible; displays show only those. */
export function registerProviders(server: FastifyInstance, deps: ServerDeps): void {
  server.put('/providers', { schema: { body: bodySchema } }, async (request, reply) => {
    const token = requireBearer(request.headers.authorization);
    const identity = token === null ? null : deps.tokens.resolveCollector(token);
    if (identity === null) return reply.code(401).send({ error: 'unauthorized' });

    const { enabled } = request.body as { enabled: ProviderId[] };
    deps.providerSelection.set(identity, enabled);
    logger.info('provider selection updated', { identity, enabled });
    return reply.code(204).send();
  });
}
