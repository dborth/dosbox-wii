/****************************************************************************
 * DOSBox Wii
 *
 * menupalette.h
 *
 * The colors that give the app its identity, and nothing else: a primary hue
 * for the chrome, an accent for focus and selection, and a gray secondary for
 * surfaces. Nothing here touches a platform API, so it can be built on a host.
 * theme.cpp turns the palette into the GuiTheme the shared libgui widgets read;
 * menustyle.h builds the menus' own shape styles from it. Sister emulators
 * change MenuPaletteDefault() and keep the rest.
 ***************************************************************************/
#pragma once
#include "libgui/GuiPaint.h"

static inline PixelColor MenuRGBA(int r, int g, int b, int a = 255)
{
	PixelColor c = { (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a };
	return c;
}

//!Everything that differs between the emulators' themes
struct MenuPalette {
	// chrome: the header and footer bars (light edge at the top of the gradient)
	PixelColor chromeLight, chromeDark, chromeEdge;
	// page background, a vertical gradient behind everything
	PixelColor backgroundTop, backgroundBottom;
	// highlight: focused outline, selected-row outline, scrollbar thumb when grabbed
	PixelColor accent, accentDark;
	PixelColor accentTint;		// selected-row fill
	PixelColor mark;			// the one hue in the UI: what is being edited (the control on the picture, its row, the line between)
	PixelColor glow;			// halo around a focused button; must read on the chrome
	// the gray secondary: cards, outlines, resting controls
	PixelColor surface, surfaceRaised, outline, outlineStrong;
	PixelColor text, textMuted, textOnAccent;
};

//!DOSBox Wii: monochrome chrome on a gray page
static inline MenuPalette MenuPaletteDefault()
{
	MenuPalette p;
	p.chromeLight    = MenuRGBA(76, 76, 76);
	p.chromeDark     = MenuRGBA(42, 42, 42);
	p.chromeEdge     = MenuRGBA(118, 118, 118);
	p.backgroundTop    = MenuRGBA(240, 240, 240);
	p.backgroundBottom = MenuRGBA(212, 212, 212);
	p.accent         = MenuRGBA(64, 64, 64);
	p.accentDark     = MenuRGBA(34, 34, 34);
	p.accentTint     = MenuRGBA(224, 224, 224);
	p.mark           = MenuRGBA(236, 128, 12);
	p.glow           = MenuRGBA(255, 255, 255, 170);
	p.surface        = MenuRGBA(246, 246, 246);
	p.surfaceRaised  = MenuRGBA(255, 255, 255);
	p.outline        = MenuRGBA(208, 208, 208);
	p.outlineStrong  = MenuRGBA(166, 166, 166);
	p.text           = MenuRGBA(30, 30, 30);
	p.textMuted      = MenuRGBA(118, 118, 118);
	p.textOnAccent   = MenuRGBA(255, 255, 255);
	return p;
}

//!The palette in use
static inline MenuPalette& MenuPaletteCurrent()
{
	static MenuPalette palette = MenuPaletteDefault();
	return palette;
}
