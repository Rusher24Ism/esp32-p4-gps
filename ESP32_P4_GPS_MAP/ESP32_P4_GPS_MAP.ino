#include <TinyGPSPlus.h>
#include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>

// --- OUR MODULAR HEADERS ---
#include "Config.h"
#include "Graphics.h"
#include "MapEngine.h"

// --- STATE MANAGEMENT ---
enum ScreenMode { MAP_VIEW, MENU_MAIN, CLOCK_VIEW, DIRECTION_VIEW };
ScreenMode currentMode = MAP_VIEW;

// --- GLOBAL OBJECTS ---
Preferences prefs;
WebServer server(80);
SPIClass sdSPI(HSPI);
TinyGPSPlus gps;

// --- NAVIGATION VARIABLES ---
float mapZoom;
float homeLat, homeLon;
double destLat = 0, destLon = 0;
float mapRotation = 0;
int menuIndex = 0;
bool wifiActive = false;
unsigned long lastToggle = 0;

// --- WEB SERVER HANDLERS ---
void handleRoot() {
  String html = "<html><body><h1>Navigator Setup</h1><form action='/set'>Lat: <input name='la'><br>Lon: <input name='lo'><br><input type='submit'></form></body></html>";
  server.send(200, "text/html", html);
}

void handleSet() {
  destLat = server.arg("la").toDouble();
  destLon = server.arg("lo").toDouble();
  wifiActive = false;
  WiFi.softAPdisconnect(true);
  currentMode = DIRECTION_VIEW;
  server.send(200, "text/plain", "Destination Set! Returning to device.");
}

// --- INPUT PROCESSING ---
void updateButtons() {
  static unsigned long lastPress = 0;
  static bool centerHeld = false;

  if (digitalRead(BTN_CENTER) == LOW) {
    if (!centerHeld) { lastPress = millis(); centerHeld = true; }
    if (millis() - lastPress > 1000) { // Long Press Toggle
      currentMode = (currentMode == CLOCK_VIEW) ? MAP_VIEW : CLOCK_VIEW;
      centerHeld = false; delay(500);
    }
  } else if (centerHeld) { // Short Press
    if (currentMode == DIRECTION_VIEW) currentMode = MAP_VIEW;
    else if (currentMode == MAP_VIEW) currentMode = MENU_MAIN;
    else if (currentMode == MENU_MAIN) {
      if (menuIndex == 1) { // Save Home
        homeLat = gps.location.lat(); homeLon = gps.location.lng(); 
        prefs.putFloat("hLat", homeLat); prefs.putFloat("hLon", homeLon); 
        currentMode = MAP_VIEW; 
      }
      else if (menuIndex == 2) { destLat = homeLat; destLon = homeLon; currentMode = DIRECTION_VIEW; }
      else if (menuIndex == 3) { // Toggle WiFi
        wifiActive = !wifiActive; 
        if(wifiActive) { WiFi.softAP("ESP32_P4_NAV"); server.begin(); } 
        else { WiFi.softAPdisconnect(true); } 
      }
      else if (menuIndex == 4) currentMode = MAP_VIEW;
    }
    centerHeld = false;
  }

  if (digitalRead(BTN_UP) == LOW) {
    if (currentMode == MENU_MAIN) menuIndex = max(0, menuIndex - 1);
    else { mapZoom += 10000; prefs.putFloat("zoom", mapZoom); }
    delay(150);
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    if (currentMode == MENU_MAIN) menuIndex = min(4, menuIndex + 1);
    else { mapZoom = max(10000.0f, mapZoom - 10000); prefs.putFloat("zoom", mapZoom); }
    delay(150);
  }
}

// --- CORE SETUP ---
void setup() {
  Serial.begin(115200);
  
  // Hardware Pins
  pinMode(LCD_DISP, OUTPUT); digitalWrite(LCD_DISP, HIGH); 
  pinMode(LCD_SCS, OUTPUT); pinMode(LCD_EXTCOMIN, OUTPUT);
  pinMode(BTN_CENTER, INPUT_PULLUP); pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP);

  // Initialize modular systems
  initMapEngine(); // Allocates 8MB PSRAM
  
  // Peripherals
  Serial1.begin(115200, SERIAL_8N1, 24, 25); // GPS
  Wire.begin(7, 8); // I2C
  SPI.begin(46, -1, 48, LCD_SCS); // Display SPI
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS); // SD SPI
  
  if (!SD.begin(SD_CS, sdSPI, 40000000)) {
    Serial.println("❌ SD Mount Failed!");
  }

  // Load Saved Preferences
  prefs.begin("nav", false);
  mapZoom = prefs.getFloat("zoom", 250000.0f);
  homeLat = prefs.getFloat("hLat", 0);
  homeLon = prefs.getFloat("hLon", 0);

  server.on("/", handleRoot); 
  server.on("/set", handleSet);
}

// --- MAIN LOOP ---
void loop() {
  // Toggle LCD polarity (Required for Sharp Memory LCDs)
  if (millis() - lastToggle > 500) { 
    digitalWrite(LCD_EXTCOMIN, !digitalRead(LCD_EXTCOMIN)); 
    lastToggle = millis(); 
  }

  // Process GPS Data
  while (Serial1.available() > 0) gps.encode(Serial1.read());
  
  // Background Tasks
  if (wifiActive) server.handleClient();
  updateButtons();

  // Clear Framebuffer (0xFF = White)
  memset(frameBuffer, 0xFF, sizeof(frameBuffer));

  // --- RENDERING BRANCHES ---
  if (currentMode == MAP_VIEW || currentMode == DIRECTION_VIEW) {
    char satStr[12]; sprintf(satStr, "SATS:%02d", (int)gps.satellites.value());
    drawLargeString(5, 5, satStr);

    if (gps.location.isValid()) {
      // Call modular map renderer
      renderMap(gps.location.lat(), gps.location.lng(), mapZoom, mapRotation);
      
      // Draw User Crosshair
      drawLine((RES_X/2)-8, (RES_Y/2), (RES_X/2)+8, (RES_Y/2)); 
      drawLine((RES_X/2), (RES_Y/2)-8, (RES_X/2), (RES_Y/2)+8);
      
      if (currentMode == DIRECTION_VIEW) {
        double dist = gps.distanceBetween(gps.location.lat(), gps.location.lng(), destLat, destLon);
        char dBuf[20]; 
        if (dist > 1000) sprintf(dBuf, "%.1fkm", dist/1000.0);
        else sprintf(dBuf, "%dm", (int)dist);
        drawLargeString(100, 500, dBuf);
      }
    } else {
      drawLargeString(80, 250, "WAITING FOR GPS");
    }
  } 
  else if (currentMode == MENU_MAIN) {
    const char* items[] = {"ZOOM ADJ", "SAVE HOME", "GO HOME", "WIFI NAV", "BACK"};
    for(int i=0; i<5; i++) drawLargeString(40, 100 + (i*50), items[i], (menuIndex == i));
  }
  else if (currentMode == CLOCK_VIEW && gps.time.isValid()) {
    char tBuf[16]; sprintf(tBuf, "%02d:%02d", (gps.time.hour()+6)%24, gps.time.minute());
    drawLargeString(120, 250, tBuf);
  }

  // --- LCD REFRESH (SPI Burst) ---
  SPI.beginTransaction(SPISettings(2000000, LSBFIRST, SPI_MODE0));
  digitalWrite(LCD_SCS, HIGH); delayMicroseconds(5);
  for (uint16_t i = 1; i <= RES_Y; i++) {
    SPI.transfer((uint8_t)((i << 6) | 0x01)); 
    SPI.transfer((uint8_t)(i >> 2));
    for (int j = 0; j < 42; j++) SPI.transfer(frameBuffer[i-1][j]);
  }
  digitalWrite(LCD_SCS, LOW); 
  SPI.endTransaction();
}