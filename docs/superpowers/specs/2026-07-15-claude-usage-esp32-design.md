# tokEsp — Display físico de consumo da assinatura Claude

> Histórico: este documento registra as decisões de desenho originais. Para o
> comportamento e as rotas atuais, consulte o `README.md` e `docs/device-api.md`.

**Data:** 2026-07-15
**Status:** Aprovado, pendente de plano de implementação

## Objetivo

Mostrar num display OLED (Heltec WiFi LoRa 32 V2) quanto da **assinatura Claude
Pro/Max** já foi consumido nas janelas de 5 horas e 7 dias, com indicação de
quando cada janela reseta.

## Não-objetivos

Explicitamente fora de escopo desta versão:

- Consumo/custo da **API** da Anthropic (é outra conta, outro produto).
- Contas de usuário, multi-tenancy, cobrança.
- TLS no ESP32.
- Captive portal para Wi-Fi.
- Outros providers (Cursor, Copilot). O formato de dados é neutro, mas nenhum
  collector além do Claude Code será escrito agora.

## Restrições descobertas na pesquisa

Estas restrições vêm da documentação oficial e **definem o desenho**. Quem for
implementar deve entendê-las antes de discordar de qualquer decisão abaixo.

### Não existe API de consumo de assinatura

A Admin API (`/v1/organizations/usage_report/messages`, `/v1/organizations/cost_report`)
cobre exclusivamente faturamento de **API**, exige credencial de admin da
organização, e não tem relação com assinaturas Pro/Max. Não há endpoint público
para "quanto resta da minha assinatura".

Também não existe OAuth de terceiros na Anthropic: o caminho documentado para
software de terceiros é API key, e o `ant auth login` é o cliente OAuth
first-party da própria Anthropic. Uma tela de "login no provider" é
irrealizável.

### O statusline do Claude Code expõe o dado

O Claude Code invoca um script configurável e entrega JSON no stdin, contendo:

```json
"rate_limits": {
  "five_hour": { "used_percentage": 23.5, "resets_at": 1738425600 },
  "seven_day": { "used_percentage": 41.2, "resets_at": 1738857600 }
}
```

Propriedades relevantes, todas da doc oficial:

- **Só existe para assinantes Pro/Max**, e só **após a primeira resposta da API
  na sessão**. Ausente antes disso. Cada janela pode faltar independentemente.
- Requer Claude Code **>= 2.1.92**. (Ambiente atual: 2.1.126.)
- O script roda após cada mensagem do assistente, com **debounce de 300ms**.
- **Execução em voo é cancelada** se um novo update disparar. Trabalho lento no
  script trava o render e apanha cancelamento.
- `refreshInterval` re-executa o script em timer fixo durante ociosidade.
- É exclusivo do Claude Code. **Claude Desktop não tem statusline.**

### Consequência: o dado nunca é "agora"

`rate_limits` reflete a **última resposta da API daquela sessão**. Durante
ociosidade o `refreshInterval` re-executa o script, mas o JSON continua trazendo
o percentual antigo. Carimbar `updatedAt = now()` a cada push faz dado velho
parecer fresco.

O percentual é sempre *"na sua última interação com o Claude Code"*, nunca
*"agora"*. Nenhum design resolve isso — é inerente ao mecanismo. O produto deve
comunicar, não esconder.

Como os limites de 5h/7d são da **assinatura** (não do Claude Code), o
percentual já reflete uso do Desktop e da web. Mas **só atualiza enquanto o
Claude Code estiver aberto**.

## Arquitetura

O dado é **empurrado**, não buscado. A fonte é a máquina do usuário; o backend é
uma caixa de correio, não um fetcher. Não há credencial da Anthropic em lugar
nenhum do sistema — quem está autenticado é o Claude Code do usuário.

```
Claude Code ──statusline (push, background)──► Backend ──Bearer──► ESP32
             rate_limits.{five_hour,seven_day}    │  (último snapshot + observedAt)
                                                  └──────────────► /web
```

| Componente | Responsabilidade | Depende de |
|---|---|---|
| `collector/` | Extrai `rate_limits` do stdin, faz POST em background, imprime a linha | config (URL + token) |
| `backend/core/` | Snapshot, pareamento, resolução de identidade | nada |
| `backend/http/` | Rotas, auth | `core/` |
| `firmware/` | Polling, render OLED, pareamento | — |
| `web/` | Dashboard read-only + tela de aprovação de pareamento | backend |

O core não conhece o framework HTTP. O adaptador é fino. Isso mantém a decisão
de hospedagem em aberto.

## Pareamento do ESP32: OAuth Device Authorization Grant (RFC 8628)

**Decisão central.** Nenhum humano transporta segredo até o device. O device
mostra um código curto; o humano digita esse código no `/web`; o backend entrega
o token direto ao device.

```
1. ESP32 liga    → POST /device/code    → { device_code, user_code: "K7QM-3F9A", interval: 5 }
2. OLED mostra   → "tok.local/pair"  +  "K7QM-3F9A"
3. ESP32 polling → POST /device/token   → 400 { error: "authorization_pending" }
4. Humano        → abre /pair, digita K7QM-3F9A, aprova
5. Próximo poll  → 200 { access_token } → grava na NVS → nunca mais pergunta
```

Motivação — vale **hoje**, não só como preparo para o futuro:

- Nenhum token é gerado, copiado ou guardado por humano.
- Segundo device usa o mesmo binário, sem edição.
- Revogar = apagar uma linha. Sem reflash.
- Idêntico para uso pessoal e para cliente. Adicionar contas só acrescenta
  exigência de sessão no passo 4.

`user_code`: 8 caracteres exibidos em dois grupos de 4 (`K7QM-3F9A`), alfabeto
de 30 símbolos sem ambiguidade — `ABCDEFGHJKMNPQRSTVWXYZ23456789`, removendo
`I`, `L`, `O`, `U`, `0` e `1`. TTL de 5 minutos, invalidado após uso.

**Risco aceito:** sem login no `/web`, qualquer um na rede local que abrir
`/pair` e adivinhar um código ativo aprova um device. Mitigado por TTL curto e
espaço de busca grande. Deixa de existir quando entrarem contas.

### Por que o collector não usa device flow

Device flow existe para resolver ausência de teclado. O collector roda numa
máquina com shell, onde o usuário já vai editar `settings.json`. Token em
arquivo de config é a resposta certa ali; device flow seria cerimônia sem ganho.

## Contratos

Formato neutro, para não vazar o shape da Anthropic para o firmware:

```ts
type UsageWindow = {
  id: "five_hour" | "seven_day";
  usedPercentage: number;   // 0-100
  resetsAt: number;         // unix epoch seconds
};

type Snapshot = {
  provider: string;         // "claude-code"
  windows: UsageWindow[];   // pode vir vazio ou parcial
  observedAt: number;       // quando o collector rodou (limite superior de frescor)
  source: string;           // hostname da máquina que reportou
};
```

| Rota | Auth | Corpo / Resposta |
|---|---|---|
| `POST /ingest` | Bearer (collector) | `Snapshot` → `204` |
| `GET /usage` | Bearer (device) | `Snapshot & { ageSeconds }` |
| `POST /device/code` | — | `{ hardware_id }` → `{ device_code, user_code, verification_uri, expires_in, interval }` |
| `POST /device/token` | — | `{ device_code }` → `200 { access_token }` ou `400 { error }` |
| `POST /pair/approve` | — (sessão, no futuro) | `{ user_code }` → `204` |
| `GET /usage/web` | — | `Snapshot & { ageSeconds }` |

Erros de `/device/token` conforme RFC 8628: `authorization_pending`,
`slow_down`, `expired_token`, `access_denied`.

## Tratamento de frescor e virada de janela

Regra que o firmware e o `/web` aplicam igualmente:

1. `now > window.resetsAt` → a janela virou; o `usedPercentage` guardado é lixo.
   Exibir **estado desconhecido**, nunca o número velho.
2. `ageSeconds` alto → dado velho. Exibir a idade ("há 12min") e atenuar.
3. `windows` vazio → o Claude Code ainda não respondeu nada nesta sessão, ou a
   conta não é Pro/Max. Exibir estado próprio, distinto de "0%".

Estes três estados são **produto**, não caso de borda. É o que impede o display
de mentir.

## Dobradiças para virar produto

Baratas agora, evitam reescrita depois. Nenhuma delas é multi-tenancy.

| Hoje | Vira |
|---|---|
| Snapshot em mapa com chave `"default"` | Chave = ID de usuário |
| `resolveToken(token) → identity` como função | Consulta a banco |
| Token do device na NVS | Captive portal escreve no mesmo lugar |
| Cliente HTTP do firmware atrás de wrapper | `WiFiClient` → `WiFiClientSecure` |

## Stack

- **backend:** Node + TypeScript, `strict: true`. Fastify como adaptador HTTP —
  escolhido por dar roteamento e validação de schema baratos. O `core/` não o
  importa, então trocá-lo é reescrever só `http/`. Persistência: arquivo JSON —
  sobrevive restart, sem banco.
- **collector:** shell + `jq`. POST com `&` (background) para não travar o
  render nem apanhar cancelamento.
- **web:** Vite + React (já existe, hoje é scaffold cru).
- **firmware:** PlatformIO/Arduino, U8g2 (já existe WiFi e display funcionando).

## Testes

- `core/`: snapshot, virada de janela, expiração de código, resolução de token.
  Puros, sem I/O.
- `collector`: alimentar JSON mock no stdin, incluindo `rate_limits` ausente e
  parcial.
- `firmware`: os três estados de frescor com respostas mockadas.

## Decisões em aberto

- Hospedagem (local vs VPS vs serverless) — deliberadamente adiada; o core é
  agnóstico.
- Intervalo de polling do firmware: começar em 30s e ajustar.
- Layout exato do OLED (128x64): duas barras + reset mais próximo + idade.
