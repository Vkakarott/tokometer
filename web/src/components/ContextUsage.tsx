import type { JSX } from 'react';
import type { ContextUsage } from '../api';

const formatNumber = (value: number): string => new Intl.NumberFormat('pt-BR').format(Math.round(value));

const percentage = (value: number): number => Math.max(0, Math.min(100, value));

export function ContextUsage({ context }: { context: ContextUsage }): JSX.Element {
  const totalTokens = context.inputTokens + context.outputTokens;
  const usedPercentage = percentage(context.usedPercentage);

  return (
    <section className="context-usage" aria-labelledby="context-title">
      <div className="context-heading">
        <div>
          <h3 id="context-title">Contexto atual</h3>
          <p>Tokens na janela da sessão atual, não no limite da assinatura.</p>
        </div>
        <strong>{formatNumber(totalTokens)} tokens</strong>
      </div>
      <div
        className="context-track"
        role="progressbar"
        aria-label="Janela de contexto usada"
        aria-valuemin={0}
        aria-valuemax={100}
        aria-valuenow={usedPercentage}
        aria-valuetext={`${Math.round(usedPercentage)}% da janela de contexto usada`}
      >
        <div className="context-fill" style={{ width: `${usedPercentage}%` }} />
      </div>
      <dl className="context-details">
        <div>
          <dt>Entrada</dt>
          <dd>{formatNumber(context.inputTokens)}</dd>
        </div>
        <div>
          <dt>Saída</dt>
          <dd>{formatNumber(context.outputTokens)}</dd>
        </div>
        <div>
          <dt>Janela</dt>
          <dd>{formatNumber(context.windowSize)}</dd>
        </div>
      </dl>
    </section>
  );
}
