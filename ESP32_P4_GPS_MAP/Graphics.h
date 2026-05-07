#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "Config.h"

extern uint8_t frameBuffer[RES_Y][42];

void setPixel(int x, int y, bool b);
void drawLine(int x0, int y0, int x1, int y1, bool thick = false);
void drawRailway(int x0, int y0, int x1, int y1);
void drawLargeChar(int x, int y, char c, bool inverted = false);
void drawLargeString(int x, int y, const char* s, bool inverted = false);

#endif