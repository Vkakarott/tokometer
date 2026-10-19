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
  "interval": 2
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

### Aprovação sem digitar código

Quem configura a placa pelo app não lê código nenhum. O app obtém o
`hardware_id` direto da placa (seção 4) e chama, **na própria máquina do
backend**:

`POST /pair/approve` com `{ "hardware_id": "esp32-a1b2c3d4" }` → `204`.

Isso aprova os códigos que essa placa já tem e o próximo que ela pedir, por 5
minutos. Para o firmware nada muda: o primeiro `POST /device/token` já responde
`200`. `GET /pair/status?hardware_id=...` devolve `{ "paired": true }` depois
que a placa troca a aprovação pelo token. As duas chamadas respondem `403` a
qualquer endereço que não seja loopback; de outra máquina só vale a aprovação
por `user_code`.

## 2. Ler o consumo

### `GET /usage`

Header: `Authorization: Bearer <access_token>`

Resposta `200`, com um bloco por provedor **ativo**, sempre na ordem Claude,
Codex, Cursor. Os ativos são os que estão visíveis nos Ajustes do app do Mac
(veja `PUT /providers` abaixo); enquanto o app nunca enviou essa escolha, os três
vêm. Um provedor ativo que nunca enviou nada chega com `hasData: false`. Um
provedor desativado não aparece no objeto, e o display não deve reservar espaço
para ele. `providers` pode vir vazio quando o usuário oculta todos.

```json
{
  "providers": {
    "claude": {
      "windows": [
        { "id": "five_hour", "usedPercentage": 23.5, "resetsAt": 1738425600 },
        { "id": "seven_day", "usedPercentage": null, "resetsAt": 1738857600 }
      ],
      "ageSeconds": 42,
      "stale": false,
      "hasData": true,
      "context": null
    },
    "codex": {
      "windows": [
        { "id": "five_hour", "usedPercentage": 38, "resetsAt": 1738443600 },
        { "id": "seven_day", "usedPercentage": 54, "resetsAt": 1738989136 }
      ],
      "ageSeconds": 10,
      "stale": false,
      "hasData": true,
      "context": null
    },
    "cursor": {
      "windows": [],
      "ageSeconds": 0,
      "stale": false,
      "hasData": false,
      "context": null
    }
  }
}
```

`401` significa que o backend não conhece mais este token: apague-o do
armazenamento e recomece o pareamento.

`context` só existe para o Claude, e só quando o collector usa a statusline.
Ele descreve a janela da sessão atual, não o consumo da assinatura; o firmware
pode ignorá-lo.

### Cursor billing windows

The ingest and device contracts also accept `included`, `api`, and `on_demand`
window IDs. These identify Cursor's included plan usage, API usage, and optional
on-demand spending allowance. Their `resetsAt` value is the billing cycle's end,
not a five-hour or seven-day reset. Keep these IDs separate from `five_hour` and
`seven_day`; do not convert one window type into another.

All three windows are optional. A missing window is unavailable, not zero.
`POST /ingest` requires a numeric percentage for each submitted window; collectors
must omit unavailable readings. `GET /usage` preserves the window IDs and returns
`usedPercentage: null` after the reported reset time, using the same freshness and
unknown-value rules as other providers. Percentages do not imply token counts or
an absolute token limit.

Cursor remains `hasData: false` until a collector submits a real snapshot. The
macOS app forwards its Cursor readings (see `macos/README.md`).

### `PUT /providers` (app do Mac)

Header: `Authorization: Bearer <collector token>`

```json
{ "enabled": ["claude", "codex"] }
```

Responde `204`. Substitui a lista de provedores ativos da identidade. Nomes
desconhecidos ou repetidos dão `400`. O app envia a lista ao abrir e sempre que
um provedor é mostrado ou ocultado nos Ajustes.

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

## 4. Modo de configuração (API da própria placa)

Sem rede salva, ou quando nenhuma rede salva responde, a placa abre o ponto de
acesso `tokEsp-xxxx` (sem senha) e atende em `http://192.168.4.1`:

| Rota | Corpo | Resposta |
|---|---|---|
| `GET /api/status` | — | `200` `{ "hardware_id": "esp32-a1b2c3d4" }` |
| `POST /api/wifi` | formulário `ssid`, `password` | `200` `{ "hardware_id": "..." }`, ou `400` `{ "error": "invalid_credentials" }` |

`ssid` aceita até 32 caracteres e `password` até 64 (vazio para rede aberta).
Depois de responder ao `POST`, a placa fecha o ponto de acesso e tenta a rede;
se não conseguir em 15 segundos, volta ao modo de configuração. Qualquer outra
rota devolve a página HTML de configuração manual.

Com redes salvas, a placa sai sozinha do modo de configuração a cada minuto
para tentá-las de novo, desde que ninguém esteja conectado ao ponto de acesso.
