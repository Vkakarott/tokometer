import { randomBytes, randomUUID } from 'node:crypto';
import { generateUserCode, normalizeUserCode } from './userCode.ts';

export type PairingRecord = {
  deviceCode: string;
  userCode: string;
  hardwareId: string;
  approved: boolean;
  expiresAt: number;
};

export type ApproveResult = { status: 'ok' | 'expired' | 'unknown' };

export type RedeemResult =
  | { status: 'approved'; accessToken: string; hardwareId: string }
  | { status: 'pending' }
  | { status: 'expired' }
  | { status: 'unknown' };

export class PairingStore {
  readonly #records = new Map<string, PairingRecord>();
  readonly #ttlSeconds: number;

  constructor(ttlSeconds: number) {
    this.#ttlSeconds = ttlSeconds;
  }

  create(hardwareId: string, nowSeconds: number): PairingRecord {
    const record: PairingRecord = {
      deviceCode: randomUUID(),
      userCode: generateUserCode(),
      hardwareId,
      approved: false,
      expiresAt: nowSeconds + this.#ttlSeconds,
    };
    this.#records.set(record.deviceCode, record);
    return record;
  }

  approve(userCode: string, nowSeconds: number): ApproveResult {
    const normalized = normalizeUserCode(userCode);
    const record = [...this.#records.values()].find((r) => r.userCode === normalized);
    if (record === undefined) return { status: 'unknown' };
    if (nowSeconds > record.expiresAt) return { status: 'expired' };
    record.approved = true;
    return { status: 'ok' };
  }

  redeem(deviceCode: string, nowSeconds: number): RedeemResult {
    const record = this.#records.get(deviceCode);
    if (record === undefined) return { status: 'unknown' };
    if (nowSeconds > record.expiresAt) return { status: 'expired' };
    if (!record.approved) return { status: 'pending' };

    // Single use: the code dies the moment it becomes a token.
    this.#records.delete(deviceCode);
    return {
      status: 'approved',
      accessToken: randomBytes(32).toString('base64url'),
      hardwareId: record.hardwareId,
    };
  }

  sweep(nowSeconds: number): void {
    for (const [deviceCode, record] of this.#records) {
      if (nowSeconds > record.expiresAt) this.#records.delete(deviceCode);
    }
  }
}
