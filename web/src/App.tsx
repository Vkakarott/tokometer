import { useEffect, useState } from 'react';
import type { JSX } from 'react';
import { fetchUsage, type ProvidersView } from './api';
import { PairForm } from './components/PairForm';
import { UsageSection } from './components/UsageSection';
import './App.css';

const POLL_MS = 15_000;

function App(): JSX.Element {
  const [usage, setUsage] = useState<ProvidersView | null>(null);
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
          <p className="eyebrow">Assinaturas Claude e Codex</p>
          <h1>Consumo atual</h1>
          <p>Lido direto de cada conta. Atualização automática a cada 15 segundos.</p>
        </section>
        <UsageSection view={usage} failed={failed} onRetry={() => setRefreshKey((key) => key + 1)} />
      </main>
    </div>
  );
}

export default App;
