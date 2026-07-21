export function requireBearer(header: string | undefined): string | null {
  if (header === undefined || !header.startsWith('Bearer ')) return null;
  const token = header.slice('Bearer '.length).trim();
  return token.length === 0 ? null : token;
}
