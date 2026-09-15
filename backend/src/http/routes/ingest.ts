import type { FastifyInstance } from 'fastify';
import type { Snapshot } from '../../core/types.ts';
import { logger } from '../../logger.ts';
import { requireBearer } from '../auth.ts';
import type { ServerDeps } from '../server.ts';

const bodySchema = {
  type: 'object',
  required: ['provider', 'source', 'observedAt', 'windows'],
  properties: {
    provider: { type: 'string' },
    source: { type: 'string' },
    observedAt: { type: 'number' },
    windows: {
      type: 'array',
      items: {
        type: 'object',
        required: ['id', 'usedPercentage', 'resetsAt'],
        properties: {
          id: { type: 'string', enum: ['five_hour', 'seven_day'] },
          usedPercentage: { type: 'number', minimum: 0, maximum: 100 },
          resetsAt: { type: 'number' },
        },
      },
    },
    context: {
      type: 'object',
      required: ['inputTokens', 'outputTokens', 'windowSize', 'usedPercentage'],
      properties: {
        inputTokens: { type: 'number', minimum: 0 },
        outputTokens: { type: 'number', minimum: 0 },
        windowSize: { type: 'number', minimum: 1 },
        usedPercentage: { type: 'number', minimum: 0, maximum: 100 },
      },
    },
  },
} as const;

export function registerIngest(server: FastifyInstance, deps: ServerDeps): void {
  server.post('/ingest', { schema: { body: bodySchema } }, async (request, reply) => {
    const token = requireBearer(request.headers.authorization);
    const identity = token === null ? null : deps.tokens.resolveCollector(token);
    if (identity === null) return reply.code(401).send({ error: 'unauthorized' });

    const snapshot = request.body as Snapshot;
    deps.snapshots.put(identity, snapshot);
    logger.info('snapshot ingested', { identity, source: snapshot.source });
    return reply.code(204).send();
  });
}
