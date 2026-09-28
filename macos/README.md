# tokEsp para macOS

Primeira entrega do aplicativo nativo do tokEsp. Ela apresenta uma interface
compacta na borda superior da tela e uma janela de configurações, mas usa o
backend atual como fonte temporária de dados.

## Rodar

Mantenha o backend atual ativo em `http://127.0.0.1:43110` e execute:

```bash
cd macos
swift run TokEsp
```

O ícone de menu permite atualizar a leitura, mostrar ou ocultar o tokonotch e abrir
as configurações.

## Iniciar com o Mac

```bash
cd macos && ./scripts/install-app.sh
```

O instalador copia o app para `~/.local/share/tokesp`, registra o LaunchAgent
`com.tokesp.app` e o inicia a cada login. Para remover o início automático:

```bash
launchctl bootout gui/$(id -u)/com.tokesp.app
rm ~/Library/LaunchAgents/com.tokesp.app.plist
```

## Estado da migração

Esta é a fase de compatibilidade. O app ainda não substitui os collectors, o
backend, o painel web nem o firmware. A próxima fase move Claude e Codex para
providers nativos do aplicativo.

## Verificar

```bash
cd macos
swift test
```
