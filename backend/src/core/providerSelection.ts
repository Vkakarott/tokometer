import { PROVIDER_IDS, type ProviderId } from './types.ts';

const isProviderId = (value: unknown): value is ProviderId =>
  PROVIDER_IDS.includes(value as ProviderId);

/**
 * Which providers each identity wants on its displays, chosen in the macOS
 * app. An identity that never chose sees every provider, so a backend running
 * without the app keeps its old behaviour.
 */
export class ProviderSelection {
  readonly #byIdentity = new Map<string, ReadonlySet<ProviderId>>();

  set(identity: string, providers: readonly ProviderId[]): void {
    this.#byIdentity.set(identity, new Set(providers));
  }

  /** Always in PROVIDER_IDS order, whatever order the app sent. */
  enabledFor(identity: string): ProviderId[] {
    const chosen = this.#byIdentity.get(identity);
    return chosen === undefined ? [...PROVIDER_IDS] : PROVIDER_IDS.filter((id) => chosen.has(id));
  }

  toJSON(): Record<string, ProviderId[]> {
    return Object.fromEntries([...this.#byIdentity].map(([identity, chosen]) => [identity, [...chosen]]));
  }

  static fromJSON(data: Record<string, unknown>): ProviderSelection {
    const selection = new ProviderSelection();
    for (const [identity, value] of Object.entries(data)) {
      if (Array.isArray(value)) selection.set(identity, value.filter(isProviderId));
    }
    return selection;
  }
}
