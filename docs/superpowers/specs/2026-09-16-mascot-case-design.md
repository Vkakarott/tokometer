# tokEsp — Case mascote para a Heltec WiFi LoRa 32 V2

**Data:** 2026-09-16
**Status:** Validado no Fusion, pendente de impressão de teste

## Objetivo

Uma case de mesa, impressa em 3D, no estilo de um mascote em blocos: corpo
retangular, dois braços laterais e quatro pernas, com o OLED como rosto. A case
abriga a placa com os pinos soldados e uma bateria LiPo, para o display
funcionar sem cabo.

## Histórico de decisões

- **Monitor descartado.** O relevo da moldura inferior do Dell P2719H é
  específico demais para um clipe confiável.
- **Cabeça de IA descartada.** A cabeça facetada (referência Ultron/Visão)
  passou por três versões e foi trocada pelo mascote minimalista. As versões
  continuam no Fusion (`tokEsp AI Head Case` v1–v3).
- **Mascote maciço descartado.** A primeira versão do mascote tinha topo e base
  maciços para manter a proporção. A versão final é oca, com paredes de 2 mm e
  guias internas.

## Não-objetivos

- Mostrar o nível da bateria no OLED (mudança de firmware, fica para depois).
- Expor a antena LoRa. O firmware não usa LoRa; a antena de Wi-Fi é a de mola
  da própria placa e funciona através do PLA.
- Furos de ventilação. Voltam, entre as pernas, se a placa esquentar demais
  durante a carga.

## Restrições

| Item | Valor | Origem |
|---|---|---|
| Placa | 51 × 25,5 mm | heltec.org, página da V2 |
| Posição de USB, PRG, RST e OLED | ± 1 mm | diagrama de pinagem e STL da Heltec |
| Face da placa até o vidro do OLED | 6,0 mm | estimativa |
| Altura dos botões | 2,5 mm | estimativa |
| Pinos soldados abaixo da placa | 9,0 mm | estimativa |
| Conector USB | micro-USB, numa ponta curta | informado pelo usuário |
| Bateria | compartimento para até 52 × 26 × 11 mm (ex.: 102050, 1000 mAh) | decisão |
| Impressão | Bambu Lab, PLA, bico 0,4 | informado pelo usuário |

O firmware mantém Wi-Fi e OLED sempre ligados (~80–120 mA). Uma bateria de
1000 mAh dura horas, não dias. Como a placa não tem chave liga/desliga, a
case tem uma chave SS12D00 em série no fio da bateria.

## Desenho

### Forma

- Corpo de 67,6 × 58,3 × 34,8 mm (89 mm de largura com os braços), em
  proporção de mascote: ~1,16 : 1, com o rosto a 40% do topo.
- A largura sai do conteúdo: 2 mm de parede além da ponta da placa, simétrica
  em torno do OLED. A altura sai da proporção.
- Braços ocos e pernas maciças.
- A frente é lisa. Só aparecem a janela do OLED (26 × 15 mm), o recorte em U do
  botão PRG e o furo do RST.

### Interior

- Paredes de 2,0 mm e folga de 0,3 mm em cada lado da placa.
- Três aletas no topo e três na base, com 1,6 mm de espessura. Elas guiam a
  placa e a bateria na montagem por trás, e cada uma cobre 1 mm da borda da
  frente da placa com um degrau onde a placa para.
- Batentes à direita limitam a placa e a bateria no comprimento, sem bloquear
  o caminho dos fios.
- A bateria fica atrás da placa. Uma espuma entre a bateria e a tampa empurra o
  conjunto contra os degraus.

### Interface

| Função | Solução |
|---|---|
| Micro-USB | Na lateral esquerda: rebaixo externo de 11,5 × 8,5 × 1 mm para a cabeça do plugue e furo de 7,8 × 3,4 mm na parede restante de 1 mm |
| PRG | Aba flexível de 7 × 7 mm, cortada em três lados e presa em cima, com pino até o botão. O firmware usa o PRG para trocar a tela de uso |
| RST | Furo de 1,8 mm, acionado com clipe |
| Chave | Rasgo na lateral direita para a alavanca da SS12D00. A chave fica num berço de duas nervuras e é colada |

### Fechamento e impressão

- A tampa traseira desliza num trilho em T e sai por cima.
- O corpo imprime de frente para baixo, sem suporte. As aletas apoiam a parede
  de trás, e nenhum vão sem apoio passa de ~15 mm.
- A tampa imprime deitada.

## Arquivos

- `hardware/case/mascot_case.py`: gerador do Fusion. Todas as medidas ficam no
  topo do arquivo.
- `hardware/case/mascot_body.3mf` e `hardware/case/mascot_lid.3mf`: peças para
  imprimir.
- Fusion: `tokEsp Mascot Case v2`, no *Default Project*.

## Verificação

1. **Encaixe:** imprimir e montar a placa. Conferir o OLED centrado na janela,
   o plugue USB entrando inteiro, a aba acionando o PRG e o furo alcançando o
   RST.
2. **Medidas:** se algo não encaixar, medir a placa com paquímetro (altura do
   OLED, dos botões, dos pinos e do conector USB) e ajustar as constantes do
   gerador.
3. **Tampa:** conferir o deslizamento e se ela fica presa só pelo atrito.
