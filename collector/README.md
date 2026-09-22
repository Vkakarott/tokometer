# tokometer collector

Envia o consumo das assinaturas **Claude** e **Codex (ChatGPT)** para o backend
do tokometer.

## Como funciona

| Collector | Lê | Atualiza |
|---|---|---|
| `statusline.sh` | limites que o Claude Code recebe a cada resposta | toda resposta do `claude` no terminal |
| `codex_usage_poll.sh` | uso da conta ChatGPT (o que o Codex mostra) | launchd a cada 2 min (e `notify` do Codex, opcional) |

**Por que a statusline é a fonte do Claude.** A cada resposta, o Claude Code já
recebe os limites de 5 h e 7 dias e os entrega à statusline. Esse dado é
documentado, chega justamente quando o consumo muda e não gasta nenhuma
consulta. Ficar perguntando à rota da conta de tempos em tempos, como antes,
dava HTTP 429 com frequência e travava a coleta por até uma hora.

A statusline grava `~/.cache/tokesp/claude-statusline.json`, que o app macOS lê,
e envia o mesmo dado ao backend, que alimenta o display ESP32.

**Quando o dado passa de 15 min**, o app consulta a rota da conta por conta
própria, no máximo uma vez a cada 15 min, com pausa após um 429 e sem insistir
com um token que já foi recusado. É o que cobre a extensão do VS Code (que não
roda statusline) e o uso feito no claude.ai.

> **Riscos que você aceita ao usar:**
> - As rotas de uso (`api.anthropic.com/api/oauth/usage` e
>   `chatgpt.com/backend-api/wham/usage`) são **internas e não documentadas**.
>   Podem mudar ou deixar de funcionar sem aviso.
> - Os scripts leem os tokens de login do Claude Code (Keychain do macOS) e do
>   Codex (`~/.codex/auth.json`). Cada token só é enviado ao próprio provedor e
>   nunca aparece na linha de comando.

## Instalação (macOS)

Rode os comandos a partir desta pasta (`collector/`).

1. Configure URL e token do backend:

   ```bash
   mkdir -p ~/.config/tokesp && cp config.example.sh ~/.config/tokesp/config.sh
   # edite ~/.config/tokesp/config.sh; na mesma máquina do backend use http://localhost:43110
   ```

2. Instale os scripts e os agendamentos:

   ```bash
   ./install-usage-poll.sh
   ```

   Ele copia os scripts para `~/.local/share/tokesp` e carrega dois agentes do
   launchd (`com.tokesp.usage-poll` e `com.tokesp.codex-usage-poll`). A cópia é
   proposital: a proteção de privacidade do macOS impede agentes do launchd de
   executar arquivos dentro de `~/Documents`. Rode de novo sempre que atualizar
   o collector.

3. Aponte a statusline do Claude no `~/.claude/settings.json` para a cópia
   instalada:

   ```json
   {
     "statusLine": {
       "type": "command",
       "command": "/Users/<voce>/.local/share/tokesp/statusline.sh",
       "refreshInterval": 60
     }
   }
   ```

   `refreshInterval` é em segundos. Se você vinha do hook `Stop` com
   `usage_poll.sh`, remova esse hook: ele não existe mais.

Na primeira consulta de reserva o macOS pode pedir acesso ao item
"Claude Code-credentials": escolha **Sempre permitir**. Erros do Codex ficam em
`~/Library/Logs/tokesp-codex-usage-poll.log`.

Para remover o agendamento do Codex:

```bash
launchctl bootout gui/$(id -u)/com.tokesp.codex-usage-poll
```

### Codex a cada resposta (opcional)

O Codex roda um programa ao fim de cada turno pela chave `notify` do
`~/.codex/config.toml`, mas aceita **um só** programa. Se ela estiver livre:

```toml
notify = ["/Users/<voce>/.local/share/tokesp/codex_usage_poll.sh", "--detach"]
```

Se já estiver em uso (por exemplo, pelo Computer Use do Codex), não troque:
o agendamento de 2 minutos continua atualizando.

## Quando a statusline envia

A statusline também se redesenha sozinha, e esses redesenhos não trazem dado
novo. Por isso ela só grava e envia quando os valores mudam **ou** quando houve
outra resposta da API na sessão (medida por `cost.total_api_duration_ms`). O
controle fica em `~/.cache/tokesp/last-sent`; apague esse arquivo para forçar um
novo envio. Sessões que ainda não receberam resposta não enviam nada, para não
apagar o dado que o backend já tem.

## Usar em outra máquina

Copie a pasta `collector/` e repita a instalação com a mesma URL e token. Os
limites são da conta, então todas as máquinas mandam o mesmo número.

## Limitações

- Claude: os limites só existem para assinantes **Pro/Max**. Codex: só com login
  por conta ChatGPT (não por chave de API).
- **Só o `claude` no terminal roda statusline.** Na extensão do VS Code, o dado
  do Claude só se atualiza pela consulta de reserva do app, a cada 15 min.
- **Uso feito no claude.ai** (web ou celular) só aparece na próxima resposta do
  Claude Code, ou na consulta de reserva.
- **Com o app fechado e sem usar o terminal, nada atualiza.** O backend marca o
  dado como antigo depois de 15 min, e o OLED mostra o aviso de desatualizado.
- As rotas têm limite de frequência (a do Claude responde HTTP 429 com
  facilidade). O Codex consulta no máximo uma vez por minuto e, ao receber 429,
  espera o `Retry-After` (ou 5 minutos) antes de tentar de novo.
