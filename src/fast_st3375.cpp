#include "fast_st3375.h"
#include <SPI.h>

Fast_ST7735::Fast_ST7735(int8_t cs, int8_t dc, int8_t rst)
  : Adafruit_ST7735(cs, dc, rst)
{
}

void Fast_ST7735::swapendian(uint8_t *buffer, uint32_t size) {
  for (int i = 0; i < size; i+=2) {
    uint8_t tmp = buffer[i+1];
    buffer[i+1] = buffer[i];
    buffer[i] = tmp;
  }
}

void Fast_ST7735::writedata(uint8_t *buffer, uint32_t size) {
  digitalWrite(_dc, HIGH);
  digitalWrite(_cs, LOW);
  SPI.writeBytes(buffer, size);
  digitalWrite(_cs, HIGH);
}

void Fast_ST7735::setAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
  x0 += xstart;
  x1 += xstart;
  y0 += ystart;
  y1 += ystart;
  writecommand(ST7735_CASET);
  const uint8_t caset[] = {0x00, x0, 0x00, x1};
  writedata((uint8_t *)caset, sizeof(caset));
  writecommand(ST7735_RASET); // Row addr set
  const uint8_t raset[] = {0x00, y0, 0x00, y1};
  writedata((uint8_t *)raset, sizeof(raset));
  writecommand(ST7735_RAMWR); // write to RAM
}

void Fast_ST7735::drawBuffer(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t *buffer) {
  // rudimentary clipping (drawChar w/big text requires this)
  if((x >= _width) || (y >= _height)) return;
  if((x + w - 1) >= _width) w = _width - x;
  if((y + h - 1) >= _height) h = _height - y;

  setAddrWindow(x, y, x+w-1, y+h-1);
  writedata((uint8_t *)buffer, w*h*2);
}
