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
