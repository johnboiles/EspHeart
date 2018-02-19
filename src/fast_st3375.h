#include <Adafruit_ST7735.h>

class Fast_ST7735 : public Adafruit_ST7735 {
public:
  Fast_ST7735(int8_t CS, int8_t RS, int8_t RST = -1);
  void swapendian(uint8_t *buffer, uint32_t size);
  void writedata(uint8_t *buffer, uint32_t size);
  void setAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
  void drawBuffer(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t *buffer);
};
