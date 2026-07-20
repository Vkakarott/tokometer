import { timingSafeEqual } from 'node:crypto';

export const DEFAULT_IDENTITY = 'default';

const constantTimeEquals = (a: string, b: string): boolean => {
  const left = Buffer.from(a);
  const right = Buffer.from(b);
  if (left.length !== right.length) return false;
  return timingSafeEqual(left, right);
};

export class TokenRegistry {
  readonly #deviceTokens = new Map<string, string>();
  readonly #collectorToken: string;

  constructor(collectorToken: string) {
    this.#collectorToken = collectorToken;
  }

  registerDevice(token: string, identity: string): void {
    this.#deviceTokens.set(token, identity);
  }

  resolveDevice(token: string): string | null {
    return this.#deviceTokens.get(token) ?? null;
  }

  resolveCollector(token: string): string | null {
    return constantTimeEquals(token, this.#collectorToken) ? DEFAULT_IDENTITY : null;
  }

  toJSON(): Record<string, string> {
    return Object.fromEntries(this.#deviceTokens);
  }

  static fromJSON(collectorToken: string, data: Record<string, string>): TokenRegistry {
    const registry = new TokenRegistry(collectorToken);
    for (const [token, identity] of Object.entries(data)) {
      registry.registerDevice(token, identity);
    }
    return registry;
  }
}
