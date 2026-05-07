#ifndef MAP_ENGINE_H
#define MAP_ENGINE_H

#include "Config.h"
#include "Graphics.h"
#include <SD.h>
#include <FS.h>

extern uint8_t* tileBuffer;
extern size_t currentBufferSize;

void initMapEngine();
void renderMap(float cLat, float cLon, float mapZoom, float mapRotation);

#endif