import { useEffect, useState } from 'react';
import type { JSX } from 'react';
import { fetchUsage, type UsageView } from './api';
import { UsageBars } from './components/UsageBars';
import { PairForm } from './components/PairForm';
import './App.css';

const POLL_MS = 15_000;

function App(): JSX.Element {
  const [usage, setUsage] = useState<UsageView | null>(null);
  const [failed, setFailed] = useState(false);
  const [refreshKey, setRefreshKey] = useState(0);
  const isPairingPage = window.location.pathname.replace(/\/+$/, '') === '/pair';

  useEffect(() => {
    let active = true;
    const load = async (): Promise<void> => {
      try {
        const next = await fetchUsage();
        if (active) {
          setUsage(next);
          setFailed(false);
        }
      } catch {
        if (active) setFailed(true);
      }
    };

    void load();
    const timer = setInterval(() => void load(), POLL_MS);
    return () => {
      active = false;
      clearInterval(timer);
    };
  }, [refreshKey]);

  if (isPairingPage) {
    return (
      <div className="app-shell">
        <header className="app-header">
          <a className="brand" href="/">tokEsp</a>
          <a className="nav-link" href="/">Ver consumo</a>
        </header>
        <main className="pair-layout">
          <section className="page-heading">
            <p className="eyebrow">Pareamento</p>
            <h1>Conectar display</h1>
            <p>Digite o código que aparece no OLED para autorizar este dispositivo.</p>
          </section>
          <section className="panel" aria-labelledby="pair-title">
            <div className="panel-heading">
              <div>
                <h2 id="pair-title">Código do dispositivo</h2>
                <p>O código expira em cinco minutos e só pode ser usado uma vez.</p>
              </div>
            </div>
            <PairForm />
          </section>
        </main>
      </div>
    );
  }

  return (
    <div className="app-shell">
      <header className="app-header">
        <a className="brand" href="/">tokEsp</a>
        <a className="nav-link" href="/pair">Parear display</a>
      </header>
      <main>
        <section className="page-heading">
          <p className="eyebrow">Assinatura Claude</p>
          <h1>Consumo atual</h1>
          <p>A leitura vem da última resposta recebida pelo Claude Code.</p>
        </section>
        <section className="panel" aria-labelledby="usage-title">
          <div className="panel-heading">
            <div>
              <h2 id="usage-title">Limites da conta</h2>
              <p>Atualização automática a cada 15 segundos.</p>
            </div>
            {usage !== null && (
              <span className={usage.stale ? 'usage-status stale' : 'usage-status'}>
                {usage.stale ? 'Dado antigo' : 'Atualizado'}
              </span>
            )}
          </div>
          {failed ? (
            <div className="error-state" role="alert">
              <span>Não foi possível consultar o backend.</span>
              <button className="button" type="button" onClick={() => setRefreshKey((key) => key + 1)}>
                Tentar novamente
              </button>
            </div>
          ) : usage === null ? (
            <p className="loading-state" role="status">Carregando consumo...</p>
          ) : (
            <UsageBars usage={usage} />
          )}
        </section>
      </main>
    </div>
  );
}

export default App;
