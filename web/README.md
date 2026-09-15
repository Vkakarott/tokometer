# tokEsp web

Painel local para acompanhar os limites da assinatura Claude e autorizar um
display ESP32.

## Rodar localmente

O backend deve estar disponível em `http://localhost:8080`.

```bash
npm install
npm run dev
```

O Vite encaminha chamadas iniciadas por `/api` para o backend. A tela de uso
fica em `/` e o endereço para parear o dispositivo é `/pair`.

O servidor de desenvolvimento escuta em todas as interfaces (`host: true`),
então `http://<IP-DA-MAQUINA>:5173/pair` abre de qualquer aparelho da mesma
rede — é esse endereço que o display mostra.

O painel mostra separadamente os limites da assinatura e os tokens da janela
de contexto da sessão mais recente do Claude Code.

## Verificar

```bash
npm run build
npm run lint
```
