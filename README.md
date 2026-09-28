# tokEsp

Mostra quanto das suas assinaturas **Claude** (Pro/Max), **Codex** (ChatGPT) e
**Cursor** foi consumido — num tokonotch nativo do macOS e num display OLED
opcional.

**Não existe endpoint público de consumo de assinatura.** No Claude, a fonte é
a **statusline**: o Claude Code entrega os limites de 5 h e 7 dias a cada
resposta, sem gastar consulta. Quando esse dado passa de 15 min, o app macOS
consulta a rota interna da conta como reserva; a cadência cai para 1 min quando
o limite de 5 h chega a 90%. O Codex
é lido de 2 em 2 minutos pela rota interna que o próprio Codex usa. Essas rotas
não são documentadas e podem mudar; veja `collector/README.md`. O backend
permanece como ponte temporária para o ESP32.

```
Claude / Codex / Cursor ──► app macOS (tokonotch)
collector ───────► backend API ──► ESP32 (durante a migração)
```

## Componentes

| Pasta | O que é |
|---|---|
| `backend/` | Node + TypeScript. Ponte de snapshots e API do device. |
| `collector/` | Statusline do Claude e poller do Codex. Alimenta o app e o backend. |
| `macos/` | Aplicativo nativo: tokonotch, coleta local e aprovação de pareamento. |
| `firmware/` | ESP32 DevKit + OLED I2C externo (ou Heltec WiFi LoRa 32 V2). Consome `docs/device-api.md`. |

## Rodar

```bash
# backend
cd backend && npm install && cp .env.example .env
node -e "console.log(require('crypto').randomBytes(32).toString('base64url'))"
# cole o valor em TOKESP_COLLECTOR_TOKEN no .env
npm run dev   # porta 43110

# app macOS
cd macos && ./scripts/build-app.sh
open .build/TokEsp.app

# instalar o app para iniciar automaticamente após o login
./scripts/install-app.sh

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

### Primeira conexão do display

Depois de gravar, o display cria uma rede temporária `tokEsp-xxxx`. Conecte o
celular ou Mac a ela, abra `192.168.4.1` e informe a rede Wi-Fi que o display
deve usar. A senha é salva na memória da placa; ela não entra no firmware nem
precisa ser gravada outra vez. O display mantém até três redes conhecidas e
escolhe automaticamente uma que esteja disponível; adicionar uma rede nova não
remove as anteriores até a lista ficar cheia.

Assim que o display entrar na rede, ele exibirá o código de oito caracteres.
No tokometer, abra **Configurações → Display**, informe o código e confirme o
pareamento. Se a rede salva deixar de estar disponível, o display volta para o
modo de configuração após 15 segundos.

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
- `macos/README.md` — execução do tokonotch nativo
