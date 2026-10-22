/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * menuicon.h
 *
 * The menu's line icons: small pictures drawn from strokes and shapes in a
 * 24 x 24 box, scaled to the size asked for and drawn in one colour (the pill
 * and tile icons of the home screen, the X of a popover). No image assets.
 ***************************************************************************/
#ifndef _MENUICON_H_
#define _MENUICON_H_

#include "menuart.h"

enum {
	MENU_ICON_NONE = -1,
	MENU_ICON_GAUGE, MENU_ICON_FRAMES, MENU_ICON_ASPECT, MENU_ICON_FILTER, MENU_ICON_KEYBOARD, MENU_ICON_GEAR,
	MENU_ICON_PULSE, MENU_ICON_DISK, MENU_ICON_POWER, MENU_ICON_INFO, MENU_ICON_PLAY, MENU_ICON_PAD, MENU_ICON_CLOSE, MENU_ICON_RESTART
};

//!One of the icons above, `size` pixels square, in `color`
class MenuIcon : public MenuMeshElement
{
	public:
		MenuIcon(int icon, int size, PixelColor c) : MenuMeshElement(size, size), kind(icon), color(c) {}

		//!Changes the colour (the icon is built again at the next draw if it differs)
		void setColor(PixelColor c)
		{
			if(c.r != color.r || c.g != color.g || c.b != color.b || c.a != color.a)
			{
				color = c;
				invalidate();
			}
		}

	private:
		void build() override;

		// strokes and shapes, in the 24 x 24 box
		GuiPoint P(float x, float y) const { return GuiPoint{ x * u, y * u }; }
		void line(float x1, float y1, float x2, float y2, float w = 2.0f);
		void box(float x, float y, float w, float h, float r, float lw = 2.0f);
		void ring(float cx, float cy, float r, float lw = 2.0f);
		void dot(float cx, float cy, float r);

		int kind;
		PixelColor color;
		float u;
		GuiFill fill;
};

#endif
