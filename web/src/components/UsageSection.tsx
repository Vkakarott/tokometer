import type { JSX } from 'react';
import { PROVIDER_ORDER, type ProvidersView } from '../api';
import { ProviderPanel } from './ProviderPanel';

type UsageSectionProps = {
  view: ProvidersView | null;
  failed: boolean;
  onRetry: () => void;
};

export function UsageSection({ view, failed, onRetry }: UsageSectionProps): JSX.Element {
  if (failed) {
    return (
      <section className="panel">
        <div className="error-state" role="alert">
          <span>Não foi possível consultar o backend.</span>
          <button className="button" type="button" onClick={onRetry}>
            Tentar novamente
          </button>
        </div>
      </section>
    );
  }

  if (view === null) {
    return (
      <section className="panel">
        <p className="loading-state" role="status">Carregando consumo...</p>
      </section>
    );
  }

  return (
    <div className="provider-panels">
      {PROVIDER_ORDER.map((provider) => (
        <ProviderPanel key={provider} provider={provider} usage={view.providers[provider]} />
      ))}
    </div>
  );
}
