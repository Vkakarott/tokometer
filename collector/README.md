# tokometer collector

Envia o consumo das assinaturas **Claude** e **Codex (ChatGPT)** para o backend
do tokometer.

## Como funciona

Cada collector consulta o mesmo uso da conta que o próprio app mostra e envia
ao backend com o provedor identificado. Como lê a conta, o número acompanha
qualquer cliente daquele provedor.

| Collector | Lê | Atualiza |
|---|---|---|
| `usage_poll.sh` | uso da conta Claude (tela `/usage` do Claude Code) | hook `Stop` do Claude Code + launchd a cada 2 min |
| `codex_usage_poll.sh` | uso da conta ChatGPT (o que o Codex mostra) | launchd a cada 2 min (e `notify` do Codex, opcional) |

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

3. Adicione o hook do Claude ao `~/.claude/settings.json` (vale para a extensão
   e o CLI), apontando para a cópia instalada:

   ```json
   {
     "hooks": {
       "Stop": [
         {
           "hooks": [
             { "type": "command", "command": "/Users/<voce>/.local/share/tokesp/usage_poll.sh --detach" }
           ]
         }
       ]
     }
   }
   ```

Na primeira execução o macOS pode pedir acesso ao item
"Claude Code-credentials": escolha **Sempre permitir**. Erros ficam em
`~/Library/Logs/tokesp-usage-poll.log` e
`~/Library/Logs/tokesp-codex-usage-poll.log`.

Para remover os agendamentos:

```bash
launchctl bootout gui/$(id -u)/com.tokesp.usage-poll
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

## Alternativa sem credenciais para o Claude: statusline

`statusline.sh` usa só dados documentados, mas **só o `claude` no terminal roda
statusline**. A extensão do VS Code não envia nada, e o número só muda quando
uma sessão do terminal recebe resposta. Em troca, ele também envia a contagem
de tokens da janela de contexto.

**Use uma fonte ou a outra, nunca as duas:** a statusline envia o número da
última resposta daquela sessão e sobrescreveria o valor mais novo da conta.

```json
{
  "statusLine": {
    "type": "command",
    "command": "/caminho/absoluto/para/collector/statusline.sh",
    "refreshInterval": 60
  }
}
```

`refreshInterval` é em **segundos**. A statusline só envia quando os valores
mudam em relação ao último envio aceito, e sessões ainda sem resposta da API
não enviam nada. O controle fica em `~/.cache/tokesp/last-sent`; apague esse
arquivo para forçar um novo envio.

## Usar em outra máquina

Copie a pasta `collector/` e repita a instalação com a mesma URL e token. Os
limites são da conta, então todas as máquinas mandam o mesmo número.

## Limitações

- Claude: os limites só existem para assinantes **Pro/Max**. Codex: só com login
  por conta ChatGPT (não por chave de API).
- As rotas têm limite de frequência (a do Claude responde HTTP 429 com
  facilidade). Os collectors consultam no máximo uma vez por minuto e, ao
  receber 429, esperam o `Retry-After` (ou 5 minutos) antes de tentar de novo.
- Os tokens de login expiram. Se o app ficar fechado por muito tempo, o envio
  daquele provedor para até ele abrir de novo; web e OLED mostram o aviso de
  dado antigo.
- Com `usage_poll.sh`, o web não mostra a contagem de contexto: esse dado só
  existe na statusline.
