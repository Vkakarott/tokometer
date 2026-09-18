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

O ícone de menu permite atualizar a leitura, mostrar ou ocultar o notch e abrir
as configurações.

## Estado da migração

Esta é a fase de compatibilidade. O app ainda não substitui os collectors, o
backend, o painel web nem o firmware. A próxima fase move Claude e Codex para
providers nativos do aplicativo.

## Verificar

```bash
cd macos
swift test
```
