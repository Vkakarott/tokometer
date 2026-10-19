# tokEsp

Mostra quanto das suas assinaturas **Claude** (Pro/Max), **Codex** (ChatGPT) e
**Cursor** foi consumido — num tokonotch nativo do macOS e num display TFT
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
| `windows/` | App de bandeja para Windows: coleta local de Claude, Codex e Cursor, sem depender do display. |
| `firmware/` | ESP32-C3 Mini + TFT SPI JD9853 240×296. Consome `docs/device-api.md`. |

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
pio run   # alvo único: esp32_c3_jd9853
```

Antes de gravar o ESP32, `API_HOST` em `firmware/src/config/api_config.h` e
`TOKESP_VERIFICATION_URI` no `backend/.env` precisam usar o nome Bonjour da
máquina (`scutil --get LocalHostName` no macOS, ex.: `minha-maquina.local`). O
firmware resolve esse nome por mDNS a cada falha, então trocar de IP não exige
regravar a placa.

### Primeira conexão do display

Depois de gravar, o display mostra a tela de configuração. No tokometer, abra
**Conectar display**, informe a rede Wi-Fi (2,4 GHz) e a senha, e confirme. O
app entra sozinho na rede temporária `tokEsp-xxxx`, entrega a rede ao display,
volta para a sua rede e aprova o pareamento: não há código para digitar. O Mac
fica alguns segundos fora da rede durante o envio, e o macOS pede acesso à
Localização na primeira vez, porque sem ele não mostra nomes de redes Wi-Fi aos
aplicativos.

A senha é salva na memória da placa; ela não entra no firmware nem precisa ser
gravada outra vez. O display mantém até três redes conhecidas; adicionar uma
rede nova não remove as anteriores até a lista ficar cheia. Se nenhuma rede
salva responder em 15 segundos, ele volta para a tela de configuração e tenta
as redes salvas de novo a cada minuto.

**Manualmente:** em *Configurar manualmente*, ou sem o app, conecte-se à rede
`tokEsp-xxxx`, abra `192.168.4.1`, escolha a rede e depois digite no tokometer
o código de oito caracteres que o display mostrar.

## Hardware do display

O alvo é uma **ESP32-C3 Mini** com um TFT IPS SPI **JD9853 240×296**:

| Display | ESP32-C3 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCK / CLK | GPIO6 |
| MOSI / DIN / SDA | GPIO7 |
| CS | GPIO10 |
| DC / A0 | GPIO3 |
| RST | GPIO1 |
| BL / LED | 3V3 |

```bash
pio run -t upload
```

O botão **BOOT** da placa (GPIO9) troca de tela com um toque curto e alterna a
janela de consumo com um toque longo. Ele só responde na tela de consumo. Os
pinos ficam em `firmware/src/config/hardware_config.h`.

O display mostra só os provedores visíveis nos Ajustes do app do Mac. Com dois
ou mais, o ciclo é visão geral e depois o painel de cada um; com um só, a visão
geral dá lugar ao painel dele. Ocultar todos mostra "Nenhum provedor ativo".

A potência de transmissão do Wi-Fi é limitada a 8,5 dBm: em potência máxima a
antena da C3 Mini falha na autenticação com o roteador.

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
- `windows/README.md` — instalação e publicação do app para Windows
