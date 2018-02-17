#include <Adafruit_ST7735.h>
#include <SPI.h>

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

Adafruit_ST7735 tft = Adafruit_ST7735(cs, dc, rst);

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

void writecommand(uint8_t c) {
  digitalWrite(dc, LOW);
  digitalWrite(cs, LOW);
  SPI.transfer(c);
  digitalWrite(cs, HIGH);
}

void writedata(uint8_t *buffer, uint32_t size) {
  digitalWrite(dc, HIGH);
  digitalWrite(cs, LOW);
  SPI.writeBytes(buffer, size);
  digitalWrite(cs, HIGH);
}

void setAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
  writecommand(ST7735_CASET);
  const uint8_t caset[] = {0x00, x0+XSTART, 0x00, x1+XSTART};
  writedata((uint8_t *)caset, sizeof(caset));
  writecommand(ST7735_RASET); // Row addr set
  const uint8_t raset[] = {0x00, y0+YSTART, 0x00, y1+YSTART};
  writedata((uint8_t *)raset, sizeof(raset));
  writecommand(ST7735_RAMWR); // write to RAM
}

void drawBuffer(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t *buffer) {
  // rudimentary clipping (drawChar w/big text requires this)
  if((x >= WIDTH) || (y >= HEIGHT)) return;
  if((x + w - 1) >= WIDTH) w = WIDTH - x;
  if((y + h - 1) >= HEIGHT) h = HEIGHT - y;

  setAddrWindow(x, y, x+w-1, y+h-1);
  writedata((uint8_t *)buffer, w*h*2);
}

void drawacloud() {
  uint16_t *buf = new uint16_t[sizeof(tile_a)];
  if (buf) {
    memcpy_P(buf, tile_a, sizeof(tile_a));
  }
  for (int x = 0; x < 5; x++) {
    for (int y = 0; y < 8; y++) {
      drawBuffer(x*24 + xoffset, y*16 + yoffset, 24, 16, buf);
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
