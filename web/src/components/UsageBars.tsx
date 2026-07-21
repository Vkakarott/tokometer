import type { UsageView, WindowView } from '../api';

const WINDOW_LABELS: Record<WindowView['id'], string> = {
  five_hour: 'Últimas 5 horas',
  seven_day: 'Últimos 7 dias',
};

const formatReset = (resetsAt: number): string => {
  const minutes = Math.round((resetsAt * 1000 - Date.now()) / 60_000);
  if (minutes <= 0) return 'reiniciando';
  if (minutes < 60) return `reseta em ${minutes}min`;
  return `reseta em ${Math.round(minutes / 60)}h`;
};

const formatAge = (ageSeconds: number): string => {
  if (ageSeconds < 60) return 'agora há pouco';
  if (ageSeconds < 3600) return `há ${Math.round(ageSeconds / 60)}min`;
  return `há ${Math.round(ageSeconds / 3600)}h`;
};

function Bar({ window }: { window: WindowView }) {
  const unknown = window.usedPercentage === null;
  return (
    <div className="bar">
      <div className="bar-head">
        <span>{WINDOW_LABELS[window.id]}</span>
        <span>{unknown ? '—' : `${Math.round(window.usedPercentage ?? 0)}%`}</span>
      </div>
      <div className="bar-track">
        <div className="bar-fill" style={{ width: unknown ? '0%' : `${window.usedPercentage}%` }} />
      </div>
      <small>{unknown ? 'Janela reiniciou, aguardando novo dado' : formatReset(window.resetsAt)}</small>
    </div>
  );
}

export function UsageBars({ usage }: { usage: UsageView }) {
  if (!usage.hasData) {
    return (
      <p className="empty">
        Sem dados ainda. Abra o Claude Code e envie uma mensagem — o consumo aparece
        depois da primeira resposta. Requer assinatura Pro ou Max.
      </p>
    );
  }

  return (
    <section className={usage.stale ? 'usage stale' : 'usage'}>
      {usage.windows.map((window) => (
        <Bar key={window.id} window={window} />
      ))}
      <small className="age">
        Atualizado {formatAge(usage.ageSeconds)}
        {usage.stale ? ' — o Claude Code pode estar fechado' : ''}
      </small>
    </section>
  );
}
