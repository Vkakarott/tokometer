# tokEsp

Mostra quanto das suas assinaturas **Claude** (Pro/Max) e **Codex** (ChatGPT)
já foi consumido nas janelas de 5 horas e 7 dias — num notch nativo do macOS e
num display OLED opcional.

**Não existe endpoint público de consumo de assinatura.** Os collectors leem o
uso de cada conta pelas mesmas rotas internas que o Claude Code e o Codex usam,
a cada 2 minutos (e, no Claude, também depois de cada resposta pelo hook
`Stop`). Essas rotas não são documentadas e podem mudar; veja
`collector/README.md`. O app macOS consulta as fontes diretamente; o backend
permanece como ponte temporária para o ESP32.

```
Claude / Codex ──► app macOS (notch)
collector ───────► backend API ──► ESP32 (durante a migração)
```

## Componentes

| Pasta | O que é |
|---|---|
| `backend/` | Node + TypeScript. Ponte de snapshots e API do device. |
| `collector/` | Envia o uso das contas Claude e Codex ao backend (hook `Stop` + launchd). A fonte do dado. |
| `macos/` | Aplicativo nativo: notch, coleta local e aprovação de pareamento. |
| `firmware/` | ESP32 DevKit + OLED I2C externo (ou Heltec WiFi LoRa 32 V2). Consome `docs/device-api.md`. |

## Rodar

```bash
# backend
cd backend && npm install && cp .env.example .env
node -e "console.log(require('crypto').randomBytes(32).toString('base64url'))"
# cole o valor em TOKESP_COLLECTOR_TOKEN no .env
npm run dev   # porta 43110

# app macOS
cd macos && swift run TokEsp

# collector: veja collector/README.md
```

## Rodar como serviço (macOS)

Para o backend subir sozinho com o Mac e reiniciar se cair:

```bash
./services/install-services.sh
```

O script copia o backend para `~/.local/share/tokesp` e carrega o agente
`com.tokesp.backend` no launchd. A cópia é necessária porque o macOS impede agentes
do launchd de ler arquivos dentro de `~/Documents`. Rode de novo depois de
mudar o código; o `state.json` instalado (pareamentos e último consumo) é
preservado. Logs em `~/Library/Logs/tokesp-backend.log`.

Para remover:

```bash
launchctl bootout gui/$(id -u)/com.tokesp.backend
```

## Testes

```bash
cd backend && npm test
./collector/test/payload.test.sh
./collector/test/statusline.test.sh
./collector/test/usage.test.sh
./collector/test/usage_poll.test.sh
./collector/test/codex_usage.test.sh
./collector/test/codex_usage_poll.test.sh
cd macos && swift test

# firmware (na raiz, com PlatformIO)
pio test -e native
pio run   # alvo padrão: esp32_i2c
pio run -e heltec_wifi_lora_32_V2
```

Antes de gravar o ESP32, `API_HOST` em `firmware/src/config/api_config.h` e
`TOKESP_VERIFICATION_URI` no `backend/.env` precisam usar o nome Bonjour da
máquina (`scutil --get LocalHostName` no macOS, ex.: `minha-maquina.local`). O
firmware resolve esse nome por mDNS a cada falha, então trocar de IP não exige
regravar a placa.

## Hardware do display

O alvo padrão é uma **ESP32 DevKit (ESP-WROOM-32)** com um módulo **OLED I2C de
4 pinos** (SSD1306 0.96", endereço 0x3C):

| Display | ESP32 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | D22 |
| SDA | D21 |

```bash
pio run -e esp32_i2c -t upload    # placa padrão
pio run -e heltec_wifi_lora_32_V2 -t upload    # placa com OLED embutido
```

O firmware varre o barramento I2C no boot e registra os endereços que
respondem: se nada aparecer, o problema é ligação ou alimentação. Se a imagem
sair deslocada, o painel é SH1106 — some `-DTOKESP_DISPLAY_SH1106` ao
`build_flags` do env. Os pinos de cada placa ficam em
`firmware/src/config/board_*.h`.

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
- `macos/README.md` — execução do notch nativo
