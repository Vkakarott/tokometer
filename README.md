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
