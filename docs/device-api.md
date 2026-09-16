# Contrato do device (ESP32)

O que o firmware precisa implementar para consumir o backend do tokEsp.
Base URL sem barra no final, ex.: `http://minha-maquina.local:43110` — o
firmware resolve o nome por mDNS a cada falha, então o backend pode trocar de
IP sem regravação.

## 1. Parear (uma vez por device)

O device **nunca** recebe um token gravado por humano. Ele pede um código,
mostra na tela, e o humano aprova em `/pair`.

### `POST /device/code`

```json
{ "hardware_id": "esp32-a1b2c3d4" }
```

Resposta `200`:

```json
{
  "device_code": "uuid-opaco",
  "user_code": "K7QM3F9A",
  "verification_uri": "http://tok.local/pair",
  "expires_in": 300,
  "interval": 5
}
```

Mostre `verification_uri` e `user_code` na tela. **Insira o hífen só na
exibição** (`K7QM-3F9A`) — o valor armazenado não tem hífen.

### `POST /device/token` (polling)

Repita a cada `interval` segundos:

```json
{ "device_code": "uuid-opaco" }
```

| Status | Corpo | O que fazer |
|---|---|---|
| `200` | `{ "access_token": "...", "token_type": "Bearer" }` | Grave o token de forma persistente (NVS). Pareamento concluído. |
| `400` | `{ "error": "authorization_pending" }` | Ninguém aprovou ainda. Continue o polling. |
| `400` | `{ "error": "expired_token" }` | O código morreu. Recomece do `/device/code`. |
| `400` | `{ "error": "access_denied" }` | Código desconhecido. Recomece do `/device/code`. |

O código é **de uso único** e vale 5 minutos.

## 2. Ler o consumo

### `GET /usage`

Header: `Authorization: Bearer <access_token>`

Resposta `200`:

```json
{
  "windows": [
    { "id": "five_hour", "usedPercentage": 23.5, "resetsAt": 1738425600 },
    { "id": "seven_day", "usedPercentage": null, "resetsAt": 1738857600 }
  ],
  "ageSeconds": 42,
  "stale": false,
  "hasData": true,
  "context": {
    "inputTokens": 12500,
    "outputTokens": 2400,
    "windowSize": 200000,
    "usedPercentage": 7.45
  }
}
```

`401` significa que o backend não conhece mais este token: apague-o do
armazenamento e recomece o pareamento.

`context` é opcional e descreve a janela da sessão atual do Claude Code. Ele
não mede o consumo da assinatura; o firmware pode ignorá-lo até ter uma tela
para essa métrica.

## 3. As três regras que o display precisa respeitar

Estas não são casos de borda. São o produto — é o que impede o display de
mentir com confiança.

| Condição | O que mostrar | Por quê |
|---|---|---|
| `usedPercentage === null` | `--%`, barra vazia | A janela já reiniciou. O valor guardado descreve uma janela que não existe mais. **Nunca mostre o número antigo.** |
| `hasData === false` | "Sem dados" | O Claude Code ainda não respondeu nada nesta sessão, ou a conta não é Pro/Max. **Não é 0%.** |
| `stale === true` | Idade + aviso | O dado está velho; o Claude Code provavelmente está fechado. |

`ageSeconds` é sempre um **limite superior de frescor**: o percentual vem da
última resposta da API na sessão do Claude Code, então mesmo `ageSeconds: 0`
significa "na última interação", nunca "agora".

## Notas de implementação

- **Polling do `/usage`:** 30s é um bom começo.
- **`hardware_id`:** derive do MAC (`ESP.getEfuseMac()`) — estável entre boots,
  único por placa, nada a configurar.
- **URL base:** configure `API_HOST` (nome Bonjour da máquina, ex.:
  `minha-maquina.local`) e `API_PORT` em `firmware/src/config/api_config.h`. O
  ESP32 resolve o nome por mDNS (`ESPmDNS`) e guarda o endereço; se uma
  requisição falha, ele resolve de novo no próximo ciclo.
- **Token:** guarde na NVS (`Preferences.h`), não no `secrets.h`. Isso permite
  rotacionar sem reflashar e é onde um captive portal escreveria depois.
- **TLS:** o backend hoje é HTTP puro em LAN. Mantenha o cliente HTTP atrás de
  um wrapper fino para que trocar `WiFiClient` por `WiFiClientSecure` fique
  localizado num arquivo.
- **`localhost` não funciona no ESP32.** Use o nome `.local` da máquina (ou o
  IP da LAN, se a sua rede bloquear mDNS).
