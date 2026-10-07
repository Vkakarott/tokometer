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
  // hardwareId -> expiry of an approval given before the device asked for a code.
  readonly #preapprovals = new Map<string, number>();
  readonly #paired = new Set<string>();
  readonly #ttlSeconds: number;

  constructor(ttlSeconds: number) {
    this.#ttlSeconds = ttlSeconds;
  }

  create(hardwareId: string, nowSeconds: number): PairingRecord {
    const record: PairingRecord = {
      deviceCode: randomUUID(),
      userCode: generateUserCode(),
      hardwareId,
      approved: this.#consumePreapproval(hardwareId, nowSeconds),
      expiresAt: nowSeconds + this.#ttlSeconds,
    };
    this.#records.set(record.deviceCode, record);
    return record;
  }

  // Approval by whoever configured the board itself: it covers the codes this
  // hardware already holds and the next one it asks for.
  preapprove(hardwareId: string, nowSeconds: number): void {
    this.#paired.delete(hardwareId);
    this.#preapprovals.set(hardwareId, nowSeconds + this.#ttlSeconds);
    for (const record of this.#records.values()) {
      if (record.hardwareId === hardwareId && nowSeconds <= record.expiresAt) record.approved = true;
    }
  }

  // True once this hardware turned an approved code into a token.
  isPaired(hardwareId: string): boolean {
    return this.#paired.has(hardwareId);
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
    this.#preapprovals.delete(record.hardwareId);
    this.#paired.add(record.hardwareId);
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
    for (const [hardwareId, expiresAt] of this.#preapprovals) {
      if (nowSeconds > expiresAt) this.#preapprovals.delete(hardwareId);
    }
  }

  #consumePreapproval(hardwareId: string, nowSeconds: number): boolean {
    const expiresAt = this.#preapprovals.get(hardwareId);
    if (expiresAt === undefined) return false;
    this.#preapprovals.delete(hardwareId);
    return nowSeconds <= expiresAt;
  }
}
