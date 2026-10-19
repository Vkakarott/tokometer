# Tokometer para Windows

Ícone na bandeja do sistema que mostra o consumo das assinaturas de Claude,
Codex e Cursor deste PC. Só aparecem os provedores com login neste
computador. É a versão Windows do app do Mac (`macos/`): lê o login que cada ferramenta já guarda no computador e consulta o serviço dela.
Não precisa de backend nem do display ESP32.

## Instalar

1. Baixe `Tokometer-windows-x64.zip` na página de Releases do repositório.
2. Extraia e abra `Tokometer.exe`. Não há instalador e não precisa de
   administrador; o .NET já vai dentro do executável.
3. O Windows pode mostrar "O Windows protegeu o computador", porque o
   executável não é assinado. Clique em **Mais informações → Executar assim mesmo**.

Ao abrir, o painel de consumo aparece e um aviso diz que o app está rodando.
O ícone fica perto do relógio (no Windows 11, normalmente dentro da seta
**^**; arraste-o para fora para deixá-lo sempre visível). Abrir de novo com o
app já rodando só avisa que ele está aberto. Se algo der errado, uma mensagem
aparece e o erro fica em `%LOCALAPPDATA%\Tokometer\erro.log`. O arco do ícone mostra o maior
consumo entre os provedores visíveis: verde, laranja a partir de 70% e
vermelho a partir de 90%.

- **Clique:** abre o painel com o consumo de cada provedor.
- **Botão direito:** Atualizar agora, Ajustes e Sair.
- **Ajustes:** escolha quais provedores aparecem e se o app inicia com o
  Windows (ligado por padrão).

## De onde vêm os dados

| Provedor | Login lido | Frequência |
|---|---|---|
| Claude | Sessão do app Claude (`%APPDATA%\Claude`); se não houver, `%USERPROFILE%\.claude\.credentials.json` (Claude Code) | App: a cada 5 min. Claude Code: a cada 15 min; 1 min perto do limite de 5 h |
| Codex | `%USERPROFILE%\.codex\auth.json` (Codex CLI) | A cada 1 min |
| Cursor | `%APPDATA%\Cursor\User\globalStorage\state.vscdb` | A cada 5 min |

Um provedor sem login neste computador não aparece no painel; ele surge sozinho
quando você entra na ferramenta. Um login recusado pelo serviço aparece com o
aviso para entrar de novo.

Para o app Claude, o Tokometer lê o cookie de sessão que o app guarda (protegido
pelo Windows para o seu usuário) e consulta o consumo no claude.ai, como a
página de uso do próprio app. Essa consulta não é documentada pela Anthropic e
pode mudar. Se o app estiver com os cookies bloqueados, o Tokometer usa o último
login lido; na primeira vez, pode pedir para fechar o app Claude uma vez.

Uma falha temporária mantém o último valor, marcado com a idade do dado. Um valor
de janela que já reiniciou aparece como `--%`, nunca como 0%.

Variáveis opcionais: `CODEX_HOME` e `CLAUDE_CONFIG_DIR` mudam onde o login é
procurado, e `TOKOMETER_STATE_DIR` muda onde o app guarda o estado
(padrão `%LOCALAPPDATA%\Tokometer`).

## Display ESP32 (opcional)

Se existir `%USERPROFILE%\.config\tokesp\config.sh` com `TOKESP_URL` e
`TOKESP_TOKEN`, o app também envia as leituras e os provedores visíveis ao
backend, como o app do Mac. Sem esse arquivo, nada é enviado.

## Desenvolver

```bash
cd windows
dotnet test Tokometer.sln          # roda em qualquer sistema
dotnet build Tokometer.sln         # compila também no macOS e no Linux
```

`Tokometer.Core` tem a coleta e a formatação e roda em qualquer sistema.
`Tokometer.App` é a interface WPF e só abre no Windows.

## Publicar uma versão

O workflow `.github/workflows/windows-app.yml` compila numa máquina Windows,
roda os testes e, para uma tag `windows-v*`, anexa o zip a uma Release:

```bash
git tag windows-v0.1.0
git push origin windows-v0.1.0
```
