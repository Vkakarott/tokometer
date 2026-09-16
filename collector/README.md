# tokEsp collector

Envia o consumo da assinatura Claude para o backend do tokEsp.

## Como funciona

`usage_poll.sh` consulta o mesmo uso da conta que a tela `/usage` do Claude
Code mostra e envia ao backend. Como lê a conta, o número acompanha qualquer
cliente: extensão do VS Code, `claude` no terminal e claude.ai.

Ele roda em dois momentos:

- **Hook `Stop` do Claude Code:** depois de cada resposta, inclusive no chat da
  extensão do VS Code.
- **launchd a cada 2 minutos:** mantém o número em dia enquanto você não usa o
  Claude Code.

> **Riscos que você aceita ao usar:**
> - A rota de uso (`api.anthropic.com/api/oauth/usage`) é **interna do Claude
>   Code e não documentada**. Pode mudar ou deixar de funcionar sem aviso.
> - O script lê o token de login do Claude Code no Keychain do macOS. O token só
>   é enviado para a Anthropic e nunca aparece na linha de comando.

## Instalação (macOS)

Rode os comandos a partir desta pasta (`collector/`).

1. Configure URL e token do backend:

   ```bash
   mkdir -p ~/.config/tokesp && cp config.example.sh ~/.config/tokesp/config.sh
   # edite ~/.config/tokesp/config.sh; na mesma máquina do backend use http://localhost:43110
   ```

2. Instale o script e o agendamento:

   ```bash
   ./install-usage-poll.sh
   ```

   Ele copia `usage_poll.sh` e `usage.jq` para `~/.local/share/tokesp`: a
   proteção de privacidade do macOS impede agentes do launchd de executar
   arquivos dentro de `~/Documents`. Rode de novo sempre que atualizar o
   collector.

3. Adicione o hook ao `~/.claude/settings.json` (vale para a extensão e o CLI),
   apontando para a cópia instalada:

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
`~/Library/Logs/tokesp-usage-poll.log`.

Para remover o agendamento:
`launchctl bootout gui/$(id -u)/com.tokesp.usage-poll`.

## Alternativa sem credenciais: statusline

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

- Os limites só existem para assinantes **Pro/Max**.
- O token de login expira. Se o Claude Code ficar fechado por muito tempo, o
  envio para até ele abrir de novo; web e OLED mostram o aviso de dado antigo.
- Com `usage_poll.sh`, o web não mostra a contagem de contexto: esse dado só
  existe na statusline.
