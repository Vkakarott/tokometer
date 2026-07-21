import type { FastifyInstance } from 'fastify';
import { DEFAULT_IDENTITY } from '../../core/identity.ts';
import { logger } from '../../logger.ts';
import type { ServerDeps } from '../server.ts';

const POLL_INTERVAL_SECONDS = 5;

const codeSchema = {
  type: 'object',
  required: ['hardware_id'],
  properties: { hardware_id: { type: 'string' } },
} as const;

const tokenSchema = {
  type: 'object',
  required: ['device_code'],
  properties: { device_code: { type: 'string' } },
} as const;

const approveSchema = {
  type: 'object',
  required: ['user_code'],
  properties: { user_code: { type: 'string' } },
} as const;

const REDEEM_ERRORS = {
  pending: 'authorization_pending',
  expired: 'expired_token',
  unknown: 'access_denied',
} as const;

export function registerDevice(server: FastifyInstance, deps: ServerDeps): void {
  server.post('/device/code', { schema: { body: codeSchema } }, async (request, reply) => {
    const { hardware_id } = request.body as { hardware_id: string };
    deps.pairing.sweep(deps.now());
    const record = deps.pairing.create(hardware_id, deps.now());
    logger.info('pairing started', { hardwareId: hardware_id, userCode: record.userCode });

    return reply.send({
      device_code: record.deviceCode,
      user_code: record.userCode,
      verification_uri: deps.verificationUri,
      expires_in: record.expiresAt - deps.now(),
      interval: POLL_INTERVAL_SECONDS,
    });
  });

  server.post('/device/token', { schema: { body: tokenSchema } }, async (request, reply) => {
    const { device_code } = request.body as { device_code: string };
    const result = deps.pairing.redeem(device_code, deps.now());

    if (result.status === 'approved') {
      deps.tokens.registerDevice(result.accessToken, DEFAULT_IDENTITY);
      logger.info('device paired', { hardwareId: result.hardwareId });
      return reply.send({ access_token: result.accessToken, token_type: 'Bearer' });
    }
    return reply.code(400).send({ error: REDEEM_ERRORS[result.status] });
  });

  server.post('/pair/approve', { schema: { body: approveSchema } }, async (request, reply) => {
    const { user_code } = request.body as { user_code: string };
    const result = deps.pairing.approve(user_code, deps.now());
    if (result.status !== 'ok') return reply.code(400).send({ error: result.status });
    return reply.code(204).send();
  });
}
