# tokEsp — Migração para aplicativo macOS com display opcional

**Status:** Em andamento — Fase 1 concluída

## Objetivo

Transformar o tokEsp de um conjunto de collector, backend, painel web e
firmware em um aplicativo nativo para macOS que mostra o consumo de Claude e
Codex em uma interface discreta de borda de tela. O ESP32 passa a ser uma
extensão opcional do aplicativo, e não o motivo pelo qual o sistema existe.

O produto final deve esconder infraestrutura local: o usuário instala um app,
autoriza as fontes já existentes no Mac, escolhe o que deseja acompanhar e,
opcionalmente, adiciona um display.

## Decisões de produto

- O aplicativo macOS é a fonte de verdade dos dados e das configurações.
- O MVP suporta somente Claude e Codex, com uma conta de cada.
- A interface tipo notch é a superfície primária; o display OLED é uma
  superfície secundária.
- O produto é local-first. Nenhuma credencial de Claude ou Codex sai do Mac e
  o ESP32 só recebe um snapshot normalizado, nunca tokens dos provedores.
- O painel web deixa de fazer parte do fluxo de uso e de pareamento. Pode
  sobreviver provisoriamente apenas como ponte de migração.
- O projeto usa o CodeNotch como referência de arquitetura e UX, não como
  dependência de runtime nem como código a ser copiado sem adaptação.

## Arquitetura-alvo

```text
Claude Code / Claude Desktop ─┐
                              ├─> Providers ─> UsageStore ─┬─> Notch
Codex local sign-in ──────────┘                            ├─> Settings
                                                           └─> DeviceLink (LAN)
                                                                   └─> ESP32 OLED
```

O aplicativo terá estes módulos lógicos:

| Módulo | Responsabilidade |
|---|---|
| `Providers` | Descobrir contas, ler uso, aplicar backoff e reportar fidelidade da fonte. |
| `UsageStore` | Guardar a última leitura válida, calcular frescor e publicar um snapshot único. |
| `Notch` | Exibir o estado resumido na borda da tela e abrir detalhes sob demanda. |
| `Settings` | Configurar provedores, aparência, notificações e dispositivos. |
| `DeviceLink` | Descobrir, parear, revogar e servir snapshots para ESP32s na LAN. |
| `Diagnostics` | Mostrar fonte, última atualização e falhas recuperáveis sem expor segredos. |

### Contrato canônico de apresentação

`UsageStore` publica um modelo independente de qualquer tela:

```text
ProviderSnapshot
  id, displayName, fidelity, status, headlineWindow, windows, account

Window
  id, label, usedFraction | unknown, resetsAt | unknown

Status
  ok | stale(since) | needsAuth | accessDenied | unsupported(reason) | error(reason)
```

O notch, as configurações e o firmware consomem esse modelo. Nenhuma dessas
camadas pode ler `auth.json`, Keychain ou uma resposta bruta de fornecedor.

## Mapeamento do sistema atual

| Atual | Destino | Estratégia |
|---|---|---|
| `collector/usage_poll.sh` | Provider Claude | Manter como ponte; substituir por adapter nativo. |
| `collector/codex_usage_poll.sh` | Provider Codex | Manter como ponte; substituir por adapter nativo. |
| `backend/src/core/*` | `UsageStore` | Reaproveitar regras de frescor; reimplementar no domínio do app. |
| `backend/src/http/*` | `DeviceLink` | Substituir por API local do aplicativo, versionada. |
| `web/` | `Notch` e `Settings` | Não portar a interface; preservar apenas comportamento útil. |
| `firmware/src/display/*` | Firmware | Manter layouts e estados; trocar somente a camada de rede e pareamento. |

## Plano de implementação

### Fase 0 — Especificação e métricas de paridade

**Meta:** congelar o comportamento que não pode regredir antes de introduzir a
nova interface.

- [ ] Documentar snapshots de referência para Claude e Codex: dado válido,
  janela virada, dado antigo, sem dado, autenticação expirada e erro de rede.
- [x] Definir o contrato `ProviderSnapshot` e as regras de conversão a partir
  do atual `ProvidersView`.
- [ ] Definir métricas de aceite: percentual e reset iguais ao sistema atual;
  dado inválido nunca aparece como percentual válido; estado de frescor sempre
  visível.
- [ ] Definir o escopo de macOS suportado e o modelo de distribuição do MVP.

**Saída:** fixtures compartilhadas e uma especificação do snapshot. Nenhuma
mudança de produção nesta fase.

### Fase 1 — Shell do aplicativo e adaptador de compatibilidade

**Meta:** validar a experiência tipo notch sem alterar a coleta que já
funciona.

- [x] Criar o aplicativo nativo macOS com processo de longa duração, item de
  menu e uma janela de configurações.
- [x] Implementar o notch compacto, detalhes ao interagir e estados visuais
  para `ok`, `stale`, `needsAuth` e `error`.
- [x] Criar um `LegacySnapshotProvider` que lê o snapshot do backend existente
  em `localhost` e o transforma no contrato canônico.
- [x] Adicionar um modo de demonstração determinístico para validar layout e
  estados sem conta conectada.
- [x] Persistir preferências de UI disponíveis nesta etapa: posição,
  provedores visíveis e modo de demonstração.

**Critério de aceite:** por sete dias de uso normal, o app e o painel atual
apresentam os mesmos números e o app mostra explicitamente qualquer dado
desatualizado ou ausente.

**O que continua existindo:** backend, collectors, launchd, painel web e
firmware atuais. Eles são a ponte, não o desenho definitivo.

### Fase 2 — Providers nativos para Claude e Codex

**Meta:** o aplicativo passa a coletar e interpretar dados sem scripts,
backend ou configuração manual de hooks.

- [ ] Criar a interface de provider: descoberta, leitura, intervalo de polling,
  backoff, fidelidade da fonte e estado de autenticação.
- [ ] Implementar o provider Codex lendo o login local, sem copiar, renovar ou
  transmitir a credencial.
- [ ] Implementar o provider Claude com caminhos ordenados por qualidade:
  fonte local/documentada quando disponível, CLI instalada e Keychain como
  fallback explícito.
- [ ] Persistir a última leitura boa e a data da leitura; nunca persistir
  respostas brutas ou credenciais.
- [ ] Implementar backoff exponencial e limites por provedor, persistindo a
  próxima tentativa após resposta limitada.
- [ ] Expor em Configurações a origem dos dados, a última leitura e uma ação
  de atualizar agora que respeite o rate limit.
- [ ] Criar testes de parsing, transições de estado, rollover de janelas e
  renovação/autenticação expirada.

**Critério de aceite:** o aplicativo pode ser instalado em um Mac limpo com
Claude/Codex já autenticados e começar a mostrar os dois provedores sem o
usuário editar `.env`, `config.sh`, hooks ou `config.toml`.

### Fase 3 — Configurações e onboarding de produto

**Meta:** substituir conhecimento técnico por passos claros dentro do app.

- [ ] Criar onboarding: detectar Claude e Codex, explicar permissões e mostrar
  o resultado de cada verificação.
- [ ] Permitir ativar, ocultar, reordenar e atualizar cada provider.
- [ ] Adicionar tela de diagnósticos com linguagem de produto: "dados de 12 min
  atrás", "abra o Codex para renovar o login", sem URLs, tokens ou logs brutos.
- [ ] Adicionar início automático no login pelo mecanismo nativo do macOS.
- [ ] Substituir o dashboard por detalhes sob demanda no notch e pela tela de
  Configurações. Não recriar um dashboard web completo.

**Critério de aceite:** a primeira configuração leva poucos minutos e não
exige terminal, edição de arquivo ou firmware.

### Fase 4 — Serviço local e pareamento do display

**Meta:** substituir o backend Node por uma API local pertencente ao app.

- [ ] Criar `DeviceLink` que escuta somente em endereços privados da LAN e se
  reconfigura quando a rede muda.
- [ ] Publicar descoberta por mDNS/Bonjour para que o ESP32 não receba host ou
  IP compilado.
- [ ] Definir `GET /snapshot` versionado, retornando apenas o contrato canônico
  reduzido para o display.
- [ ] Implementar o ciclo de pareamento: janela aberta pelo usuário, código de
  uso único, expiração de cinco minutos e credencial derivada por dispositivo.
- [ ] Autenticar as consultas com identificador de dispositivo, timestamp,
  nonce e assinatura; rejeitar replay, dispositivo revogado e origem fora da
  LAN.
- [ ] Guardar segredos de dispositivos no Keychain do Mac e no armazenamento
  persistente do ESP32. Remover um dispositivo deve revogar sua credencial nos
  dois lados.
- [ ] Criar testes de protocolo com vetores compartilhados entre app e
  firmware.

**Critério de aceite:** um ESP32 já conectado ao Wi-Fi pareia pelo aplicativo,
continua funcionando após reiniciar o Mac e deixa de acessar dados
imediatamente quando revogado.

### Fase 5 — Migrar o firmware para DeviceLink

**Meta:** o display torna-se um cliente do aplicativo, preservando a interface
OLED atual.

- [ ] Trocar a configuração estática de `API_HOST` pelo host descoberto via
  mDNS durante o pareamento.
- [ ] Substituir o fluxo atual de `/device/code`, `/device/token` e `/usage`
  pelo protocolo `DeviceLink` versionado.
- [ ] Adaptar o parser do firmware do `ProvidersView` atual para o snapshot
  reduzido, preservando as regras de `sem dados`, `janela virada` e `stale`.
- [ ] Mostrar estados específicos: procurando app, aguardando pareamento,
  pareado sem dados e conexão perdida.
- [ ] Executar testes de compatibilidade de protocolo e testes manuais contra
  uma placa real.

**Critério de aceite:** o display nunca exige regravação ao trocar o IP do Mac
e não recebe credenciais dos provedores.

### Fase 6 — Provisionamento de Wi-Fi do ESP32

**Meta:** eliminar SSID e senha compilados em `secrets.h`.

- [ ] Escolher um único caminho de provisionamento para o MVP: portal cativo
  ou BLE. A decisão deve considerar simplicidade no iPhone e Android, não só
  facilidade de firmware.
- [ ] Implementar modo de primeira configuração e recuperação após falha de
  Wi-Fi.
- [ ] Guardar credenciais de rede apenas na NVS do dispositivo.
- [ ] Permitir redefinir rede pelo botão físico, sem apagar a identidade do
  dispositivo até confirmação do usuário.

**Critério de aceite:** uma pessoa consegue colocar uma placa nova na rede sem
PlatformIO, serial, código-fonte ou senha compilada.

### Fase 7 — Retirada controlada da arquitetura antiga

**Meta:** remover somente o que deixou de ser necessário.

- [ ] Manter o `LegacySnapshotProvider` por uma versão de transição, marcado
  como legado nos diagnósticos.
- [ ] Comparar leituras do provider nativo e do legado em instalações de teste
  por pelo menos duas semanas.
- [ ] Remover collectors, backend, painel web e scripts de serviço quando a
  paridade estiver comprovada e o display usar `DeviceLink`.
- [ ] Atualizar README, instruções de instalação e procedimentos de suporte.
- [ ] Adicionar distribuição assinada/notarizada e mecanismo de atualização
  antes de tratar o app como produto para terceiros.

## Ordem de entrega do MVP

1. Fases 0 e 1: provar que a interface desktop entrega valor com os dados
   atuais.
2. Fase 2: eliminar a configuração técnica que impede adoção.
3. Fase 3: tornar o aplicativo instalável e compreensível.
4. Fases 4 e 5: trazer o display como bônus funcional.
5. Fase 6: tornar o hardware configurável por pessoas não técnicas.

O display não bloqueia o lançamento do aplicativo. O aplicativo também não
deve esperar o provisionamento perfeito do ESP32 para validar a proposta.

## Riscos e decisões futuras

| Risco | Mitigação |
|---|---|
| Endpoints de uso não documentados mudam | Providers isolados, fixtures por resposta, backoff e estado de erro honesto. |
| Permissões do Keychain mudam entre builds | Assinatura estável do app, autorização explícita e fallback pela CLI quando possível. |
| App e legado mostram números diferentes | Período de paridade com snapshots registrados sem dados sensíveis. |
| Exposição do servidor local | Bind apenas em LAN privada, janela curta de pareamento, autenticação por dispositivo e revogação. |
| Escopo explode com muitos provedores | Manter Claude e Codex como única meta do MVP. |
| Wi-Fi do ESP32 atrasa a entrega | Manter Wi-Fi compilado no protótipo e tratar provisionamento como fase própria. |

## Fora de escopo do MVP

- Aplicativo iOS/Android.
- Windows ou Linux.
- Cursor, Copilot, Ollama, LM Studio ou outros provedores.
- Múltiplas contas por provedor.
- Sincronização em nuvem.
- Alertas complexos de sessões/agentes.
- Bateria, telemetria ou novas telas do mascote.
