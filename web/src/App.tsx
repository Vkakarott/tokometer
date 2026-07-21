import { useEffect, useState } from 'react';
import { fetchUsage, type UsageView } from './api';
import { UsageBars } from './components/UsageBars';
import { PairForm } from './components/PairForm';
import './App.css';

const POLL_MS = 15_000;

function App() {
  const [usage, setUsage] = useState<UsageView | null>(null);
  const [failed, setFailed] = useState(false);

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
  }, []);

  return (
    <main>
      <h1>Consumo da assinatura Claude</h1>
      {failed && <p className="err">Backend fora do ar.</p>}
      {usage !== null && <UsageBars usage={usage} />}
      <h2>Parear dispositivo</h2>
      <p>Digite o código que aparece no display.</p>
      <PairForm />
    </main>
  );
}

export default App;
