#include <Arduino.h>
#include "SD.h"
#include <math.h>
#include "Config.h"
#include "Graphics.h"

// Struct to track our 12MB buffers
struct TileSlot {
    uint8_t* buffer = nullptr;
    size_t size = 0;
    int latID = -999;
    int lonID = -999;
};

TileSlot tilePool[MAX_TILES];

extern void drawLine(int x0, int y0, int x1, int y1, bool thick);
extern void drawRailway(int x0, int y0, int x1, int y1);

void initMapEngine() {
    for (int i = 0; i < MAX_TILES; i++) {
        if (tilePool[i].buffer == nullptr) {
            tilePool[i].buffer = (uint8_t*)ps_malloc(TILE_BUFFER_SIZE);
            if (tilePool[i].buffer == nullptr) {
                Serial.printf("❌ Tile Slot %d: PSRAM Allocation FAILED!\n", i);
            } else {
                Serial.printf("✅ Tile Slot %d: 12MB Allocated\n", i);
            }
        }
    }
}

// Helper to load a specific tile into a specific slot
void loadTile(int slot, int latID, int lonID) {
    char fName[64];
    sprintf(fName, "/Map/bin_tiles/%d/tile_%d.bin", latID, lonID);
    
    if (SD.exists(fName)) {
        File rf = SD.open(fName);
        if (rf) {
            tilePool[slot].size = rf.read(tilePool[slot].buffer, TILE_BUFFER_SIZE);
            tilePool[slot].latID = latID;
            tilePool[slot].lonID = lonID;
            rf.close();
            Serial.printf("🗺️ Slot %d Loaded: %s (%d bytes)\n", slot, fName, tilePool[slot].size);
        }
    } else {
        tilePool[slot].size = 0;
        tilePool[slot].latID = latID;
        tilePool[slot].lonID = lonID;
    }
}

void renderMap(float cLat, float cLon, float mapZoom, float mapRotation) {
    // 1. Identify Target Tiles (Current + nearest neighbor)
    int tLat = (int)floor(cLat / TILE_SIZE);
    int tLon = (int)floor(cLon / TILE_SIZE);

    // Basic logic: Slot 0 is current, Slot 1 is neighbor (here simplified to current logic check)
    if (tLat != tilePool[0].latID || tLon != tilePool[0].lonID) {
        loadTile(0, tLat, tLon);
    }

    // 2. Rendering Preparation (Pristine)
    float rad = mapRotation * 0.01745329f;
    auto project = [&](float lat, float lon, int &x, int &y) {
        double dx = (double)(lon - cLon) * mapZoom;
        double dy = (double)(lat - cLat) * mapZoom;
        x = 168 + (int)(dx * cos(rad) - dy * sin(rad));
        y = 268 - (int)(dx * sin(rad) + dy * cos(rad));
    };

    // 3. Multi-Buffer Loop
    for (int b = 0; b < MAX_TILES; b++) {
        if (tilePool[b].size == 0) continue;

        size_t offset = 0;
        uint8_t* buf = tilePool[b].buffer;

        // 4. Binary Parsing Loop (YOUR PRISTINE LOGIC)
        while (offset + 22 <= tilePool[b].size) {
            float type, minLat, maxLat, minLon, maxLon;
            uint16_t numPoints;

            memcpy(&type, buf + offset, 4);
            memcpy(&minLat, buf + offset + 4, 4);
            memcpy(&maxLat, buf + offset + 8, 4);
            memcpy(&minLon, buf + offset + 12, 4);
            memcpy(&maxLon, buf + offset + 16, 4);
            memcpy(&numPoints, buf + offset + 20, 2);
            offset += 24;

            // Bounding Box Clipping
            float viewMargin = 0.05f; 
            if (minLat > cLat + viewMargin || maxLat < cLat - viewMargin || 
                minLon > cLon + viewMargin || maxLon < cLon - viewMargin) {
                offset += 8 + ((numPoints - 1) * 4);
                continue; 
            }

            // LOD Check
            if (type == 5.0f && mapZoom < 180000.0f) {
                offset += 8 + ((numPoints - 1) * 4);
                continue; 
            }

            float pLat, pLon;
            memcpy(&pLat, buf + offset, 4);
            memcpy(&pLon, buf + offset + 4, 4);
            offset += 8;

            int xPrev, yPrev, xCur, yCur;
            project(pLat, pLon, xPrev, yPrev);

            for (int i = 1; i < numPoints; i++) {
                int16_t dLat, dLon;
                memcpy(&dLat, buf + offset, 2);
                memcpy(&dLon, buf + offset + 2, 2);
                offset += 4;

                float curLat = pLat + (dLat / 100000.0f);
                float curLon = pLon + (dLon / 100000.0f);
                project(curLat, curLon, xCur, yCur);
                
                if (xPrev > -50 && xPrev < 386 && yPrev > -50 && yPrev < 586) {
                    if (type == 1.0f) drawLine(xPrev, yPrev, xCur, yCur, true); 
                    else if (type == 2.0f) drawLine(xPrev, yPrev, xCur, yCur, false);
                    else if (type == 4.0f) drawRailway(xPrev, yPrev, xCur, yCur);
                    else drawLine(xPrev, yPrev, xCur, yCur, false);
                }
                xPrev = xCur; yPrev = yCur;
            }
        }
    }
}