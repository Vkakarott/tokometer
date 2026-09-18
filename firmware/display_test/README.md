# Teste temporário do display

Este firmware é independente da aplicação e alterna automaticamente entre:

- tela inteira branca por 3 segundos;
- uma borda de 1 pixel nos quatro limites do painel por 3 segundos.

Para gravar no ESP32 DevKit com OLED I2C externo:

```bash
pio run -e display_test -t upload
pio device monitor -b 115200
```

O teste usa SDA `21`, SCL `22`, endereço I2C `0x3C` e controlador SSD1306.
Se a imagem aparecer deslocada, adicione `-DTOKESP_DISPLAY_SH1106` aos
`build_flags` do ambiente `display_test` em `platformio.ini`.

Depois da medição, remova o ambiente `display_test` de `platformio.ini` e a
pasta `firmware/display_test/`.
