import { PROVIDER_IDS, type ProviderId, type Snapshot } from './types.ts';

type ProviderSnapshots = Partial<Record<ProviderId, Snapshot>>;

/** State saved before providers existed held one Claude snapshot per identity. */
const isLegacySnapshot = (value: unknown): value is Snapshot =>
  typeof value === 'object' && value !== null && Array.isArray((value as Snapshot).windows);

/**
 * Keyed by identity from day one, then by provider. Today every caller passes
 * "default"; when accounts arrive the key becomes a user id and nothing else moves.
 */
export class SnapshotStore {
  readonly #byIdentity = new Map<string, ProviderSnapshots>();

  put(identity: string, provider: ProviderId, snapshot: Snapshot): void {
    this.#byIdentity.set(identity, { ...this.#byIdentity.get(identity), [provider]: snapshot });
  }

  get(identity: string, provider: ProviderId): Snapshot | null {
    return this.#byIdentity.get(identity)?.[provider] ?? null;
  }

  toJSON(): Record<string, ProviderSnapshots> {
    return Object.fromEntries(this.#byIdentity);
  }

  static fromJSON(data: Record<string, unknown>): SnapshotStore {
    const store = new SnapshotStore();
    for (const [identity, value] of Object.entries(data)) {
      if (isLegacySnapshot(value)) {
        store.put(identity, 'claude', value);
        continue;
      }
      const byProvider = (value ?? {}) as ProviderSnapshots;
      for (const provider of PROVIDER_IDS) {
        const snapshot = byProvider[provider];
        if (snapshot !== undefined) store.put(identity, provider, snapshot);
      }
    }
    return store;
  }
}
