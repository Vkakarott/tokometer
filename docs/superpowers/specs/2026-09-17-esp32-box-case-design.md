# tokometer — Case caixa para ESP32 DevKit com OLED externo

**Data:** 2026-09-17
**Status:** Gerada no Fusion e exportada, pendente de impressão de teste

## Objetivo

Uma caixa de mesa simples para a montagem que substituiu o Heltec: uma ESP32
DevKit (ESP-WROOM-32) com um módulo OLED I2C de 4 pinos separado. O display
fica na frente, e o cabo USB-C sai por trás.

## Decisões

- **Caixa lisa, não mascote.** A case do Heltec é um mascote; esta é uma caixa,
  porque o pedido foi "uma caixa simples".
- **Placa deitada no fundo, comprimento no sentido frente-fundo.** É o que põe o
  conector USB-C contra a parede de trás sem torcer a placa.
- **Dois furos na traseira, não um.** O BOOT troca as telas e o EN reinicia;
  como eles ficam um de cada lado do USB-C e a orientação da placa na caixa
  pode inverter, os dois ficam acessíveis.
- **Tampa por atrito, sem parafuso.** Mesma escolha da case do Heltec.

## Não-objetivos

- Bateria. Esta versão vive ligada ao USB.
- Furos de ventilação. Entram se a placa esquentar.
- Botão externo no painel. Exigiria solda; o furo na traseira resolve.

## Restrições

| Item | Valor | Origem |
|---|---|---|
| Placa | 51,5 × 28,5 × 1,6 mm | medida padrão da DevKit 30 pinos |
| Pinos soldados abaixo da placa | 7,0 mm | estimativa |
| Módulo OLED | 27,3 × 27,8 × 4,4 mm | medida padrão do módulo 0,96" |
| Endereço I2C | 0x3C | varredura no boot da placa real |
| Fios entre display e placa | 16 mm | conectores DuPont da montagem atual |
| Impressão | PLA, bico 0,4 | igual à case do Heltec |

## Desenho

- **Externo:** 33,1 × 32,4 × 76,5 mm (largura × altura × profundidade),
  paredes de 2,0 mm e folga de 0,3 mm por lado.
- **Placa:** apoiada em dois trilhos de 2,5 × 1,5 mm, com a face de baixo a
  7,5 mm do fundo, o que deixa os pinos soldados livres.
- **Display:** encostado por dentro na parede da frente, preso por duas
  nervuras de 0,7 mm nas laterais e quatro batentes de canto atrás.
- **Janela:** 24 × 14,5 mm, centrada na altura do módulo.

## Interface

| Função | Solução |
|---|---|
| USB-C | Rasgo de 13 × 7 mm na traseira, dimensionado para a capa do cabo |
| BOOT e EN | Dois furos de 5 mm, a 11 mm de cada lado do centro |
| Tampa | Placa de 2 mm com lábio de 3 mm que entra por atrito (folga 0,25 mm) |

## Impressão

- **Corpo:** com a face fechada de cima apoiada na mesa e a abertura para
  cima. Assim nenhuma parede precisa de suporte e não há vão longo para
  atravessar no ar.
- **Tampa:** deitada.

## Arquivos

- `hardware/case/box_case.py`: gerador do Fusion; todas as medidas ficam no
  topo do arquivo.
- `hardware/case/box_case_body.3mf` e `hardware/case/box_case_lid.3mf`: peças
  para imprimir.

## Verificação

1. **Janela:** medir, no módulo real, a distância da borda de baixo até o vidro.
   Se o vidro não ficar centrado na janela, ajustar `WINDOW_OFFSET_Y`.
2. **Encaixe:** imprimir e montar. Conferir o cabo USB-C entrando, os dois
   furos alcançando os botões e o display parado contra a frente.
3. **Medidas da placa:** se algo não encaixar, medir com paquímetro e corrigir
   as constantes do topo do gerador.
4. **Fios:** se você soldar os fios em vez de usar DuPont, `WIRE_GAP` pode cair
   para 8 mm, encurtando a caixa em 8 mm.

## Como rodar o gerador

O script roda dentro do Fusion (Scripts and Add-Ins, ou pelo MCP). Ele constrói
num documento de rascunho e exporta os dois `.3mf`. O MCP acusa erro quando o
script troca de documento, porque guarda referências durante a chamada: o que
vale é o log em `/tmp/tokometer_case.log` e os arquivos exportados.
