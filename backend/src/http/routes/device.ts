import type { FastifyInstance } from 'fastify';
import { DEFAULT_IDENTITY } from '../../core/identity.ts';
import { logger } from '../../logger.ts';
import type { ServerDeps } from '../server.ts';

// Short on purpose: it is the delay between an approval and the display reacting.
const POLL_INTERVAL_SECONDS = 2;

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
  properties: { user_code: { type: 'string' }, hardware_id: { type: 'string', minLength: 1 } },
} as const;

const statusSchema = {
  type: 'object',
  required: ['hardware_id'],
  properties: { hardware_id: { type: 'string', minLength: 1 } },
} as const;

const isLoopback = (ip: string): boolean => ip === '127.0.0.1' || ip === '::1' || ip === '::ffff:127.0.0.1';

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
    const { user_code, hardware_id } = request.body as { user_code?: string; hardware_id?: string };
    if (hardware_id !== undefined) {
      // Nobody read a code off the screen here, so only this machine may vouch for the board.
      if (!isLoopback(request.ip)) return reply.code(403).send({ error: 'local_only' });
      deps.pairing.preapprove(hardware_id, deps.now());
      logger.info('pairing preapproved', { hardwareId: hardware_id });
      return reply.code(204).send();
    }
    if (user_code === undefined) return reply.code(400).send({ error: 'unknown' });
    const result = deps.pairing.approve(user_code, deps.now());
    if (result.status !== 'ok') return reply.code(400).send({ error: result.status });
    return reply.code(204).send();
  });

  server.get('/pair/status', { schema: { querystring: statusSchema } }, async (request, reply) => {
    if (!isLoopback(request.ip)) return reply.code(403).send({ error: 'local_only' });
    const { hardware_id } = request.query as { hardware_id: string };
    return reply.send({ paired: deps.pairing.isPaired(hardware_id) });
  });
}
