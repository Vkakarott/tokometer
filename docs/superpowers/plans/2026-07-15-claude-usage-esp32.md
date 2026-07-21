# tokEsp — Backend, collector e web — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Entregar o backend, o collector e o `/web` que expõem o consumo da assinatura Claude Pro/Max, com um endpoint `/usage` autenticado pronto para o ESP32 consumir.

**Escopo:** Este plano **não toca em `firmware/`**. O ESP32 é consumidor do contrato, não entregável aqui — por isso a Task 9 documenta o contrato e verifica o fluxo do device via `curl`. O spec (`docs/superpowers/specs/2026-07-15-claude-usage-esp32-design.md`) descreve o produto completo, incluindo firmware; este plano cobre a primeira fatia.

**Architecture:** O dado é empurrado, não buscado. Um script de statusline do Claude Code extrai `rate_limits` do JSON que recebe no stdin e faz POST em background para um backend Node. O backend guarda o último snapshot e o serve a dois consumidores: o `/web` e o ESP32, este último autenticado via OAuth Device Authorization Grant (RFC 8628) — o device mostra um código, o humano aprova no `/web`, o token vai direto para o device.

**Tech Stack:** Node 25 + TypeScript (type stripping nativo, `node:test` embutido), Fastify, shell + jq, Vite + React.

## Global Constraints

- **TypeScript:** `strict: true` obrigatório em todo tsconfig. Nunca `var`. `const` por padrão, `let` só quando há reatribuição. Tipo de retorno explícito em funções exportadas.
- **Type stripping:** o backend roda `.ts` direto no Node, sem build. Isso proíbe `enum`, `namespace` e decorators — use union types de string. Só sintaxe apagável.
- **Funções < 30 linhas.** Se passar, quebre antes de terminar.
- **Sem `console.log` em código de produção.** Use um logger.
- **Idioma:** texto de UI, labels e mensagens ao usuário em **pt-BR**. Código, nomes de variáveis, funções e comentários em **inglês**.
- **Commits:** Conventional Commits (`feat:`, `fix:`, `chore:`, `docs:`, `refactor:`, `test:`). Mensagem em inglês, imperativo presente. **Nunca adicionar `Co-Authored-By`.** Commit após cada feature completa.
- **Rodar os testes antes de todo commit.** Não commitar com teste falhando.
- **Não modificar nada em `firmware/` nem em `platformio.ini`.**
- **Versão mínima do Claude Code:** 2.1.92 (o campo `rate_limits` não existe abaixo disso). Ambiente atual: 2.1.126.
- **Este projeto ainda não é um repositório git.** A Task 1 inicializa.

## Alfabeto do `user_code` (valor exato, usado em várias tasks)

```
ABCDEFGHJKMNPQRSTVWXYZ23456789
```

30 símbolos. Removidos `I`, `L`, `O`, `U`, `0`, `1` por ambiguidade visual num OLED. 8 caracteres, exibidos em dois grupos de 4 separados por hífen (`K7QM-3F9A`). O hífen é apresentação: nunca armazene nem compare com ele.

## File Structure

```
backend/
├── package.json
├── tsconfig.json
└── src/
    ├── core/                    ← puro, sem I/O, sem Fastify
    │   ├── types.ts             — Snapshot, UsageWindow, UsageView
    │   ├── freshness.ts         — buildUsageView(): as 3 regras de frescor
    │   ├── userCode.ts          — generateUserCode(), normalizeUserCode()
    │   ├── pairing.ts           — PairingStore: create/approve/redeem
    │   ├── snapshots.ts         — SnapshotStore: mapa por identity
    │   └── identity.ts          — resolveDeviceToken/resolveCollectorToken
    ├── store/
    │   └── jsonFile.ts          — persistência em arquivo
    ├── http/
    │   ├── server.ts            — wiring Fastify
    │   └── routes/
    │       ├── ingest.ts
    │       ├── usage.ts
    │       └── device.ts
    ├── logger.ts
    └── main.ts
backend/test/*.test.ts           ← node --test

collector/
├── payload.jq                   — transformação pura, testável sem curl
├── statusline.sh                — orquestra: extrai, POSTa em background, imprime
├── config.example.sh
├── README.md
└── test/payload.test.sh

web/src/
├── api.ts
├── components/UsageBars.tsx
└── components/PairForm.tsx

docs/
└── device-api.md                ← contrato para quem escrever o firmware
```

**Fronteira que não pode vazar:** `core/` nunca importa Fastify, `node:fs` ou `node:http`. É isso que mantém a hospedagem em aberto e os testes puros. Se um arquivo de `core/` precisar de I/O, o desenho está errado — passe o dado por parâmetro.

---

### Task 1: Backend scaffold + tipos + lógica de frescor

O coração do produto. As três regras de frescor são o que impede o display de mentir, então são a primeira coisa a existir e a ser testada.

**Files:**
- Create: `backend/package.json`, `backend/tsconfig.json`, `backend/src/core/types.ts`, `backend/src/core/freshness.ts`
- Test: `backend/test/freshness.test.ts`

**Interfaces:**
- Consumes: nada (primeira task)
- Produces: `UsageWindow`, `Snapshot`, `UsageView`, `WindowView`, `WindowId`, `buildUsageView(snapshot: Snapshot | null, nowSeconds: number, staleAfterSeconds: number): UsageView`

- [ ] **Step 1: Inicializar o repositório git**

O projeto tem `.gitignore` mas nunca teve `git init`. O `.gitignore` já protege `firmware/src/config/secrets.h`.

```bash
cd /Users/lucas/Documents/Projetos/Pessoal/harware/tokEsp
git init
git add .gitignore docs/
git commit -m "docs: add design spec and implementation plan"
```

- [ ] **Step 2: Criar `backend/package.json`**

Sem dependência de teste: o Node 25 traz `node:test`. Fastify entra na Task 5.

```json
{
  "name": "tokesp-backend",
  "private": true,
  "version": "0.0.0",
  "type": "module",
  "scripts": {
    "test": "node --test 'test/**/*.test.ts'",
    "dev": "node --watch src/main.ts"
  }
}
```

- [ ] **Step 3: Criar `backend/tsconfig.json`**

`strict: true` conforme Global Constraints. `noEmit` porque o Node executa o `.ts` direto — o tsc aqui é só type-checker.

```json
{
  "compilerOptions": {
    "target": "es2023",
    "lib": ["ES2023"],
    "module": "esnext",
    "moduleResolution": "bundler",
    "allowImportingTsExtensions": true,
    "verbatimModuleSyntax": true,
    "erasableSyntaxOnly": true,
    "noEmit": true,
    "strict": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "noFallthroughCasesInSwitch": true,
    "skipLibCheck": true
  },
  "include": ["src", "test"]
}
```

`erasableSyntaxOnly: true` faz o tsc rejeitar `enum`/`namespace` — o compilador passa a garantir a regra de type stripping em vez de você lembrar dela.

- [ ] **Step 4: Escrever o teste que falha**

Cobre as três regras de frescor do spec. Note o teste de rollover: é o que impede o display de mostrar 23% quando a janela já virou.

Criar `backend/test/freshness.test.ts`:

```ts
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { buildUsageView } from '../src/core/freshness.ts';
import type { Snapshot } from '../src/core/types.ts';

const NOW = 1_800_000_000;

const snapshot = (overrides: Partial<Snapshot> = {}): Snapshot => ({
  provider: 'claude-code',
  source: 'laptop',
  observedAt: NOW,
  windows: [
    { id: 'five_hour', usedPercentage: 23.5, resetsAt: NOW + 3600 },
    { id: 'seven_day', usedPercentage: 41.2, resetsAt: NOW + 86_400 },
  ],
  ...overrides,
});

test('reports fresh data with both windows known', () => {
  const view = buildUsageView(snapshot(), NOW, 900);
  assert.equal(view.hasData, true);
  assert.equal(view.stale, false);
  assert.equal(view.ageSeconds, 0);
  assert.equal(view.windows[0]?.usedPercentage, 23.5);
});

test('marks a rolled-over window as unknown instead of showing the old number', () => {
  const view = buildUsageView(snapshot(), NOW + 7200, 999_999);
  const fiveHour = view.windows.find((w) => w.id === 'five_hour');
  const sevenDay = view.windows.find((w) => w.id === 'seven_day');
  assert.equal(fiveHour?.usedPercentage, null, 'window reset, old value is garbage');
  assert.equal(sevenDay?.usedPercentage, 41.2, 'rollover is per-window, not global');
});

test('flags data older than the stale threshold', () => {
  const view = buildUsageView(snapshot(), NOW + 1000, 900);
  assert.equal(view.stale, true);
  assert.equal(view.ageSeconds, 1000);
});

test('reports no data when no snapshot was ever ingested', () => {
  const view = buildUsageView(null, NOW, 900);
  assert.equal(view.hasData, false);
  assert.deepEqual(view.windows, []);
});

test('reports no data when Claude Code has not answered yet', () => {
  const view = buildUsageView(snapshot({ windows: [] }), NOW, 900);
  assert.equal(view.hasData, false, 'empty windows is not the same as 0%');
});

test('never reports a negative age when clocks disagree', () => {
  const view = buildUsageView(snapshot(), NOW - 50, 900);
  assert.equal(view.ageSeconds, 0);
});
```

- [ ] **Step 5: Rodar o teste e verificar que falha**

```bash
cd backend && npm test
```

Esperado: FAIL — `Cannot find module '../src/core/freshness.ts'`.

- [ ] **Step 6: Criar `backend/src/core/types.ts`**

```ts
export type WindowId = 'five_hour' | 'seven_day';

export type UsageWindow = {
  id: WindowId;
  usedPercentage: number;
  resetsAt: number;
};

export type Snapshot = {
  provider: string;
  source: string;
  observedAt: number;
  windows: UsageWindow[];
};

/** usedPercentage is null when the window already reset and the stored value is meaningless. */
export type WindowView = {
  id: WindowId;
  usedPercentage: number | null;
  resetsAt: number;
};

export type UsageView = {
  windows: WindowView[];
  ageSeconds: number;
  stale: boolean;
  hasData: boolean;
};
```

- [ ] **Step 7: Criar `backend/src/core/freshness.ts`**

```ts
import type { Snapshot, UsageView, WindowView } from './types.ts';

const viewWindow = (window: Snapshot['windows'][number], nowSeconds: number): WindowView => ({
  id: window.id,
  // The window already reset: the stored percentage describes a window that no
  // longer exists. Report unknown rather than a confidently wrong number.
  usedPercentage: nowSeconds > window.resetsAt ? null : window.usedPercentage,
  resetsAt: window.resetsAt,
});

export function buildUsageView(
  snapshot: Snapshot | null,
  nowSeconds: number,
  staleAfterSeconds: number,
): UsageView {
  if (snapshot === null || snapshot.windows.length === 0) {
    return { windows: [], ageSeconds: 0, stale: false, hasData: false };
  }

  const ageSeconds = Math.max(0, nowSeconds - snapshot.observedAt);

  return {
    windows: snapshot.windows.map((window) => viewWindow(window, nowSeconds)),
    ageSeconds,
    stale: ageSeconds > staleAfterSeconds,
    hasData: true,
  };
}
```

- [ ] **Step 8: Rodar os testes e verificar que passam**

```bash
cd backend && npm test
```

Esperado: `# pass 6`, `# fail 0`.

- [ ] **Step 9: Commit**

```bash
git add backend/package.json backend/tsconfig.json backend/src/core backend/test
git commit -m "feat: add usage freshness model with rollover detection"
```

---

### Task 2: Geração e normalização do `user_code`

**Files:**
- Create: `backend/src/core/userCode.ts`
- Test: `backend/test/userCode.test.ts`

**Interfaces:**
- Consumes: nada
- Produces: `generateUserCode(): string` (8 chars, sem hífen), `normalizeUserCode(input: string): string` (uppercase, remove não-alfabéticos)

O backend nunca formata o código com hífen: ele armazena e compara a forma crua, e quem exibe (o device) insere o hífen. `normalizeUserCode` existe justamente para aceitar de volta o que o humano digitou com hífen.

- [ ] **Step 1: Escrever o teste que falha**

Criar `backend/test/userCode.test.ts`:

```ts
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { generateUserCode, normalizeUserCode } from '../src/core/userCode.ts';

const ALPHABET = 'ABCDEFGHJKMNPQRSTVWXYZ23456789';

test('generates an 8-character code from the unambiguous alphabet', () => {
  for (let i = 0; i < 200; i++) {
    const code = generateUserCode();
    assert.equal(code.length, 8);
    for (const char of code) {
      assert.ok(ALPHABET.includes(char), `"${char}" is not in the alphabet`);
    }
  }
});

test('never emits visually ambiguous characters', () => {
  const codes = Array.from({ length: 500 }, () => generateUserCode()).join('');
  for (const banned of ['I', 'L', 'O', 'U', '0', '1']) {
    assert.ok(!codes.includes(banned), `"${banned}" is ambiguous on the OLED`);
  }
});

test('does not repeat within a small sample', () => {
  const codes = new Set(Array.from({ length: 500 }, () => generateUserCode()));
  assert.equal(codes.size, 500);
});

test('normalizes what a human types back to storage form', () => {
  assert.equal(normalizeUserCode('k7qm-3f9a'), 'K7QM3F9A');
  assert.equal(normalizeUserCode(' K7QM 3F9A '), 'K7QM3F9A');
  assert.equal(normalizeUserCode('K7QM3F9A'), 'K7QM3F9A');
});
```

- [ ] **Step 2: Rodar o teste e verificar que falha**

```bash
cd backend && npm test
```

Esperado: FAIL — `Cannot find module '../src/core/userCode.ts'`.

- [ ] **Step 3: Criar `backend/src/core/userCode.ts`**

`randomInt` do `node:crypto` é uniforme e evita o viés de módulo que `Math.random() * 30` introduziria.

```ts
import { randomInt } from 'node:crypto';

const ALPHABET = 'ABCDEFGHJKMNPQRSTVWXYZ23456789';
const CODE_LENGTH = 8;

export function generateUserCode(): string {
  let code = '';
  for (let i = 0; i < CODE_LENGTH; i++) {
    code += ALPHABET[randomInt(ALPHABET.length)];
  }
  return code;
}

/** Accepts the hyphenated form a human reads off the display. */
export function normalizeUserCode(input: string): string {
  return input.toUpperCase().replace(/[^A-Z0-9]/g, '');
}
```

- [ ] **Step 4: Rodar os testes e verificar que passam**

```bash
cd backend && npm test
```

Esperado: `# pass 10`, `# fail 0`.

- [ ] **Step 5: Commit**

```bash
git add backend/src/core/userCode.ts backend/test/userCode.test.ts
git commit -m "feat: add unambiguous user code generation"
```

---

### Task 3: PairingStore (device flow, RFC 8628)

**Files:**
- Create: `backend/src/core/pairing.ts`
- Test: `backend/test/pairing.test.ts`

**Interfaces:**
- Consumes: `generateUserCode`, `normalizeUserCode` de `core/userCode.ts`
- Produces: `PairingStore` com `create(hardwareId: string, nowSeconds: number): PairingRecord`, `approve(userCode: string, nowSeconds: number): ApproveResult`, `redeem(deviceCode: string, nowSeconds: number): RedeemResult`, `sweep(nowSeconds: number): void`; tipos `PairingRecord`, `ApproveResult`, `RedeemResult`

- [ ] **Step 1: Escrever o teste que falha**

Criar `backend/test/pairing.test.ts`:

```ts
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { PairingStore } from '../src/core/pairing.ts';

const NOW = 1_800_000_000;
const TTL = 300;

test('redeem stays pending until a human approves', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  assert.equal(store.redeem(record.deviceCode, NOW).status, 'pending');
});

test('redeem returns a token once approved', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  assert.equal(store.approve(record.userCode, NOW).status, 'ok');

  const result = store.redeem(record.deviceCode, NOW);
  assert.equal(result.status, 'approved');
  assert.ok(result.status === 'approved' && result.accessToken.length >= 32);
});

test('a code is single-use: the second redeem no longer knows it', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  store.approve(record.userCode, NOW);
  store.redeem(record.deviceCode, NOW);
  assert.equal(store.redeem(record.deviceCode, NOW).status, 'unknown');
});

test('expires after the TTL', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  assert.equal(store.redeem(record.deviceCode, NOW + TTL + 1).status, 'expired');
});

test('refuses to approve an expired code', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  assert.equal(store.approve(record.userCode, NOW + TTL + 1).status, 'expired');
});

test('refuses to approve a code that does not exist', () => {
  const store = new PairingStore(TTL);
  assert.equal(store.approve('ZZZZZZZZ', NOW).status, 'unknown');
});

test('approve accepts the hyphenated form a human reads off the OLED', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  const typed = `${record.userCode.slice(0, 4)}-${record.userCode.slice(4)}`.toLowerCase();
  assert.equal(store.approve(typed, NOW).status, 'ok');
});

test('redeem reports unknown for an unrecognised device code', () => {
  const store = new PairingStore(TTL);
  assert.equal(store.redeem('nope', NOW).status, 'unknown');
});

test('sweep drops expired records', () => {
  const store = new PairingStore(TTL);
  const record = store.create('esp32-abc', NOW);
  store.sweep(NOW + TTL + 1);
  assert.equal(store.redeem(record.deviceCode, NOW).status, 'unknown');
});

test('issues a distinct token per device', () => {
  const store = new PairingStore(TTL);
  const first = store.create('esp32-a', NOW);
  const second = store.create('esp32-b', NOW);
  store.approve(first.userCode, NOW);
  store.approve(second.userCode, NOW);

  const a = store.redeem(first.deviceCode, NOW);
  const b = store.redeem(second.deviceCode, NOW);
  assert.ok(a.status === 'approved' && b.status === 'approved');
  assert.notEqual(
    a.status === 'approved' ? a.accessToken : '',
    b.status === 'approved' ? b.accessToken : '',
  );
});
```

- [ ] **Step 2: Rodar o teste e verificar que falha**

```bash
cd backend && npm test
```

Esperado: FAIL — `Cannot find module '../src/core/pairing.ts'`.

- [ ] **Step 3: Criar `backend/src/core/pairing.ts`**

```ts
import { randomBytes, randomUUID } from 'node:crypto';
import { generateUserCode, normalizeUserCode } from './userCode.ts';

export type PairingRecord = {
  deviceCode: string;
  userCode: string;
  hardwareId: string;
  approved: boolean;
  expiresAt: number;
};

export type ApproveResult = { status: 'ok' | 'expired' | 'unknown' };

export type RedeemResult =
  | { status: 'approved'; accessToken: string; hardwareId: string }
  | { status: 'pending' }
  | { status: 'expired' }
  | { status: 'unknown' };

export class PairingStore {
  readonly #records = new Map<string, PairingRecord>();
  readonly #ttlSeconds: number;

  constructor(ttlSeconds: number) {
    this.#ttlSeconds = ttlSeconds;
  }

  create(hardwareId: string, nowSeconds: number): PairingRecord {
    const record: PairingRecord = {
      deviceCode: randomUUID(),
      userCode: generateUserCode(),
      hardwareId,
      approved: false,
      expiresAt: nowSeconds + this.#ttlSeconds,
    };
    this.#records.set(record.deviceCode, record);
    return record;
  }

  approve(userCode: string, nowSeconds: number): ApproveResult {
    const normalized = normalizeUserCode(userCode);
    const record = [...this.#records.values()].find((r) => r.userCode === normalized);
    if (record === undefined) return { status: 'unknown' };
    if (nowSeconds > record.expiresAt) return { status: 'expired' };
    record.approved = true;
    return { status: 'ok' };
  }

  redeem(deviceCode: string, nowSeconds: number): RedeemResult {
    const record = this.#records.get(deviceCode);
    if (record === undefined) return { status: 'unknown' };
    if (nowSeconds > record.expiresAt) return { status: 'expired' };
    if (!record.approved) return { status: 'pending' };

    // Single use: the code dies the moment it becomes a token.
    this.#records.delete(deviceCode);
    return {
      status: 'approved',
      accessToken: randomBytes(32).toString('base64url'),
      hardwareId: record.hardwareId,
    };
  }

  sweep(nowSeconds: number): void {
    for (const [deviceCode, record] of this.#records) {
      if (nowSeconds > record.expiresAt) this.#records.delete(deviceCode);
    }
  }
}
```

- [ ] **Step 4: Rodar os testes e verificar que passam**

```bash
cd backend && npm test
```

Esperado: `# pass 20`, `# fail 0`.

- [ ] **Step 5: Commit**

```bash
git add backend/src/core/pairing.ts backend/test/pairing.test.ts
git commit -m "feat: add device authorization grant pairing store"
```

---

### Task 4: SnapshotStore, identidade e persistência

A dobradiça de multi-tenancy do spec: o snapshot já nasce indexado por `identity`, hoje sempre `"default"`.

**Files:**
- Create: `backend/src/core/snapshots.ts`, `backend/src/core/identity.ts`, `backend/src/store/jsonFile.ts`, `backend/src/logger.ts`
- Test: `backend/test/snapshots.test.ts`, `backend/test/identity.test.ts`

**Interfaces:**
- Consumes: `Snapshot` de `core/types.ts`
- Produces: `SnapshotStore` com `put(identity: string, snapshot: Snapshot): void`, `get(identity: string): Snapshot | null`, `toJSON(): Record<string, Snapshot>`, `static fromJSON(data: Record<string, Snapshot>): SnapshotStore`; `TokenRegistry` com `registerDevice(token: string, identity: string): void`, `resolveDevice(token: string): string | null`, `resolveCollector(token: string): string | null`, `toJSON(): Record<string, string>`, `static fromJSON(collectorToken: string, data: Record<string, string>): TokenRegistry`; constante `DEFAULT_IDENTITY`; `loadJson<T>(path: string, fallback: T): Promise<T>` e `saveJson(path: string, data: unknown): Promise<void>`; `logger` com `info`/`warn`/`error`

- [ ] **Step 1: Escrever os testes que falham**

Criar `backend/test/snapshots.test.ts`:

```ts
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { SnapshotStore } from '../src/core/snapshots.ts';
import type { Snapshot } from '../src/core/types.ts';

const snapshot: Snapshot = {
  provider: 'claude-code',
  source: 'laptop',
  observedAt: 1_800_000_000,
  windows: [{ id: 'five_hour', usedPercentage: 10, resetsAt: 1_800_003_600 }],
};

test('returns null for an identity that never reported', () => {
  assert.equal(new SnapshotStore().get('default'), null);
});

test('stores and reads back per identity', () => {
  const store = new SnapshotStore();
  store.put('default', snapshot);
  assert.deepEqual(store.get('default'), snapshot);
  assert.equal(store.get('someone-else'), null);
});

test('last write wins: two machines report the same account', () => {
  const store = new SnapshotStore();
  store.put('default', snapshot);
  const newer: Snapshot = { ...snapshot, source: 'desktop', observedAt: snapshot.observedAt + 60 };
  store.put('default', newer);
  assert.equal(store.get('default')?.source, 'desktop');
});

test('survives a round trip through JSON', () => {
  const store = new SnapshotStore();
  store.put('default', snapshot);
  const restored = SnapshotStore.fromJSON(JSON.parse(JSON.stringify(store.toJSON())));
  assert.deepEqual(restored.get('default'), snapshot);
});
```

Criar `backend/test/identity.test.ts`:

```ts
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { TokenRegistry } from '../src/core/identity.ts';

test('resolves a registered device token to its identity', () => {
  const registry = new TokenRegistry('collector-secret');
  registry.registerDevice('device-token-abc', 'default');
  assert.equal(registry.resolveDevice('device-token-abc'), 'default');
});

test('rejects an unknown device token', () => {
  const registry = new TokenRegistry('collector-secret');
  assert.equal(registry.resolveDevice('nope'), null);
});

test('resolves the collector token to its identity', () => {
  const registry = new TokenRegistry('collector-secret');
  assert.equal(registry.resolveCollector('collector-secret'), 'default');
});

test('rejects a wrong collector token', () => {
  const registry = new TokenRegistry('collector-secret');
  assert.equal(registry.resolveCollector('wrong'), null);
});

test('rejects a collector token of a different length without throwing', () => {
  const registry = new TokenRegistry('collector-secret');
  assert.equal(registry.resolveCollector('x'), null);
});

test('survives a round trip through JSON', () => {
  const registry = new TokenRegistry('collector-secret');
  registry.registerDevice('device-token-abc', 'default');
  const restored = TokenRegistry.fromJSON(
    'collector-secret',
    JSON.parse(JSON.stringify(registry.toJSON())),
  );
  assert.equal(restored.resolveDevice('device-token-abc'), 'default');
});
```

- [ ] **Step 2: Rodar os testes e verificar que falham**

```bash
cd backend && npm test
```

Esperado: FAIL — `Cannot find module '../src/core/snapshots.ts'`.

- [ ] **Step 3: Criar `backend/src/core/snapshots.ts`**

```ts
import type { Snapshot } from './types.ts';

/**
 * Keyed by identity from day one. Today every caller passes "default";
 * when accounts arrive the key becomes a user id and nothing else moves.
 */
export class SnapshotStore {
  readonly #byIdentity = new Map<string, Snapshot>();

  put(identity: string, snapshot: Snapshot): void {
    this.#byIdentity.set(identity, snapshot);
  }

  get(identity: string): Snapshot | null {
    return this.#byIdentity.get(identity) ?? null;
  }

  toJSON(): Record<string, Snapshot> {
    return Object.fromEntries(this.#byIdentity);
  }

  static fromJSON(data: Record<string, Snapshot>): SnapshotStore {
    const store = new SnapshotStore();
    for (const [identity, snapshot] of Object.entries(data)) {
      store.put(identity, snapshot);
    }
    return store;
  }
}
```

- [ ] **Step 4: Criar `backend/src/core/identity.ts`**

`timingSafeEqual` exige buffers do mesmo tamanho — daí a checagem de length antes. Um token de tamanho diferente já é inválido, então retornar cedo não vaza nada útil.

```ts
import { timingSafeEqual } from 'node:crypto';

export const DEFAULT_IDENTITY = 'default';

const constantTimeEquals = (a: string, b: string): boolean => {
  const left = Buffer.from(a);
  const right = Buffer.from(b);
  if (left.length !== right.length) return false;
  return timingSafeEqual(left, right);
};

export class TokenRegistry {
  readonly #deviceTokens = new Map<string, string>();
  readonly #collectorToken: string;

  constructor(collectorToken: string) {
    this.#collectorToken = collectorToken;
  }

  registerDevice(token: string, identity: string): void {
    this.#deviceTokens.set(token, identity);
  }

  resolveDevice(token: string): string | null {
    return this.#deviceTokens.get(token) ?? null;
  }

  resolveCollector(token: string): string | null {
    return constantTimeEquals(token, this.#collectorToken) ? DEFAULT_IDENTITY : null;
  }

  toJSON(): Record<string, string> {
    return Object.fromEntries(this.#deviceTokens);
  }

  static fromJSON(collectorToken: string, data: Record<string, string>): TokenRegistry {
    const registry = new TokenRegistry(collectorToken);
    for (const [token, identity] of Object.entries(data)) {
      registry.registerDevice(token, identity);
    }
    return registry;
  }
}
```

- [ ] **Step 5: Criar `backend/src/logger.ts`**

Global Constraints proíbem `console.log` em produção. Este é o logger.

```ts
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
```

- [ ] **Step 6: Criar `backend/src/store/jsonFile.ts`**

Escrita atômica: grava num temporário e renomeia. Sem isso, um restart no meio da escrita corrompe o arquivo e o backend sobe vazio.

```ts
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
```

- [ ] **Step 7: Rodar os testes e verificar que passam**

```bash
cd backend && npm test
```

Esperado: `# pass 30`, `# fail 0`.

- [ ] **Step 8: Commit**

```bash
git add backend/src/core/snapshots.ts backend/src/core/identity.ts backend/src/store backend/src/logger.ts backend/test/snapshots.test.ts backend/test/identity.test.ts
git commit -m "feat: add snapshot store, token registry and json persistence"
```

---

### Task 5: Rotas HTTP — ingest, usage e device flow

Primeira task com I/O. Todo o `core/` já está testado; aqui é só o adaptador.

**Files:**
- Modify: `backend/package.json` (adiciona `fastify`)
- Create: `backend/src/http/server.ts`, `backend/src/http/auth.ts`, `backend/src/http/routes/ingest.ts`, `backend/src/http/routes/usage.ts`, `backend/src/http/routes/device.ts`
- Test: `backend/test/http.test.ts`

**Interfaces:**
- Consumes: `buildUsageView`, `SnapshotStore`, `TokenRegistry`, `PairingStore`, `DEFAULT_IDENTITY`, `logger`
- Produces: `buildServer(deps: ServerDeps): FastifyInstance`; tipo `ServerDeps = { snapshots: SnapshotStore; tokens: TokenRegistry; pairing: PairingStore; now: () => number; staleAfterSeconds: number; verificationUri: string }`; `requireBearer(header: string | undefined): string | null`

- [ ] **Step 1: Instalar Fastify**

```bash
cd backend && npm install fastify
```

- [ ] **Step 2: Escrever o teste que falha**

`buildServer` recebe `now` injetado — é isso que torna expiração testável sem `sleep`. Usa `server.inject()` do Fastify, sem abrir socket.

Criar `backend/test/http.test.ts`:

```ts
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { buildServer } from '../src/http/server.ts';
import { SnapshotStore } from '../src/core/snapshots.ts';
import { TokenRegistry } from '../src/core/identity.ts';
import { PairingStore } from '../src/core/pairing.ts';

const NOW = 1_800_000_000;

const build = (nowRef = { value: NOW }) =>
  buildServer({
    snapshots: new SnapshotStore(),
    tokens: new TokenRegistry('collector-secret'),
    pairing: new PairingStore(300),
    now: () => nowRef.value,
    staleAfterSeconds: 900,
    verificationUri: 'http://tok.local/pair',
  });

const payload = {
  provider: 'claude-code',
  source: 'laptop',
  observedAt: NOW,
  windows: [{ id: 'five_hour', usedPercentage: 23.5, resetsAt: NOW + 3600 }],
};

test('rejects ingest without a token', async () => {
  const server = build();
  const response = await server.inject({ method: 'POST', url: '/ingest', payload });
  assert.equal(response.statusCode, 401);
});

test('rejects ingest with a wrong token', async () => {
  const server = build();
  const response = await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer wrong' },
    payload,
  });
  assert.equal(response.statusCode, 401);
});

test('accepts ingest from the collector', async () => {
  const server = build();
  const response = await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer collector-secret' },
    payload,
  });
  assert.equal(response.statusCode, 204);
});

test('rejects ingest with a malformed body', async () => {
  const server = build();
  const response = await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer collector-secret' },
    payload: { provider: 'claude-code' },
  });
  assert.equal(response.statusCode, 400);
});

test('rejects usage without a device token', async () => {
  const server = build();
  assert.equal((await server.inject({ method: 'GET', url: '/usage' })).statusCode, 401);
});

test('rejects usage when the device token is not a known device', async () => {
  const server = build();
  const response = await server.inject({
    method: 'GET',
    url: '/usage',
    headers: { authorization: 'Bearer collector-secret' },
  });
  assert.equal(response.statusCode, 401, 'the collector token must not read the device endpoint');
});

test('walks the full pairing flow and then serves usage to the device', async () => {
  const server = build();

  const codeResponse = await server.inject({
    method: 'POST',
    url: '/device/code',
    payload: { hardware_id: 'esp32-abc' },
  });
  assert.equal(codeResponse.statusCode, 200);
  const { device_code, user_code, verification_uri, interval } = codeResponse.json();
  assert.equal(user_code.length, 8);
  assert.equal(verification_uri, 'http://tok.local/pair');
  assert.equal(interval, 5);

  const pending = await server.inject({
    method: 'POST',
    url: '/device/token',
    payload: { device_code },
  });
  assert.equal(pending.statusCode, 400);
  assert.equal(pending.json().error, 'authorization_pending');

  const approved = await server.inject({
    method: 'POST',
    url: '/pair/approve',
    payload: { user_code },
  });
  assert.equal(approved.statusCode, 204);

  const tokenResponse = await server.inject({
    method: 'POST',
    url: '/device/token',
    payload: { device_code },
  });
  assert.equal(tokenResponse.statusCode, 200);
  const { access_token } = tokenResponse.json();

  await server.inject({
    method: 'POST',
    url: '/ingest',
    headers: { authorization: 'Bearer collector-secret' },
    payload,
  });

  const usage = await server.inject({
    method: 'GET',
    url: '/usage',
    headers: { authorization: `Bearer ${access_token}` },
  });
  assert.equal(usage.statusCode, 200);
  const body = usage.json();
  assert.equal(body.hasData, true);
  assert.equal(body.ageSeconds, 0);
  assert.equal(body.windows[0].usedPercentage, 23.5);
});

test('reports expired_token once the code TTL passes', async () => {
  const nowRef = { value: NOW };
  const server = build(nowRef);
  const { device_code } = (
    await server.inject({
      method: 'POST',
      url: '/device/code',
      payload: { hardware_id: 'esp32-abc' },
    })
  ).json();

  nowRef.value = NOW + 301;
  const response = await server.inject({
    method: 'POST',
    url: '/device/token',
    payload: { device_code },
  });
  assert.equal(response.statusCode, 400);
  assert.equal(response.json().error, 'expired_token');
});

test('serves the web view without a token', async () => {
  const server = build();
  const response = await server.inject({ method: 'GET', url: '/usage/web' });
  assert.equal(response.statusCode, 200);
  assert.equal(response.json().hasData, false);
});
```

- [ ] **Step 3: Rodar o teste e verificar que falha**

```bash
cd backend && npm test
```

Esperado: FAIL — `Cannot find module '../src/http/server.ts'`.

- [ ] **Step 4: Criar `backend/src/http/auth.ts`**

Isolado para que `ingest.ts` e `usage.ts` compartilhem sem importar um do outro.

```ts
export function requireBearer(header: string | undefined): string | null {
  if (header === undefined || !header.startsWith('Bearer ')) return null;
  const token = header.slice('Bearer '.length).trim();
  return token.length === 0 ? null : token;
}
```

- [ ] **Step 5: Criar `backend/src/http/routes/ingest.ts`**

```ts
import type { FastifyInstance } from 'fastify';
import type { Snapshot } from '../../core/types.ts';
import { logger } from '../../logger.ts';
import { requireBearer } from '../auth.ts';
import type { ServerDeps } from '../server.ts';

const bodySchema = {
  type: 'object',
  required: ['provider', 'source', 'observedAt', 'windows'],
  properties: {
    provider: { type: 'string' },
    source: { type: 'string' },
    observedAt: { type: 'number' },
    windows: {
      type: 'array',
      items: {
        type: 'object',
        required: ['id', 'usedPercentage', 'resetsAt'],
        properties: {
          id: { type: 'string', enum: ['five_hour', 'seven_day'] },
          usedPercentage: { type: 'number', minimum: 0, maximum: 100 },
          resetsAt: { type: 'number' },
        },
      },
    },
  },
} as const;

export function registerIngest(server: FastifyInstance, deps: ServerDeps): void {
  server.post('/ingest', { schema: { body: bodySchema } }, async (request, reply) => {
    const token = requireBearer(request.headers.authorization);
    const identity = token === null ? null : deps.tokens.resolveCollector(token);
    if (identity === null) return reply.code(401).send({ error: 'unauthorized' });

    const snapshot = request.body as Snapshot;
    deps.snapshots.put(identity, snapshot);
    logger.info('snapshot ingested', { identity, source: snapshot.source });
    return reply.code(204).send();
  });
}
```

- [ ] **Step 6: Criar `backend/src/http/routes/usage.ts`**

```ts
import type { FastifyInstance } from 'fastify';
import { buildUsageView } from '../../core/freshness.ts';
import { DEFAULT_IDENTITY } from '../../core/identity.ts';
import { requireBearer } from '../auth.ts';
import type { ServerDeps } from '../server.ts';

export function registerUsage(server: FastifyInstance, deps: ServerDeps): void {
  server.get('/usage', async (request, reply) => {
    const token = requireBearer(request.headers.authorization);
    const identity = token === null ? null : deps.tokens.resolveDevice(token);
    if (identity === null) return reply.code(401).send({ error: 'unauthorized' });

    return reply.send(
      buildUsageView(deps.snapshots.get(identity), deps.now(), deps.staleAfterSeconds),
    );
  });

  // No auth today: there is no Anthropic credential to protect, and the /web is
  // LAN-only. This gains a session check when accounts arrive.
  server.get('/usage/web', async (_request, reply) =>
    reply.send(
      buildUsageView(deps.snapshots.get(DEFAULT_IDENTITY), deps.now(), deps.staleAfterSeconds),
    ),
  );
}
```

- [ ] **Step 7: Criar `backend/src/http/routes/device.ts`**

```ts
import type { FastifyInstance } from 'fastify';
import { DEFAULT_IDENTITY } from '../../core/identity.ts';
import { logger } from '../../logger.ts';
import type { ServerDeps } from '../server.ts';

const POLL_INTERVAL_SECONDS = 5;

const codeSchema = {
  type: 'object',
  required: ['hardware_id'],
  properties: { hardware_id: { type: 'string' } },
} as const;

const tokenSchema = {
  type: 'object',
  required: ['device_code'],
  properties: { device_code: { type: 'string' } },
} as const;

const approveSchema = {
  type: 'object',
  required: ['user_code'],
  properties: { user_code: { type: 'string' } },
} as const;

const REDEEM_ERRORS = {
  pending: 'authorization_pending',
  expired: 'expired_token',
  unknown: 'access_denied',
} as const;

export function registerDevice(server: FastifyInstance, deps: ServerDeps): void {
  server.post('/device/code', { schema: { body: codeSchema } }, async (request, reply) => {
    const { hardware_id } = request.body as { hardware_id: string };
    deps.pairing.sweep(deps.now());
    const record = deps.pairing.create(hardware_id, deps.now());
    logger.info('pairing started', { hardwareId: hardware_id, userCode: record.userCode });

    return reply.send({
      device_code: record.deviceCode,
      user_code: record.userCode,
      verification_uri: deps.verificationUri,
      expires_in: record.expiresAt - deps.now(),
      interval: POLL_INTERVAL_SECONDS,
    });
  });

  server.post('/device/token', { schema: { body: tokenSchema } }, async (request, reply) => {
    const { device_code } = request.body as { device_code: string };
    const result = deps.pairing.redeem(device_code, deps.now());

    if (result.status === 'approved') {
      deps.tokens.registerDevice(result.accessToken, DEFAULT_IDENTITY);
      logger.info('device paired', { hardwareId: result.hardwareId });
      return reply.send({ access_token: result.accessToken, token_type: 'Bearer' });
    }
    return reply.code(400).send({ error: REDEEM_ERRORS[result.status] });
  });

  server.post('/pair/approve', { schema: { body: approveSchema } }, async (request, reply) => {
    const { user_code } = request.body as { user_code: string };
    const result = deps.pairing.approve(user_code, deps.now());
    if (result.status !== 'ok') return reply.code(400).send({ error: result.status });
    return reply.code(204).send();
  });
}
```

- [ ] **Step 8: Criar `backend/src/http/server.ts`**

```ts
import Fastify, { type FastifyInstance } from 'fastify';
import type { PairingStore } from '../core/pairing.ts';
import type { SnapshotStore } from '../core/snapshots.ts';
import type { TokenRegistry } from '../core/identity.ts';
import { registerDevice } from './routes/device.ts';
import { registerIngest } from './routes/ingest.ts';
import { registerUsage } from './routes/usage.ts';

export type ServerDeps = {
  snapshots: SnapshotStore;
  tokens: TokenRegistry;
  pairing: PairingStore;
  now: () => number;
  staleAfterSeconds: number;
  verificationUri: string;
};

export function buildServer(deps: ServerDeps): FastifyInstance {
  const server = Fastify({ logger: false });
  registerIngest(server, deps);
  registerUsage(server, deps);
  registerDevice(server, deps);
  return server;
}
```

- [ ] **Step 9: Rodar os testes e verificar que passam**

```bash
cd backend && npm test
```

Esperado: `# pass 39`, `# fail 0`.

- [ ] **Step 10: Commit**

```bash
git add backend/package.json backend/package-lock.json backend/src/http backend/test/http.test.ts
git commit -m "feat: add http routes for ingest, usage and device pairing"
```

---

### Task 6: Entrypoint do backend

**Files:**
- Create: `backend/src/main.ts`, `backend/.env.example`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: `buildServer`, `SnapshotStore`, `TokenRegistry`, `PairingStore`, `loadJson`, `saveJson`, `logger`
- Produces: processo executável. Sem exports.

- [ ] **Step 1: Criar `backend/.env.example`**

```sh
# Shared secret the collector uses on POST /ingest. Generate with:
#   node -e "console.log(require('crypto').randomBytes(32).toString('base64url'))"
TOKESP_COLLECTOR_TOKEN=change-me

# Shown on the device display so you know where to type the pairing code.
TOKESP_VERIFICATION_URI=http://tok.local/pair

PORT=8080
TOKESP_STATE_FILE=./state.json
TOKESP_STALE_AFTER_SECONDS=900
```

- [ ] **Step 2: Criar `backend/src/main.ts`**

```ts
import { PairingStore } from './core/pairing.ts';
import { SnapshotStore } from './core/snapshots.ts';
import { TokenRegistry } from './core/identity.ts';
import { buildServer } from './http/server.ts';
import { loadJson, saveJson } from './store/jsonFile.ts';
import { logger } from './logger.ts';
import type { Snapshot } from './core/types.ts';

type PersistedState = {
  snapshots: Record<string, Snapshot>;
  deviceTokens: Record<string, string>;
};

const PAIRING_TTL_SECONDS = 300;
const SAVE_INTERVAL_MS = 10_000;

const required = (name: string): string => {
  const value = process.env[name];
  if (value === undefined || value.length === 0) {
    logger.error('missing required environment variable', { name });
    process.exit(1);
  }
  return value;
};

const start = async (): Promise<void> => {
  const stateFile = process.env.TOKESP_STATE_FILE ?? './state.json';
  const state = await loadJson<PersistedState>(stateFile, { snapshots: {}, deviceTokens: {} });

  const snapshots = SnapshotStore.fromJSON(state.snapshots);
  const tokens = TokenRegistry.fromJSON(required('TOKESP_COLLECTOR_TOKEN'), state.deviceTokens);
  const pairing = new PairingStore(PAIRING_TTL_SECONDS);

  const server = buildServer({
    snapshots,
    tokens,
    pairing,
    now: () => Math.floor(Date.now() / 1000),
    staleAfterSeconds: Number(process.env.TOKESP_STALE_AFTER_SECONDS ?? 900),
    verificationUri: required('TOKESP_VERIFICATION_URI'),
  });

  const persist = (): Promise<void> =>
    saveJson(stateFile, { snapshots: snapshots.toJSON(), deviceTokens: tokens.toJSON() });

  setInterval(() => void persist(), SAVE_INTERVAL_MS).unref();
  for (const signal of ['SIGINT', 'SIGTERM'] as const) {
    process.on(signal, () => void persist().then(() => process.exit(0)));
  }

  const port = Number(process.env.PORT ?? 8080);
  await server.listen({ port, host: '0.0.0.0' });
  logger.info('backend listening', { port });
};

void start();
```

- [ ] **Step 3: Ignorar os artefatos do backend**

```bash
cd /Users/lucas/Documents/Projetos/Pessoal/harware/tokEsp
printf 'backend/.env\nbackend/state.json\nbackend/state.json.tmp\nnode_modules\n' >> .gitignore
```

- [ ] **Step 4: Verificar que o servidor sobe e responde**

```bash
cd backend
cp .env.example .env
node -e "console.log(require('crypto').randomBytes(32).toString('base64url'))"
# Cole o valor em TOKESP_COLLECTOR_TOKEN no .env, depois:
node --env-file=.env src/main.ts &
sleep 1
curl -s localhost:8080/usage/web
kill %1
```

Esperado: `{"windows":[],"ageSeconds":0,"stale":false,"hasData":false}`

- [ ] **Step 5: Commit**

```bash
git add backend/src/main.ts backend/.env.example backend/package.json .gitignore
git commit -m "feat: add backend entrypoint with state persistence"
```

---

### Task 7: Collector statusline

A transformação vive em `payload.jq` separada do `statusline.sh` justamente para ser testável sem rede.

**Files:**
- Create: `collector/payload.jq`, `collector/statusline.sh`, `collector/config.example.sh`, `collector/test/payload.test.sh`, `collector/README.md`

**Interfaces:**
- Consumes: contrato de `POST /ingest` da Task 5
- Produces: nada em código. Produz o JSON que o backend consome.

- [ ] **Step 1: Escrever o teste que falha**

Cobre os casos que a doc do Claude Code avisa: `rate_limits` ausente (antes da primeira resposta da API, ou conta não-Pro/Max) e janelas parciais.

Criar `collector/test/payload.test.sh`:

```bash
#!/usr/bin/env bash
set -uo pipefail

JQ_FILE="$(dirname "$0")/../payload.jq"
FAILURES=0

run() {
  printf '%s' "$1" | jq -S -c --arg src "testhost" --argjson now 1800000000 -f "$JQ_FILE"
}

expect() {
  local name="$1" actual="$2" expected="$3"
  if [ "$actual" = "$(printf '%s' "$expected" | jq -S -c .)" ]; then
    echo "ok - $name"
  else
    echo "FAIL - $name"
    echo "  expected: $(printf '%s' "$expected" | jq -S -c .)"
    echo "  actual:   $actual"
    FAILURES=$((FAILURES + 1))
  fi
}

expect "both windows present" \
  "$(run '{"rate_limits":{"five_hour":{"used_percentage":23.5,"resets_at":1800003600},"seven_day":{"used_percentage":41.2,"resets_at":1800086400}}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[{"id":"five_hour","resetsAt":1800003600,"usedPercentage":23.5},{"id":"seven_day","resetsAt":1800086400,"usedPercentage":41.2}]}'

expect "rate_limits absent yields empty windows, not an error" \
  "$(run '{"model":{"display_name":"Opus"}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[]}'

expect "only five_hour present" \
  "$(run '{"rate_limits":{"five_hour":{"used_percentage":10,"resets_at":1800003600}}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[{"id":"five_hour","resetsAt":1800003600,"usedPercentage":10}]}'

expect "zero percent is preserved, not dropped as falsy" \
  "$(run '{"rate_limits":{"five_hour":{"used_percentage":0,"resets_at":1800003600}}}')" \
  '{"observedAt":1800000000,"provider":"claude-code","source":"testhost","windows":[{"id":"five_hour","resetsAt":1800003600,"usedPercentage":0}]}'

[ "$FAILURES" -eq 0 ] || exit 1
echo "all payload tests passed"
```

```bash
chmod +x collector/test/payload.test.sh
```

- [ ] **Step 2: Rodar o teste e verificar que falha**

```bash
./collector/test/payload.test.sh
```

Esperado: FAIL — `jq: error: Could not open payload.jq`.

- [ ] **Step 3: Criar `collector/payload.jq`**

`// empty` é o que faz janela ausente sumir do array em vez de virar `null`.

```jq
{
  provider: "claude-code",
  source: $src,
  observedAt: $now,
  windows: [
    (.rate_limits.five_hour // empty
      | { id: "five_hour", usedPercentage: .used_percentage, resetsAt: .resets_at }),
    (.rate_limits.seven_day // empty
      | { id: "seven_day", usedPercentage: .used_percentage, resetsAt: .resets_at })
  ]
}
```

- [ ] **Step 4: Rodar o teste e verificar que passa**

```bash
./collector/test/payload.test.sh
```

Esperado: 4 linhas `ok - ...` e `all payload tests passed`.

- [ ] **Step 5: Criar `collector/config.example.sh`**

```sh
# Copy to ~/.config/tokesp/config.sh and fill in.
export TOKESP_URL="http://localhost:8080"
export TOKESP_TOKEN="the value of TOKESP_COLLECTOR_TOKEN from the backend .env"
```

- [ ] **Step 6: Criar `collector/statusline.sh`**

Duas decisões carregam o peso aqui e não podem ser "simplificadas":

1. **`( ... & ) &` com subshell duplo.** A doc do Claude Code diz que a execução em voo é cancelada quando um novo update dispara. Um `curl` em foreground travaria o render *e* apanharia cancelamento. O duplo detach desliga o POST do ciclo de vida do script.
2. **A saída do statusline nunca depende do POST.** Se o backend estiver fora do ar, a linha de status continua funcionando.

```bash
#!/usr/bin/env bash
set -uo pipefail

CONFIG_FILE="${TOKESP_CONFIG:-$HOME/.config/tokesp/config.sh}"
# shellcheck source=/dev/null
[ -f "$CONFIG_FILE" ] && . "$CONFIG_FILE"

input=$(cat)
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -n "${TOKESP_URL:-}" ] && [ -n "${TOKESP_TOKEN:-}" ]; then
  payload=$(printf '%s' "$input" | jq -c \
    --arg src "$(hostname -s)" \
    --argjson now "$(date +%s)" \
    -f "$script_dir/payload.jq" 2>/dev/null)

  if [ -n "$payload" ]; then
    # Detached twice: the statusline render must never wait on the network, and
    # Claude Code cancels in-flight executions when a new update arrives.
    ( curl -sS -m 3 -X POST "$TOKESP_URL/ingest" \
        -H "Authorization: Bearer $TOKESP_TOKEN" \
        -H 'Content-Type: application/json' \
        -d "$payload" >/dev/null 2>&1 & ) &
  fi
fi

printf '%s' "$input" | jq -r '
  [ (.model.display_name // "?"),
    (if .rate_limits.five_hour then "5h \(.rate_limits.five_hour.used_percentage | round)%" else empty end),
    (if .rate_limits.seven_day then "7d \(.rate_limits.seven_day.used_percentage | round)%" else empty end)
  ] | join("  ")'
```

```bash
chmod +x collector/statusline.sh
```

- [ ] **Step 7: Verificar o script com input mock**

```bash
echo '{"model":{"display_name":"Opus"},"rate_limits":{"five_hour":{"used_percentage":23.5,"resets_at":1800003600}}}' | ./collector/statusline.sh
```

Esperado: `Opus  5h 24%`

```bash
echo '{"model":{"display_name":"Opus"}}' | ./collector/statusline.sh
```

Esperado: `Opus` (sem erro — `rate_limits` ausente é normal antes da primeira resposta da API).

- [ ] **Step 8: Criar `collector/README.md`**

````markdown
# tokEsp collector

Empurra o consumo da assinatura Claude para o backend do tokEsp.

## Instalação

1. `mkdir -p ~/.config/tokesp && cp config.example.sh ~/.config/tokesp/config.sh`
2. Edite `~/.config/tokesp/config.sh` com a URL e o token do backend.
3. Adicione ao `~/.claude/settings.json`:

```json
{
  "statusLine": {
    "type": "command",
    "command": "/caminho/absoluto/para/collector/statusline.sh",
    "refreshInterval": 60000
  }
}
```

`refreshInterval` mantém o push acontecendo enquanto a sessão está ociosa.

## Se você já usa um statusline

Este script substitui o seu. Para manter os dois, chame o seu script no final
de `statusline.sh` em vez do bloco `jq` de saída.

## Usar em outra máquina

Copie a pasta `collector/`, repita os 3 passos acima com a mesma URL e token.
Várias máquinas podem reportar ao mesmo tempo: os limites são da conta, então
todas mandam o mesmo número e a mais recente ganha.

## Limitações

- `rate_limits` só existe para assinantes **Pro/Max**, e só **após a primeira
  resposta da API** na sessão. Antes disso o array de janelas vai vazio.
- Requer Claude Code **>= 2.1.92**.
- O número reflete sua última interação com o Claude Code, não "agora".
- Claude Desktop não tem statusline. O percentual já inclui o uso do Desktop
  (o limite é da assinatura), mas só atualiza enquanto o Claude Code roda.
````

- [ ] **Step 9: Commit**

```bash
git add collector/
git commit -m "feat: add claude code statusline collector"
```

---

### Task 8: Web — dashboard e formulário de pareamento

**Files:**
- Modify: `web/tsconfig.app.json` (adiciona `strict`), `web/vite.config.ts` (proxy `/api`), `web/src/App.tsx` (substitui o scaffold)
- Create: `web/src/api.ts`, `web/src/components/UsageBars.tsx`, `web/src/components/PairForm.tsx`
- Delete: `web/src/assets/react.svg`, `web/src/assets/vite.svg`, `web/src/assets/hero.png`

**Interfaces:**
- Consumes: `GET /usage/web` e `POST /pair/approve` da Task 5
- Produces: nada consumido por outras tasks

- [ ] **Step 1: Habilitar `strict` no `web/tsconfig.app.json`**

O scaffold do Vite veio sem `strict`, o que viola as Global Constraints. Adicione dentro de `compilerOptions`, junto do bloco `/* Linting */`:

```json
    "strict": true,
```

- [ ] **Step 2: Configurar o proxy em `web/vite.config.ts`**

Sem isso o browser bate em CORS ao chamar o backend na porta 8080.

```ts
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      '/api': {
        target: 'http://localhost:8080',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/api/, ''),
      },
    },
  },
})
```

- [ ] **Step 3: Criar `web/src/api.ts`**

```ts
export type WindowId = 'five_hour' | 'seven_day';

export type WindowView = {
  id: WindowId;
  usedPercentage: number | null;
  resetsAt: number;
};

export type UsageView = {
  windows: WindowView[];
  ageSeconds: number;
  stale: boolean;
  hasData: boolean;
};

export async function fetchUsage(): Promise<UsageView> {
  const response = await fetch('/api/usage/web');
  if (!response.ok) throw new Error(`usage request failed: ${response.status}`);
  return (await response.json()) as UsageView;
}

export async function approvePairing(userCode: string): Promise<void> {
  const response = await fetch('/api/pair/approve', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ user_code: userCode }),
  });
  if (!response.ok) {
    const body = (await response.json()) as { error?: string };
    throw new Error(body.error ?? 'unknown');
  }
}
```

- [ ] **Step 4: Criar `web/src/components/UsageBars.tsx`**

Os três estados de frescor do spec aparecem aqui como UI. Texto em pt-BR, código em inglês.

```tsx
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
```

- [ ] **Step 5: Criar `web/src/components/PairForm.tsx`**

```tsx
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
```

- [ ] **Step 6: Substituir `web/src/App.tsx`**

```tsx
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
```

- [ ] **Step 7: Remover os assets do scaffold**

```bash
cd web && rm -f src/assets/react.svg src/assets/vite.svg src/assets/hero.png
```

- [ ] **Step 8: Verificar o build e o lint**

```bash
cd web && npx tsc -b && npm run lint
```

Esperado: sem erros. `App.css` ainda tem CSS do scaffold que referencia os assets removidos — CSS órfão não quebra o build; estilizar é trabalho de acabamento fora deste plano.

- [ ] **Step 9: Commit**

```bash
git add web/
git commit -m "feat: add usage dashboard and device pairing form"
```

---

### Task 9: Contrato do device + verificação ponta a ponta

O firmware está fora deste plano, então o contrato precisa ficar escrito — senão quem for implementar o ESP32 adivinha. Esta task também prova o sistema inteiro com `curl` no papel do device.

**Files:**
- Create: `docs/device-api.md`
- Modify: `README.md` (criar se não existir)

**Interfaces:**
- Consumes: todas as rotas da Task 5
- Produces: documentação. Sem código.

- [ ] **Step 1: Criar `docs/device-api.md`**

````markdown
# Contrato do device (ESP32)

O que o firmware precisa implementar para consumir o backend do tokEsp.
Base URL sem barra no final, ex.: `http://192.168.1.10:8080`.

## 1. Parear (uma vez por device)

O device **nunca** recebe um token gravado por humano. Ele pede um código,
mostra na tela, e o humano aprova no `/web`.

### `POST /device/code`

```json
{ "hardware_id": "esp32-a1b2c3d4" }
```

Resposta `200`:

```json
{
  "device_code": "uuid-opaco",
  "user_code": "K7QM3F9A",
  "verification_uri": "http://tok.local/pair",
  "expires_in": 300,
  "interval": 5
}
```

Mostre `verification_uri` e `user_code` na tela. **Insira o hífen só na
exibição** (`K7QM-3F9A`) — o valor armazenado não tem hífen.

### `POST /device/token` (polling)

Repita a cada `interval` segundos:

```json
{ "device_code": "uuid-opaco" }
```

| Status | Corpo | O que fazer |
|---|---|---|
| `200` | `{ "access_token": "...", "token_type": "Bearer" }` | Grave o token de forma persistente (NVS). Pareamento concluído. |
| `400` | `{ "error": "authorization_pending" }` | Ninguém aprovou ainda. Continue o polling. |
| `400` | `{ "error": "expired_token" }` | O código morreu. Recomece do `/device/code`. |
| `400` | `{ "error": "access_denied" }` | Código desconhecido. Recomece do `/device/code`. |

O código é **de uso único** e vale 5 minutos.

## 2. Ler o consumo

### `GET /usage`

Header: `Authorization: Bearer <access_token>`

Resposta `200`:

```json
{
  "windows": [
    { "id": "five_hour", "usedPercentage": 23.5, "resetsAt": 1738425600 },
    { "id": "seven_day", "usedPercentage": null, "resetsAt": 1738857600 }
  ],
  "ageSeconds": 42,
  "stale": false,
  "hasData": true
}
```

`401` significa que o backend não conhece mais este token: apague-o do
armazenamento e recomece o pareamento.

## 3. As três regras que o display precisa respeitar

Estas não são casos de borda. São o produto — é o que impede o display de
mentir com confiança.

| Condição | O que mostrar | Por quê |
|---|---|---|
| `usedPercentage === null` | `--%`, barra vazia | A janela já reiniciou. O valor guardado descreve uma janela que não existe mais. **Nunca mostre o número antigo.** |
| `hasData === false` | "Sem dados" | O Claude Code ainda não respondeu nada nesta sessão, ou a conta não é Pro/Max. **Não é 0%.** |
| `stale === true` | Idade + aviso | O dado está velho; o Claude Code provavelmente está fechado. |

`ageSeconds` é sempre um **limite superior de frescor**: o percentual vem da
última resposta da API na sessão do Claude Code, então mesmo `ageSeconds: 0`
significa "na última interação", nunca "agora".

## Notas de implementação

- **Polling do `/usage`:** 30s é um bom começo.
- **`hardware_id`:** derive do MAC (`ESP.getEfuseMac()`) — estável entre boots,
  único por placa, nada a configurar.
- **Token:** guarde na NVS (`Preferences.h`), não no `secrets.h`. Isso permite
  rotacionar sem reflashar e é onde um captive portal escreveria depois.
- **TLS:** o backend hoje é HTTP puro em LAN. Mantenha o cliente HTTP atrás de
  um wrapper fino para que trocar `WiFiClient` por `WiFiClientSecure` fique
  localizado num arquivo.
- **`localhost` não funciona no ESP32.** Use o IP da máquina na LAN.
````

- [ ] **Step 2: Criar/atualizar `README.md` na raiz**

````markdown
# tokEsp

Mostra quanto da sua assinatura Claude Pro/Max já foi consumida nas janelas de
5 horas e 7 dias — num dashboard web e num display OLED.

O dado vem do statusline do Claude Code, não de uma API: **não existe endpoint
público de consumo de assinatura**. O Claude Code empurra; o backend guarda; o
web e o ESP32 leem.

```
Claude Code ──statusline──► backend ──► /web
                               └──────► ESP32 (Bearer, via device flow)
```

## Componentes

| Pasta | O que é |
|---|---|
| `backend/` | Node + TypeScript. Guarda o snapshot, serve web e device. |
| `collector/` | Script de statusline do Claude Code. A fonte do dado. |
| `web/` | Vite + React. Dashboard e aprovação de pareamento. |
| `firmware/` | ESP32 (Heltec WiFi LoRa 32 V2). Consome `docs/device-api.md`. |

## Rodar

```bash
# backend
cd backend && npm install && cp .env.example .env
node -e "console.log(require('crypto').randomBytes(32).toString('base64url'))"
# cole em TOKESP_COLLECTOR_TOKEN no .env
npm run dev

# web
cd web && npm install && npm run dev

# collector: veja collector/README.md
```

## Testes

```bash
cd backend && npm test
./collector/test/payload.test.sh
cd web && npx tsc -b && npm run lint
```

## Limitações conhecidas

- Requer assinatura **Pro/Max** e Claude Code **>= 2.1.92**.
- O número reflete sua última interação com o Claude Code, não "agora". Se o
  Claude Code estiver fechado, o dado congela — daí o campo `stale`.
- Claude Desktop não tem statusline. O uso dele **conta** no percentual (o
  limite é da assinatura), mas só aparece quando o Claude Code roda de novo.

## Docs

- `docs/superpowers/specs/2026-07-15-claude-usage-esp32-design.md` — desenho e o porquê
- `docs/device-api.md` — contrato para o firmware
````

- [ ] **Step 3: Verificar o fluxo completo do device com `curl`**

Isto substitui o teste no hardware: `curl` faz o papel do ESP32.

```bash
cd backend && node --env-file=.env src/main.ts &
sleep 1
BASE=http://localhost:8080
TOKEN=$(grep TOKESP_COLLECTOR_TOKEN .env | cut -d= -f2)

# 1. Device pede um código
RESP=$(curl -s -X POST $BASE/device/code -H 'Content-Type: application/json' -d '{"hardware_id":"esp32-test"}')
DEVICE_CODE=$(echo "$RESP" | jq -r .device_code)
USER_CODE=$(echo "$RESP" | jq -r .user_code)
echo "código na tela: $USER_CODE"

# 2. Polling antes da aprovação
curl -s -X POST $BASE/device/token -H 'Content-Type: application/json' -d "{\"device_code\":\"$DEVICE_CODE\"}"
# Esperado: {"error":"authorization_pending"}

# 3. Humano aprova (o /web faz isto)
curl -s -o /dev/null -w '%{http_code}\n' -X POST $BASE/pair/approve -H 'Content-Type: application/json' -d "{\"user_code\":\"$USER_CODE\"}"
# Esperado: 204

# 4. Polling agora entrega o token
ACCESS=$(curl -s -X POST $BASE/device/token -H 'Content-Type: application/json' -d "{\"device_code\":\"$DEVICE_CODE\"}" | jq -r .access_token)
echo "token: ${ACCESS:0:12}..."

# 5. Sem dados ainda
curl -s $BASE/usage -H "Authorization: Bearer $ACCESS"
# Esperado: {"windows":[],"ageSeconds":0,"stale":false,"hasData":false}

# 6. Collector empurra um snapshot
NOW=$(date +%s)
curl -s -o /dev/null -X POST $BASE/ingest -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' \
  -d "{\"provider\":\"claude-code\",\"source\":\"test\",\"observedAt\":$NOW,\"windows\":[{\"id\":\"five_hour\",\"usedPercentage\":23.5,\"resetsAt\":$((NOW+3600))},{\"id\":\"seven_day\",\"usedPercentage\":41.2,\"resetsAt\":$((NOW+86400))}]}"

# 7. Device lê o consumo
curl -s $BASE/usage -H "Authorization: Bearer $ACCESS" | jq .
# Esperado: hasData true, five_hour 23.5, seven_day 41.2

# 8. Regra do rollover: janela que já virou vira null
curl -s -o /dev/null -X POST $BASE/ingest -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' \
  -d "{\"provider\":\"claude-code\",\"source\":\"test\",\"observedAt\":$NOW,\"windows\":[{\"id\":\"five_hour\",\"usedPercentage\":23.5,\"resetsAt\":$((NOW-10))}]}"
curl -s $BASE/usage -H "Authorization: Bearer $ACCESS" | jq '.windows[0].usedPercentage'
# Esperado: null  ← o número velho NÃO aparece

# 9. Token inválido é rejeitado
curl -s -o /dev/null -w '%{http_code}\n' $BASE/usage -H "Authorization: Bearer nope"
# Esperado: 401

kill %1
```

- [ ] **Step 4: Verificar o pareamento pelo `/web`**

```bash
# terminal 1
cd backend && node --env-file=.env src/main.ts
# terminal 2
cd web && npm run dev
# terminal 3
curl -s -X POST localhost:8080/device/code -H 'Content-Type: application/json' -d '{"hardware_id":"esp32-test"}' | jq -r .user_code
```

Abra `http://localhost:5173`, digite o código no formulário. Esperado: "Dispositivo pareado."

- [ ] **Step 5: Commit**

```bash
git add docs/device-api.md README.md
git commit -m "docs: add device api contract and project readme"
```

---

## Verificação final

- [ ] `cd backend && npm test` → `# pass 39`, `# fail 0`
- [ ] `./collector/test/payload.test.sh` → `all payload tests passed`
- [ ] `cd web && npx tsc -b && npm run lint` → sem erros
- [ ] Task 9, Step 3 completo — todos os 9 passos do `curl` com a saída esperada
- [ ] Task 9, Step 4 completo — pareamento aprovado pelo `/web`
- [ ] `git status` limpo; `.env` e `state.json` **não** versionados
- [ ] `git log --oneline -- firmware/ platformio.ini` vazio — nenhum commit deste plano tocou no firmware

## Fora de escopo

- **Firmware.** O contrato está em `docs/device-api.md`; a implementação é outro plano.
- Contas de usuário, TLS, captive portal de Wi-Fi, outros providers, estilização do `/web` além do funcional.
