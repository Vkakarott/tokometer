import { useState } from 'react';
import { approvePairing } from '../api';

type Status = { kind: 'idle' | 'sending' | 'done' } | { kind: 'error'; message: string };

const ERROR_MESSAGES: Record<string, string> = {
  expired: 'Código expirado. Reinicie o dispositivo para gerar um novo.',
  unknown: 'Código não encontrado. Confira o que está no display.',
};

export function PairForm() {
  const [code, setCode] = useState('');
  const [status, setStatus] = useState<Status>({ kind: 'idle' });

  const submit = async (event: React.FormEvent): Promise<void> => {
    event.preventDefault();
    setStatus({ kind: 'sending' });
    try {
      await approvePairing(code);
      setStatus({ kind: 'done' });
      setCode('');
    } catch (error) {
      const key = error instanceof Error ? error.message : 'unknown';
      setStatus({ kind: 'error', message: ERROR_MESSAGES[key] ?? 'Não foi possível aprovar.' });
    }
  };

  return (
    <form onSubmit={submit} className="pair">
      <label htmlFor="user-code">Código do dispositivo</label>
      <input
        id="user-code"
        value={code}
        onChange={(event) => setCode(event.target.value)}
        placeholder="K7QM-3F9A"
        autoComplete="off"
        spellCheck={false}
      />
      <button type="submit" disabled={status.kind === 'sending' || code.length < 8}>
        Aprovar
      </button>
      {status.kind === 'done' && <p className="ok">Dispositivo pareado.</p>}
      {status.kind === 'error' && <p className="err">{status.message}</p>}
    </form>
  );
}
