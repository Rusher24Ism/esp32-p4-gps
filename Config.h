#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- PIN DEFINITIONS --- (Unchanged)
#define SD_SCK     43
#define SD_MISO    39
#define SD_MOSI    44
#define SD_CS      42
#define LCD_SCS    22
#define LCD_DISP   20
#define LCD_EXTCOMIN 21

#define BTN_CENTER 47  
#define BTN_UP     2  
#define BTN_DOWN   3  

// --- DISPLAY SETTINGS ---
#define RES_X 336
#define RES_Y 536

// --- PSRAM ALLOCATION POOLS (Optimized to 28MB Total) ---
#define TILE_BUFFER_SIZE  (12 * 1024 * 1024) // 12MB per tile
#define MAX_TILES         2                  // 1 Active + 1 Preload (24MB total)
#define UI_ASSET_SIZE     (4 * 1024 * 1024)  // 4MB for high-res assets

// --- MAP LOGIC ---
#define TILE_SIZE 0.03125f

#endif