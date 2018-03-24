#define FS_NO_GLOBALS
#include <FS.h>
// JPEG decoder library
#include <JPEGDecoder.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include "fast_st3375.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266FtpServer.h>
#include <ArduinoOTA.h>

// GPIO 0, 2 or 15 control the boot-mode of the ESP, maybe scary to use those
// #define sclk D5 // GPIO14 (SPI SCK)
// #define mosi D7 // GPIO13 (SPI MOSI)
// #define cs   D8 // GPIO15 (SPI SS)
#define cs   16 // GPIO16
#define dc   5 // GPIO4
#define rst  4 // GPIO5

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
      fs::File f = SPIFFS.open("/out.jpg", "w");
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

//====================================================================================
//   Decode and paint onto the TFT screen
//====================================================================================
void jpegRender(int xpos, int ypos) {

  // retrieve infomration about the image
  uint16_t  *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = min(mcu_w, max_x % mcu_w);
  uint32_t min_h = min(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // read each MCU block until there are no more
  while ( JpegDec.read()) {

    // save a pointer to the image block
    pImg = JpegDec.pImage;

    // calculate where the image block should be drawn on the screen
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right and bottom edges
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // calculate how many pixels must be drawn
    uint32_t mcu_pixels = win_w * win_h;

    // draw image MCU block only if it will fit on the screen
    if ( ( mcu_x + win_w) <= tft.width() && ( mcu_y + win_h) <= tft.height()) {
      // Now set a MCU bounding window on the TFT to push pixels into (x, y, x + width - 1, y + height - 1)
      tft.setAddrWindow(mcu_x, mcu_y, mcu_x + win_w - 1, mcu_y + win_h - 1);

      // Write all MCU pixels to the TFT window
      // while (mcu_pixels--) tft.pushColor(*pImg++); // Send MCU buffer to TFT 16 bits at a time
      tft.swapendian((uint8_t *)pImg, mcu_pixels*2);
      tft.writedata((uint8_t *)pImg, mcu_pixels*2);
    }

    // Stop drawing blocks if the bottom of the screen has been reached,
    // the abort function will close the file
    else if ( ( mcu_y + win_h) >= tft.height()) JpegDec.abort();

  }

    // calculate how long it took to draw the image
  drawTime = millis() - drawTime;

  // print the results to the serial port
  Serial.print  ("Total render time was    : "); Serial.print(drawTime); Serial.println(" ms");
  Serial.println("=====================================");
}

void drawFSJpeg(const char *filename, int xpos, int ypos) {

  Serial.println("=====================================");
  Serial.print("Drawing file: "); Serial.println(filename);
  Serial.println("=====================================");

  // Open the file (the Jpeg decoder library will close it)
  fs::File jpgFile = SPIFFS.open( filename, "r");    // File handle reference for SPIFFS
  //  File jpgFile = SD.open( filename, FILE_READ);  // or, file handle reference for SD library

  if ( !jpgFile ) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println ("\" not found!");
    return;
  }

  // To initialise the decoder and provide the file, we can use one of the three following methods:
  //boolean decoded = JpegDec.decodeFsFile(jpgFile); // We can pass the SPIFFS file handle to the decoder,
  //boolean decoded = JpegDec.decodeSdFile(jpgFile); // or we can pass the SD file handle to the decoder,
  boolean decoded = JpegDec.decodeFsFile(filename);  // or we can pass the filename (leading / distinguishes SPIFFS files)
                                                     // The filename can be a String or character array
  if (decoded) {
    // print information about the image to the serial port
    // jpegInfo();

    // render the image onto the screen at given coordinates
    jpegRender(xpos, ypos);
  } else {
    Serial.println("Jpeg file format not supported!");
  }
}

void setup(void) {
  Serial.begin(115200);
  // Intitialize the LCD
  tft.initR(INITR_144GREENTAB);
  SPIFFS.begin();
  // Next lines have to be done ONLY ONCE!!!!!When SPIFFS is formatted ONCE you can comment these lines out!!
  // Serial.println("Please wait 30 secs for SPIFFS to be formatted");
//  SPIFFS.format();
  // Serial.println("Spiffs formatted");
  ftpSrv.begin("esp8266","esp8266");

   ArduinoOTA.setHostname("esp");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
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

  // downloadFile(downloadURL.getValue());
  // downloadFile("http://www.gravatar.com/avatar/b26354198c6e8a01b014a81810bafe36?s=128");
  // downloadFile("http://www.gravatar.com/avatar/71984c1e86b3a6dce833f047d4d000cc?s=128");
  // drawBmp();

  while(true) {
    // drawacloud();
    yield();
    ftpSrv.handleFTP();
    ArduinoOTA.handle();
    for (int i = 0; i <= 27; i++) {
      yield();
      ftpSrv .handleFTP();
      ArduinoOTA.handle();
      char filename[10];
      sprintf(filename, "/%d.jpg", i);
      drawFSJpeg(filename, 0, 0);
    }
  }
}
