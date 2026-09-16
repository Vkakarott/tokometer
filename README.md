# tokEsp

Mostra quanto da sua assinatura Claude Pro/Max já foi consumida nas janelas de
5 horas e 7 dias — num dashboard web e num display OLED.

**Não existe endpoint público de consumo de assinatura.** O collector lê o uso
da conta pela mesma rota interna que a tela `/usage` do Claude Code usa, depois
de cada resposta (hook `Stop`, inclusive no VS Code) e a cada 2 minutos. Essa
rota não é documentada e pode mudar; veja `collector/README.md`. O collector
empurra; o backend guarda; o web e o ESP32 leem.

```
Claude Code ──collector──► backend API ──► web (/ e /pair)
                                   └──────► ESP32 (Bearer, via device flow)
```

## Componentes

| Pasta | O que é |
|---|---|
| `backend/` | Node + TypeScript. Guarda o snapshot e expõe a API para web e device. |
| `collector/` | Envia o uso da conta ao backend (hook `Stop` + launchd). A fonte do dado. |
| `web/` | Vite + React. Dashboard e aprovação de pareamento. |
| `firmware/` | ESP32 (Heltec WiFi LoRa 32 V2). Consome `docs/device-api.md`. |

## Rodar

```bash
# backend
cd backend && npm install && cp .env.example .env
node -e "console.log(require('crypto').randomBytes(32).toString('base64url'))"
# cole o valor em TOKESP_COLLECTOR_TOKEN no .env
# ajuste TOKESP_VERIFICATION_URI para http://<NOME-DA-MAQUINA>.local:43111/pair
npm run dev   # porta 43110

# web
cd web && npm install && npm run dev   # porta 43111
# o painel fica em / e o pareamento em /pair; o backend não serve o artefato buildado.

# collector: veja collector/README.md
```

## Testes

```bash
cd backend && npm test
./collector/test/payload.test.sh
./collector/test/statusline.test.sh
./collector/test/usage.test.sh
./collector/test/usage_poll.test.sh
cd web && npm run build && npm run lint

# firmware (na raiz, com PlatformIO)
pio test -e native
pio run -e heltec_wifi_lora_32_V2
```

Antes de gravar o ESP32, `API_HOST` em `firmware/src/config/api_config.h` e
`TOKESP_VERIFICATION_URI` no `backend/.env` precisam usar o nome Bonjour da
máquina (`scutil --get LocalHostName` no macOS, ex.: `minha-maquina.local`). O
firmware resolve esse nome por mDNS a cada falha, então trocar de IP não exige
regravar a placa.

## Limitações conhecidas

- Requer assinatura **Pro/Max** e Claude Code **>= 2.1.92**.
- O número reflete sua última interação com o Claude Code, não "agora". Se o
  Claude Code estiver fechado, o dado congela — daí o campo `stale`.
- Claude Desktop não tem statusline. O uso dele **conta** no percentual (o
  limite é da assinatura), mas só aparece quando o Claude Code roda de novo.
- A contagem de contexto é uma métrica da sessão atual, separada do consumo da
  assinatura.

## Docs

- `docs/superpowers/specs/2026-07-15-claude-usage-esp32-design.md` — desenho e o porquê
- `docs/device-api.md` — contrato para o firmware
- `collector/README.md` — configuração do statusline
- `web/README.md` — execução do painel web
