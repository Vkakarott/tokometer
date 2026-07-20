import { readFile, rename, writeFile } from 'node:fs/promises';
import { logger } from '../logger.ts';

export async function loadJson<T>(path: string, fallback: T): Promise<T> {
  try {
    return JSON.parse(await readFile(path, 'utf8')) as T;
  } catch (error) {
    logger.warn('state file unreadable, starting empty', { path, error: String(error) });
    return fallback;
  }
}

export async function saveJson(path: string, data: unknown): Promise<void> {
  const temporary = `${path}.tmp`;
  await writeFile(temporary, JSON.stringify(data), 'utf8');
  await rename(temporary, path);
}
