import type { Snapshot } from './types.ts';

/**
 * Keyed by identity from day one. Today every caller passes "default";
 * when accounts arrive the key becomes a user id and nothing else moves.
 */
export class SnapshotStore {
  readonly #byIdentity = new Map<string, Snapshot>();

  put(identity: string, snapshot: Snapshot): void {
    this.#byIdentity.set(identity, snapshot);
  }

  get(identity: string): Snapshot | null {
    return this.#byIdentity.get(identity) ?? null;
  }

  toJSON(): Record<string, Snapshot> {
    return Object.fromEntries(this.#byIdentity);
  }

  static fromJSON(data: Record<string, Snapshot>): SnapshotStore {
    const store = new SnapshotStore();
    for (const [identity, snapshot] of Object.entries(data)) {
      store.put(identity, snapshot);
    }
    return store;
  }
}
