/****************************************************************************
 * DOSBox Wii Home Menu
 * Tantric 2009
 *
 * wiivideo.h
 *
 * Video routines
 ***************************************************************************/

#ifndef _WIIVIDEOH_
#define _WIIVIDEOH_

#include <ogcsys.h>

void ResetVideo_Menu();
void TakeScreenshot();
void Menu_Render();
void Menu_DrawImg(f32 xpos, f32 ypos, u16 width, u16 height, u8 data[], f32 degrees, f32 scaleX, f32 scaleY, u8 alphaF );
void Menu_DrawRectangle(f32 x, f32 y, f32 width, f32 height, GXColor color, u8 filled);

extern int screenheight;
extern int screenwidth;
extern u8 * screenTex;
extern u32 FrameTimer;

#endif
