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
    "refreshInterval": 60
  }
}
```

`refreshInterval` é em **segundos**: o script roda de novo a cada minuto
enquanto a sessão está ociosa.

O collector só envia quando os valores mudam em relação ao último envio aceito
pelo backend. Reenviar o mesmo número com horário novo faria um dado velho
parecer atual. Sessões que ainda não receberam resposta da API também não
enviam nada, para não apagar o último dado. O controle fica em
`~/.cache/tokesp/last-sent`; apague esse arquivo para forçar um novo envio
(por exemplo, depois de zerar o `state.json` do backend).

Além dos limites de 5 horas e 7 dias, o collector envia a contagem de tokens
da janela de contexto atual quando o Claude Code disponibiliza esses campos.
Essa contagem é uma métrica da sessão, não do consumo da assinatura.

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
- Só o `claude` no terminal roda o statusline. A extensão do VS Code não
  envia nada: o uso dela conta no percentual, mas só aparece quando uma sessão
  do terminal recebe a próxima resposta da API.
- Claude Desktop não tem statusline. O percentual já inclui o uso do Desktop
  (o limite é da assinatura), mas só atualiza enquanto o Claude Code roda.
