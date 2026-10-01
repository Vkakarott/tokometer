#include "display/display_manager.h"

#include <SPI.h>

#include "config/hardware_config.h"
#include "display/indexed_canvas.h"

namespace {
constexpr uint32_t SpiFrequency = 20000000;
constexpr uint16_t Width = IndexedCanvas::Width;
constexpr uint16_t Height = IndexedCanvas::Height;
uint8_t lineBuffer[Width * 2];
IndexedCanvas canvas;

void command(uint8_t value, const uint8_t *data = nullptr, size_t size = 0) {
    SPI.beginTransaction(SPISettings(SpiFrequency, MSBFIRST, SPI_MODE0));
    digitalWrite(HardwareConfig::TFT_CS_PIN, LOW);
    digitalWrite(HardwareConfig::TFT_DC_PIN, LOW);
    SPI.transfer(value);
    if (size != 0) { digitalWrite(HardwareConfig::TFT_DC_PIN, HIGH); SPI.writeBytes(data, size); }
    digitalWrite(HardwareConfig::TFT_CS_PIN, HIGH);
    SPI.endTransaction();
}
void window(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    const uint16_t xe=x+width-1, ye=y+height-1;
    const uint8_t columns[]={static_cast<uint8_t>(x>>8),static_cast<uint8_t>(x),static_cast<uint8_t>(xe>>8),static_cast<uint8_t>(xe)};
    const uint8_t rows[]={static_cast<uint8_t>(y>>8),static_cast<uint8_t>(y),static_cast<uint8_t>(ye>>8),static_cast<uint8_t>(ye)};
    command(0x2a,columns,sizeof columns); command(0x2b,rows,sizeof rows); command(0x2c);
}
void flush() {
    window(0,0,Width,Height);
    SPI.beginTransaction(SPISettings(SpiFrequency,MSBFIRST,SPI_MODE0));
    digitalWrite(HardwareConfig::TFT_CS_PIN,LOW); digitalWrite(HardwareConfig::TFT_DC_PIN,HIGH);
    for(uint16_t y=0;y<Height;++y) { for(uint16_t x=0;x<Width;++x) { const uint16_t color=canvas.pixelColor(x,y); lineBuffer[x*2]=color>>8; lineBuffer[x*2+1]=color; } SPI.writeBytes(lineBuffer,sizeof lineBuffer); }
    digitalWrite(HardwareConfig::TFT_CS_PIN,HIGH); SPI.endTransaction();
}
void drawCenteredText(const char *text,int16_t y,uint8_t size,uint16_t color) { int16_t x1,y1; uint16_t w,h; canvas.setFont(); canvas.setTextSize(size); canvas.getTextBounds(text,0,y,&x1,&y1,&w,&h); canvas.setTextColor(color); canvas.setCursor((Width-w)/2-x1,y); canvas.print(text); }
}

void initializeDisplay() {
    pinMode(HardwareConfig::TFT_CS_PIN,OUTPUT); pinMode(HardwareConfig::TFT_DC_PIN,OUTPUT); pinMode(HardwareConfig::TFT_RESET_PIN,OUTPUT);
    digitalWrite(HardwareConfig::TFT_CS_PIN,HIGH); SPI.begin(HardwareConfig::TFT_SCK_PIN,-1,HardwareConfig::TFT_MOSI_PIN,HardwareConfig::TFT_CS_PIN);
    digitalWrite(HardwareConfig::TFT_RESET_PIN,LOW); delay(10); digitalWrite(HardwareConfig::TFT_RESET_PIN,HIGH); delay(120);
    const uint8_t format=0x55, madctl=0x28; command(0x3a,&format,1); command(0x36,&madctl,1); command(0x11); delay(120); command(0x29); delay(50);
}
void drawApp(uint32_t) { canvas.fillScreen(TftColor::Background); drawCenteredText("SPEEDBIKE",126,3,TftColor::Cyan); flush(); }
