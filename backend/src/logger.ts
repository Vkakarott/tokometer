type Level = 'info' | 'warn' | 'error';

const emit = (level: Level, message: string, fields: Record<string, unknown> = {}): void => {
  const line = JSON.stringify({ level, message, at: new Date().toISOString(), ...fields });
  process.stderr.write(`${line}\n`);
};

export const logger = {
  info: (message: string, fields?: Record<string, unknown>): void => emit('info', message, fields),
  warn: (message: string, fields?: Record<string, unknown>): void => emit('warn', message, fields),
  error: (message: string, fields?: Record<string, unknown>): void => emit('error', message, fields),
};
