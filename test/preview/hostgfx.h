// Host stand-ins for the platform: a software rasterizer behind the VideoDriver / ImageRenderer / GlyphRenderer
// interfaces, so the real menu code can draw into a PNG on a PC. Built by test/preview/Makefile; never part of the
// console build.
#pragma once
#include <string>
#include <vector>
#include <stdint.h>

// Creates the host platform (assigning the global `platform`) with a w x h design canvas drawn at `scale` pixels
// per design pixel. `assetScaleX/Y` mirror PlatformConfig (3 and 2.25 for the HD art).
void HostInit(int w, int h, float scale, float assetScaleX = 1.0f, float assetScaleY = 1.0f);

// Fills the frame with a color (the menu clears it every frame on the console)
void HostClear(uint8_t r, uint8_t g, uint8_t b);

// Writes the frame as a PNG. `wide` stretches a 4:3 frame to 16:9, as a widescreen TV shows the Wii's output.
// `guides` draws the 5% overscan margin and the 4:3 centre lines over it (for checking layouts, never for shipping).
bool HostSavePng(const char * path, bool wide, bool guides);

// The frame's pixels (RGBA8, top row first) and size
const std::vector<uint8_t>& HostPixels(int& w, int& h);

// Paints a stand-in for the paused game behind the menu: a DOS prompt in an 80x25 text screen
void HostPaintGame(int w, int h);
