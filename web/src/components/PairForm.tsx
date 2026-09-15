import { useState } from 'react';
import type { JSX } from 'react';
import { approvePairing } from '../api';

type Status = { kind: 'idle' | 'sending' | 'done' } | { kind: 'error'; message: string };

const ERROR_MESSAGES: Record<string, string> = {
  expired: 'Código expirado. Aguarde alguns segundos: o display mostra um novo código sozinho.',
  unknown: 'Código não encontrado. Confira o que está no display.',
};

const normalizeCode = (value: string): string => value.replace(/[^A-Za-z0-9]/g, '').toUpperCase();

const formatCode = (value: string): string => {
  const normalized = normalizeCode(value).slice(0, 8);
  return normalized.length > 4 ? `${normalized.slice(0, 4)}-${normalized.slice(4)}` : normalized;
};

export function PairForm(): JSX.Element {
  const [code, setCode] = useState('');
  const [status, setStatus] = useState<Status>({ kind: 'idle' });

  const submit = async (event: React.FormEvent): Promise<void> => {
    event.preventDefault();
    setStatus({ kind: 'sending' });
    try {
      await approvePairing(normalizeCode(code));
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
        onChange={(event) => {
          setCode(formatCode(event.target.value));
          setStatus({ kind: 'idle' });
        }}
        placeholder="K7QM-3F9A"
        autoComplete="off"
        inputMode="text"
        maxLength={9}
        spellCheck={false}
        aria-describedby="pair-help pair-status"
      />
      <p className="pair-help" id="pair-help">Use os oito caracteres exibidos na tela do dispositivo.</p>
      <button className="button" type="submit" disabled={status.kind === 'sending' || normalizeCode(code).length !== 8}>
        Aprovar
      </button>
      <p className={status.kind === 'done' ? 'form-status ok' : status.kind === 'error' ? 'form-status err' : 'form-status'} id="pair-status" aria-live="polite">
        {status.kind === 'done' && 'Dispositivo pareado com sucesso.'}
        {status.kind === 'error' && status.message}
      </p>
    </form>
  );
}
