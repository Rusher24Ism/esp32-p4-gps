#include <TinyGPSPlus.h>
#include <Wire.h>
#include "FS.h"
#include "SD.h"
#include <SPI.h>

// --- PIN DEFINITIONS ---
#define SD_SCK    43
#define SD_MISO   39
#define SD_MOSI   44
#define SD_CS     42
#define LCD_SCS   22
#define LCD_DISP  20
#define LCD_EXTCOMIN 21
#define RES_X 336
#define RES_Y 536

// --- MAP SETTINGS ---
#define MAP_ZOOM 250000.0 
#define TILE_SIZE 0.25

// --- MENU STATES ---
enum ScreenMode { MAP_VIEW, MENU_MAIN, CLOCK_VIEW, COMPASS_VIEW };
ScreenMode currentMode = MAP_VIEW; 

SPIClass sdSPI(HSPI); 
TinyGPSPlus gps;

const char* daysOfWeek[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* months[] = {"---", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// Fixed Font Array
const uint8_t font57[] = {
  0x3e,0x51,0x49,0x45,0x3e,0x00,0x42,0x7f,0x40,0x00,0x42,0x61,0x51,0x49,0x46,0x21,0x41,0x45,0x4b,0x31,
  0x18,0x14,0x12,0x7f,0x10,0x27,0x45,0x45,0x45,0x39,0x3c,0x4a,0x49,0x49,0x30,0x01,0x71,0x09,0x05,0x03,
  0x36,0x49,0x49,0x49,0x36,0x06,0x49,0x49,0x29,0x1e,0x7c,0x12,0x11,0x12,0x7c,0x7f,0x49,0x49,0x49,0x36,
  0x3e,0x41,0x41,0x41,0x22,0x7f,0x41,0x41,0x22,0x1c,0x7f,0x49,0x49,0x49,0x41,0x7f,0x09,0x09,0x09,0x01,
  0x3e,0x41,0x49,0x49,0x7a,0x7f,0x08,0x08,0x08,0x7f,0x00,0x41,0x7f,0x41,0x00,0x20,0x40,0x41,0x3f,0x01,
  0x7f,0x08,0x14,0x22,0x41,0x7f,0x40,0x40,0x40,0x40,0x7f,0x02,0x0c,0x02,0x7f,0x7f,0x04,0x08,0x10,0x7f,
  0x3e,0x41,0x41,0x41,0x3e,0x7f,0x09,0x09,0x09,0x06,0x3e,0x41,0x51,0x21,0x5e,0x7f,0x09,0x19,0x29,0x46,
  0x46,0x49,0x49,0x49,0x31,0x01,0x01,0x7f,0x01,0x01,0x3f,0x40,0x40,0x40,0x3f,0x1f,0x20,0x40,0x20,0x1f,
  0x3f,0x40,0x38,0x40,0x3f,0x63,0x14,0x08,0x14,0x63,0x07,0x08,0x70,0x08,0x07,0x61,0x51,0x49,0x45,0x43,
  0x00,0x00,0x24,0x00,0x00, 0x00,0x00,0x60,0x60,0x00, 0x40,0x40,0x40,0x40,0x40
};

struct Point { float lat; float lon; };
uint8_t frameBuffer[536][42];
unsigned long lastToggle = 0;
float currentHeading = 0;
float magMinX = 32767, magMaxX = -32768, magMinY = 32767, magMaxY = -32768;

// --- GRAPHICS CORE ---
void setPixel(int x, int y, bool b) {
  if (x < 0 || x >= RES_X || y < 0 || y >= RES_Y) return;
  if (b) frameBuffer[y][x >> 3] &= ~(0x01 << (x & 0x07));
  else   frameBuffer[y][x >> 3] |= (0x01 << (x & 0x07));
}

void drawLargeChar(int x, int y, char c) {
  int idx = -1;
  if (c >= '0' && c <= '9') idx = (c - '0') * 5;
  else if (c >= 'A' && c <= 'Z') idx = (c - 'A' + 10) * 5;
  else if (c >= 'a' && c <= 'z') idx = (c - 'a' + 10) * 5;
  else if (c == ':') idx = 36 * 5;
  else if (c == '.') idx = 37 * 5;
  else if (c == '-' || c == '_' || c == ' ') idx = 38 * 5;
  if (idx == -1) return;
  for (int i = 0; i < 5; i++) {
    uint8_t line = font57[idx + i];
    for (int j = 0; j < 7; j++) {
      if ((line >> j) & 0x01) {
        for(int dx=0; dx<2; dx++) for(int dy=0; dy<2; dy++) setPixel(x+(i*2)+dx, y+(j*2)+dy, true);
      }
    }
  }
}

void drawLargeString(int x, int y, const char* s) {
  while (*s) { drawLargeChar(x, y, *s++); x += 14; }
}

void drawLine(int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy, e2;
  while (true) { setPixel(x0, y0, true); if (x0 == x1 && y0 == y1) break;
    e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; } }
}

void drawCircle(int x0, int y0, int radius) {
  int x = radius, y = 0, err = 0;
  while (x >= y) {
    setPixel(x0 + x, y0 + y, true); setPixel(x0 + y, y0 + x, true);
    setPixel(x0 - y, y0 + x, true); setPixel(x0 - x, y0 + y, true);
    setPixel(x0 - x, y0 - y, true); setPixel(x0 - y, y0 - x, true);
    setPixel(x0 + y, y0 - x, true); setPixel(x0 + x, y0 - y, true);
    if (err <= 0) { y += 1; err += 2 * y + 1; }
    if (err > 0) { x -= 1; err -= 2 * x + 1; }
  }
}

// --- RENDERING LOGIC ---
void renderMap(float cLat, float cLon) {
  int tLat = (int)((floor(cLat / TILE_SIZE) * TILE_SIZE * 100) + 0.5);
  int tLon = (int)((floor(cLon / TILE_SIZE) * TILE_SIZE * 100) + 0.5);
  char fName[64]; sprintf(fName, "/Map/tiles/tile_%d_%d.bin", tLat, tLon);
  
  File rf = SD.open(fName);
  if (!rf) {
    drawLargeString(60, 250, "TILE NOT FOUND");
    return;
  }

  Point pPrev; 
  bool firstPointOfLine = true;

  while (rf.available() >= sizeof(Point)) {
    Point pCurr; 
    rf.read((uint8_t*)&pCurr, sizeof(Point));

    // Pens-Up Detection: If latitude is our 999.0 marker, lift the pen
    if (pCurr.lat > 90.0) {
      firstPointOfLine = true; 
      continue;
    }

    int x1 = 168 + (pCurr.lon - cLon) * MAP_ZOOM;
    int y1 = 268 - (pCurr.lat - cLat) * MAP_ZOOM;

    if (!firstPointOfLine) {
      int x0 = 168 + (pPrev.lon - cLon) * MAP_ZOOM;
      int y0 = 268 - (pPrev.lat - cLat) * MAP_ZOOM;
      
      // Clipping: Draw only if the line segment is roughly on-screen
      if ((x0 > -20 && x0 < RES_X + 20) && (y0 > -20 && y0 < RES_Y + 20)) {
        drawLine(x0, y0, x1, y1);
      }
    }
    
    pPrev = pCurr; 
    firstPointOfLine = false;
  }
  rf.close();
}

void setup() {
  pinMode(LCD_DISP, OUTPUT); digitalWrite(LCD_DISP, HIGH); 
  pinMode(LCD_SCS, OUTPUT); pinMode(LCD_EXTCOMIN, OUTPUT); 
  Serial1.begin(115200, SERIAL_8N1, 24, 25);
  Wire.begin(7, 8);
  SPI.begin(46, -1, 48, LCD_SCS); 
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  SD.begin(SD_CS, sdSPI, 20000000); 
  Wire.beginTransmission(0x0D); Wire.write(0x09); Wire.write(0x1D); Wire.endTransmission();
}

void loop() {
  // LCD Refresh pulses
  if (millis() - lastToggle > 500) { digitalWrite(LCD_EXTCOMIN, !digitalRead(LCD_EXTCOMIN)); lastToggle = millis(); }
  
  // GPS Update
  while (Serial1.available() > 0) gps.encode(Serial1.read());
  
  // Compass Update
  Wire.beginTransmission(0x0D); Wire.write(0x00); Wire.endTransmission();
  Wire.requestFrom(0x0D, 6);
  if (Wire.available() == 6) {
    int16_t x = (int16_t)(Wire.read() | (Wire.read() << 8));
    int16_t y = (int16_t)(Wire.read() | (Wire.read() << 8));
    Wire.read(); Wire.read();
    if (x < magMinX) magMinX = x; if (x > magMaxX) magMaxX = x;
    if (y < magMinY) magMinY = y; if (y > magMaxY) magMaxY = y;
    currentHeading = atan2(y - (magMaxY+magMinY)/2.0, x - (magMaxX+magMinX)/2.0) * 180.0 / PI;
    if (currentHeading < 0) currentHeading += 360;
  }

  // Clear Buffer (White background)
  memset(frameBuffer, 0xFF, sizeof(frameBuffer));

  // --- SCREEN MODES ---
  if (currentMode == MAP_VIEW) {
    // SATS Count (Line below removed as requested)
    char sBuf[16]; sprintf(sBuf, "SATS:%02d", (int)gps.satellites.value());
    drawLargeString(10, 10, sBuf);

    // Error Handling: SD Presence
    if (!SD.cardSize()) {
      drawLargeString(80, 250, "SD CARD FAIL");
    } 
    else if (gps.location.isValid()) {
      renderMap(gps.location.lat(), gps.location.lng());
      // Crosshair
      drawLine(160, 268, 176, 268); drawLine(168, 260, 168, 276); 
    } 
    else {
      drawLargeString(80, 250, "SEARCHING GPS...");
    }
  }

  else if (currentMode == CLOCK_VIEW) {
    if (gps.time.isValid()) {
      int hr = gps.time.hour() + 6; if (hr >= 24) hr -= 24;
      char tBuf[16]; sprintf(tBuf, "%02d:%02d", (hr % 12 == 0 ? 12 : hr % 12), gps.time.minute());
      drawLargeString(100, 150, tBuf);
      drawLargeString(100, 180, daysOfWeek[(gps.date.year() + gps.date.year()/4 + 1) % 7]);
      char dBuf[16]; sprintf(dBuf, "%02d %s %02d", gps.date.day(), months[gps.date.month()], gps.date.year()%100);
      drawLargeString(100, 210, dBuf);
    } else {
      drawLargeString(80, 250, "NO TIME DATA");
    }
  }

  else if (currentMode == COMPASS_VIEW) {
    int cx = RES_X/2, cy = RES_Y/2, r = 100;
    drawCircle(cx, cy, r);
    float angle = (currentHeading - 90) * 0.017453;
    drawLine(cx, cy, cx + r * cos(angle), cy + r * sin(angle));
    drawLargeString(cx - 15, cy - r - 30, "NORTH");
    char hBuf[16]; sprintf(hBuf, "DEG:%d", (int)currentHeading);
    drawLargeString(cx - 40, cy + r + 20, hBuf);
  }

  else if (currentMode == MENU_MAIN) {
    drawLargeString(40, 50, "--- MAIN MENU ---");
    drawLargeString(40, 100, "1. MAP VIEW");
    drawLargeString(40, 140, "2. CLOCK & DATE");
    drawLargeString(40, 180, "3. COMPASS");
  }

  // --- LCD HARDWARE REFRESH ---
  SPI.beginTransaction(SPISettings(2000000, LSBFIRST, SPI_MODE0));
  digitalWrite(LCD_SCS, HIGH); delayMicroseconds(5);
  for (uint16_t i = 1; i <= RES_Y; i++) {
    SPI.transfer((uint8_t)((i << 6) | 0x01)); SPI.transfer((uint8_t)(i >> 2));
    for (int j = 0; j < 42; j++) SPI.transfer(frameBuffer[i-1][j]);
  }
  digitalWrite(LCD_SCS, LOW); SPI.endTransaction();
}