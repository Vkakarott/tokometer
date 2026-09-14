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

## Verificar

```bash
npm run build
npm run lint
```
