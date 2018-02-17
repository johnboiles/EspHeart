#include "fast_st3375.h"

// GPIO 0, 2 or 15 control the boot-mode of the ESP, maybe scary to use those
// #define sclk D5 // GPIO14 (SPI SCK)
// #define mosi D7 // GPIO13 (SPI MOSI)
// #define cs   D8 // GPIO15 (SPI SS)
#define cs   D0 // GPIO16
#define dc   D2 // GPIO4
#define rst  D1 // GPIO5

#define XSTART 2
#define YSTART 3
#define WIDTH 128
#define HEIGHT 128

#define K 0x0000 // Black
#define W 0xFFFF // White
#define B 0x3F6C // Blue (sky)
#define G 0xBE04 // Gray (cloud shadow, alt blue really)

Fast_ST7735 tft = Fast_ST7735(cs, dc, rst);

const uint16_t tile_a[] PROGMEM = {  // Left cloud tile
  B,B,B,B,B,B,B,B,B,K,K,B,B,B,B,B,B,B,B,B,B,B,B,B,
  B,B,B,B,B,B,B,B,K,W,W,K,B,B,B,B,B,B,B,B,B,B,B,B,
  B,B,B,B,B,B,B,K,W,W,W,K,B,B,B,B,B,B,B,B,B,B,B,B,
  B,B,B,B,B,B,K,W,W,W,W,W,W,K,B,B,B,B,B,B,B,B,B,B,
  B,B,B,B,K,K,W,W,W,W,W,W,W,W,K,B,B,B,B,B,B,B,B,B,
  B,B,B,K,W,W,W,G,W,W,W,W,W,W,W,K,B,B,B,B,B,B,B,B,
  B,B,B,K,W,W,G,W,W,W,W,W,W,W,W,K,B,B,B,B,B,B,B,B,
  B,B,B,K,W,G,W,W,W,W,W,W,W,W,W,K,B,B,B,B,B,B,B,B,
  B,B,K,W,W,G,W,W,W,W,W,W,W,W,W,W,K,B,B,B,B,B,B,B,
  B,K,W,W,W,G,G,W,W,W,W,W,W,W,W,W,W,K,B,B,B,B,B,B,
  B,K,W,W,G,G,W,W,W,W,W,W,W,W,W,W,W,W,K,K,K,B,B,B,
  K,W,W,W,G,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,K,B,B,
  K,W,W,G,G,W,W,W,W,W,W,W,W,W,W,W,G,W,W,W,W,K,B,B,
  K,W,W,G,G,W,W,W,W,W,W,W,W,W,W,W,W,G,W,W,W,W,K,B,
  B,K,W,W,G,G,W,W,W,W,W,W,W,W,W,W,W,G,W,W,W,W,W,K,
  B,B,K,W,G,G,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,K };

uint8_t xoffset = 0;
uint8_t yoffset = 0;

void drawacloud() {
  uint16_t *buf = new uint16_t[sizeof(tile_a)];
  if (buf) {
    memcpy_P(buf, tile_a, sizeof(tile_a));
  }
  for (int x = 0; x < 5; x++) {
    for (int y = 0; y < 8; y++) {
      tft.drawBuffer(x*24 + xoffset, y*16 + yoffset, 24, 16, buf);
      yield();
    }
  }
  free(buf);
  xoffset = (xoffset + 5) % 128;
  yoffset = (yoffset + 5) % 128;
}

void setup(void) {
  // Reuse the Adafruit initialization because it is good
  tft.initR(INITR_144GREENTAB);
}

void loop() {
  drawacloud();
  yield();
}
