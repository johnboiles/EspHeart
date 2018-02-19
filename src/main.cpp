#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include "fast_st3375.h"
#include <ESP8266HTTPClient.h>
#include <FS.h>
#include <ESP8266FtpServer.h>

// GPIO 0, 2 or 15 control the boot-mode of the ESP, maybe scary to use those
// #define sclk D5 // GPIO14 (SPI SCK)
// #define mosi D7 // GPIO13 (SPI MOSI)
// #define cs   D8 // GPIO15 (SPI SS)
#define cs   D0 // GPIO16
#define dc   D2 // GPIO4
#define rst  D1 // GPIO5

#define K 0x0000 // Black
#define W 0xFFFF // White
#define B 0x3F6C // Blue (sky)
#define G 0xBE04 // Gray (cloud shadow, alt blue really)

Fast_ST7735 tft = Fast_ST7735(cs, dc, rst);
FtpServer ftpSrv;

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
uint8_t increment = 5;

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
  xoffset = (xoffset + increment) % 128;
  yoffset = (yoffset + increment) % 128;
}

void downloadFile(const char *URL) {
  HTTPClient http;
  http.begin(URL);
  int httpCode = http.GET();

  Serial.printf("[HTTP] GET %s code: %d\n", URL, httpCode);

  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if(httpCode == HTTP_CODE_OK) {

      // open file for writing
      File f = SPIFFS.open("/out.png", "w");
      if (!f) {
          Serial.println("file open failed");
      }

      // get lenght of document (is -1 when Server sends no Content-Length header)
      int len = http.getSize();
      uint8_t buff[128] = { 0 };
      WiFiClient *stream = http.getStreamPtr();

      // read all data from server
      while(http.connected() && (len > 0 || len == -1)) {
        // get available data size
        size_t size = stream->available();
        if(size) {
          // read up to 128 byte
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          // write it to Serial
          Serial.write(buff, c);
          f.write(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          if(len > 0) {
            len -= c;
          }
        }
        yield();
      }
      f.close();

      Serial.println();
      Serial.print("[HTTP] connection closed or file end.\n");
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
  }
  http.end();
}

void setup(void) {
  Serial.begin(115200);
  // Intitialize the LCD
  tft.initR(INITR_144GREENTAB);
  SPIFFS.begin();
  // Next lines have to be done ONLY ONCE!!!!!When SPIFFS is formatted ONCE you can comment these lines out!!
  Serial.println("Please wait 30 secs for SPIFFS to be formatted");
  SPIFFS.format();
  Serial.println("Spiffs formatted");
  ftpSrv.begin("esp8266","esp8266");
}

void loop() {
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(0, 0);
  tft.setTextWrap(true);
  tft.setTextColor(ST7735_RED);
  tft.setTextSize(1);
  tft.println("Download your heart!\nOpen your WiFi config\nand look for the\nfollowing AP");
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(2);
  tft.println("HeartConf");
  tft.setTextColor(ST7735_RED);
  tft.setTextSize(1);
  tft.println("Connect and you will\nbe prompted for your\nWiFi settings and\nyour Periscope\nusername.");

  // WiFiManager wifiManager;
  // WiFiManagerParameter downloadURL("file", "file to download", "https://image.flaticon.com/icons/png/128/148/148836.png", 64);
  // wifiManager.addParameter(&downloadURL);
  // wifiManager.startConfigPortal("HeartConf");
  // Serial.printf("connected...yeey :), with increment %d", increment);

  WiFiClient wifiClient;
  WiFi.begin("Boiles", "stinaissohot");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(0, 50);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(2);
  tft.println("Connected!");


  delay(2000);
  // downloadFile(downloadURL.getValue());
  downloadFile("http://image.flaticon.com/icons/png/128/148/148836.png");

  while(true) {
    drawacloud();
    yield();
    ftpSrv.handleFTP();
  }
}
