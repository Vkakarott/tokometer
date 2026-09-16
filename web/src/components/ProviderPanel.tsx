import type { JSX } from 'react';
import type { ProviderId, UsageView } from '../api';
import { UsageBars } from './UsageBars';

const PROVIDER_NAMES: Record<ProviderId, string> = {
  claude: 'Claude',
  codex: 'Codex',
};

export function ProviderPanel({ provider, usage }: { provider: ProviderId; usage: UsageView }): JSX.Element {
  const name = PROVIDER_NAMES[provider];
  const titleId = `usage-title-${provider}`;

  return (
    <section className="panel" aria-labelledby={titleId}>
      <div className="panel-heading">
        <div>
          <h2 id={titleId}>{name}</h2>
          <p>Limites de 5 horas e 7 dias da assinatura.</p>
        </div>
        {usage.hasData && (
          <span className={usage.stale ? 'usage-status stale' : 'usage-status'}>
            {usage.stale ? 'Dado antigo' : 'Atualizado'}
          </span>
        )}
      </div>
      <UsageBars usage={usage} providerName={name} />
    </section>
  );
}
