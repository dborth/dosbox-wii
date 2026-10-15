/****************************************************************************
 * DOSBox Wii
 *
 * menustyle.h
 *
 * The look of the menus, drawn in code: colors, metrics and shape styles for
 * the header/footer bars, the page background, the buttons and (through
 * GuiTheme) the file browser and cards. Nothing here touches a platform API,
 * so it can be built on a host for previews. The GuiShape wrappers live in
 * menuart.h.
 *
 * Everything that gives the app its identity is in MenuPaletteDefault(): a
 * primary hue for the chrome, an accent for focus and selection, and a gray
 * secondary for surfaces. Sister emulators change that one function and keep
 * the rest.
 ***************************************************************************/
#pragma once
#include "libgui/GuiTheme.h"

// design-canvas sizes
#define MENU_BTN_LONG_W		216
#define MENU_BTN_LONG_H		52
#define MENU_BTN_LARGE_W	164
#define MENU_BTN_LARGE_H	92
#define MENU_BAR_TOP_H		84
#define MENU_BAR_BOTTOM_H	100
// shapes that touch the screen edge are drawn this much larger than the screen,
// so their outline and feathered edge are never visible
#define MENU_BAR_BLEED		6

#define MENU_BTN_LONG_RADIUS	26.0f	// capsule
#define MENU_BTN_LARGE_RADIUS	46.0f	// capsule (half the button height)
#define MENU_BTN_ICON_INSET		3.0f	// icons stay clear of the button's outline (up to its highlighted width)

#define MENU_BTN_NORMAL_RADIUS	26.0f
#define MENU_BTN_SHORT_RADIUS	26.0f
#define MENU_BTN_SMALL_RADIUS	20.0f

#define MENU_BTN_NORMAL_W	196
#define MENU_BTN_NORMAL_H	52
#define MENU_BTN_SHORT_W	160
#define MENU_BTN_SHORT_H	52
#define MENU_BTN_SMALL_W	104
#define MENU_BTN_SMALL_H	40

// dialogs: a card with a chrome title band
#define MENU_DIALOG_W			448
#define MENU_DIALOG_H			288
#define MENU_DIALOG_HEADER_H	52
#define MENU_CREDITS_W			580
#define MENU_CREDITS_H			448
#define MENU_CREDITS_HEADER_H	60
#define MENU_DIALOG_RADIUS		14.0f
// title text sits this far down on the band, so its glyphs look centered in it
#define MENU_DIALOG_TITLE_Y		18
#define MENU_CREDITS_TITLE_Y	18

// the on-screen keyboard: a card with a band across the top that holds the text field
#define MENU_KEYBOARD_W			580
#define MENU_KEYBOARD_H			400
#define MENU_KEYBOARD_HEADER_H	60

// the gray stand-in for the screen in the zoom and position windows
#define MENU_SCREEN_W			68
#define MENU_SCREEN_H			64

// page-turning arrows: hit area of the button, and the glyph drawn inside it
#define MENU_ARROW_HIT_LONG		32
#define MENU_ARROW_HIT_SHORT	28
#define MENU_ARROW_LONG			24
#define MENU_ARROW_SHORT		17

// progress bar and spinner inside the progress dialog
#define MENU_PROGRESS_W			398
#define MENU_PROGRESS_H			22
#define MENU_SPINNER_SIZE		72
#define MENU_SPINNER_DOTS		8

// the two cards of the game list page (the list itself is drawn by GuiFileBrowser)
#define MENU_LIST_X			20
#define MENU_LIST_Y			101
#define MENU_LIST_W			330
#define MENU_LIST_H			262
#define MENU_PREVIEW_X		364
#define MENU_PREVIEW_W		256
#define MENU_PREVIEW_H		MENU_LIST_H

static inline PixelColor MenuRGBA(int r, int g, int b, int a = 255)
{
	PixelColor c = { (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a };
	return c;
}

//!Blends a toward b (t = 0 gives a, t = 1 gives b); alpha is kept from a
static inline PixelColor MenuMix(PixelColor a, PixelColor b, float t)
{
	PixelColor c = a;
	c.r = (uint8_t)(a.r + (b.r - a.r) * t + 0.5f);
	c.g = (uint8_t)(a.g + (b.g - a.g) * t + 0.5f);
	c.b = (uint8_t)(a.b + (b.b - a.b) * t + 0.5f);
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

//!Makes the shared widgets (GuiFileBrowser, cards) follow the palette. Call once at
//!startup, before any page is built.
static inline void MenuApplyTheme()
{
	const MenuPalette& p = MenuPaletteCurrent();
	GuiTheme& t = GuiTheme::current();
	t.background = p.backgroundBottom;
	t.surface = p.surface;
	t.surfaceRaised = p.surfaceRaised;
	t.outline = p.outline;
	t.outlineStrong = p.outlineStrong;
	t.accent = p.accent;
	t.accentLight = p.accentTint;
	t.accentDark = p.accentDark;
	t.text = p.text;
	t.textMuted = p.textMuted;
	t.textOnAccent = p.textOnAccent;
	t.highlight = p.accentTint;
	t.mark = p.mark;
	t.shadow = MenuRGBA(0, 0, 0, 64);
	t.radius = 14.0f;
	t.strokeWidth = 1.5f;
	t.shadowBlur = 14.0f;
}

//!Resting, highlighted and pressed looks shared by the long and large buttons.
//!Highlight: warmer face, accent outline and a pale glow that reads on the chrome
//!as well as on the page.
static inline GuiShapeStyle MenuButtonStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();

	GuiPaint rest = GuiPaint()
		.gradient(p.surfaceRaised, MenuMix(p.surface, p.outline, 0.45f))
		.stroke(2.0f, p.chromeDark)
		.shadow(8.0f, MenuRGBA(0, 0, 0, 100), 0.0f, 2.0f);

	GuiPaint lit = GuiPaint()
		.gradient(p.surfaceRaised, MenuMix(p.surfaceRaised, p.accentTint, 0.9f))
		.stroke(3.0f, p.accent)
		.glow(14.0f, p.glow, 1.0f);

	GuiPaint pressed = GuiPaint()
		.gradient(MenuMix(p.accentTint, p.outlineStrong, 0.25f), MenuMix(p.accentTint, p.outlineStrong, 0.5f))
		.stroke(2.0f, p.chromeDark)
		.shadow(4.0f, MenuRGBA(0, 0, 0, 80), 0.0f, 1.0f);

	return GuiShapeStyle(rest).onFocus(lit).onPress(pressed);
}

//!The header bar: light at the top darkening downward, a fine light edge and a soft
//!shadow onto the page below.
static inline GuiPaint MenuBarTopPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint()
		.gradient(p.chromeLight, p.chromeDark)
		.stroke(1.5f, p.chromeEdge)
		.shadow(18.0f, MenuRGBA(0, 0, 0, 150), 0.0f, 4.0f);
}

//!The footer bar: same chrome, shadow cast upward.
static inline GuiPaint MenuBarBottomPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint()
		.gradient(MenuMix(p.chromeLight, p.chromeDark, 0.15f), MenuMix(p.chromeDark, p.chromeLight, 0.05f))
		.stroke(1.5f, p.chromeEdge)
		.shadow(18.0f, MenuRGBA(0, 0, 0, 150), 0.0f, -4.0f);
}

//!The page behind everything: a quiet vertical gradient. (It replaces the striped
//!flat color; stripes beat against the bars' shadows.)
static inline GuiPaint MenuBackgroundPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint().gradient(p.backgroundTop, p.backgroundBottom);
}

//!The body of a dialog or the credits window: a light card with a firm chrome outline
//!and a deep shadow that lifts it off the page.
static inline GuiPaint MenuDialogBodyPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint()
		.gradient(p.surfaceRaised, p.surface)
		.stroke(2.0f, p.chromeDark)
		.shadow(26.0f, MenuRGBA(0, 0, 0, 150), 0.0f, 8.0f);
}

//!The title band across the top of a dialog (the title text on it is white)
static inline GuiPaint MenuDialogHeaderPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint()
		.gradient(p.chromeLight, p.chromeDark)
		.stroke(2.0f, p.chromeDark);
}

//!The empty part of a progress bar
static inline GuiPaint MenuProgressTrackPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint().color(p.outline).stroke(1.5f, p.outlineStrong);
}

//!The filled part of a progress bar
static inline GuiPaint MenuProgressFillPaint()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiPaint().gradient(MenuMix(p.accent, MenuRGBA(255, 255, 255), 0.3f), p.accent);
}

//!One dot of the busy spinner. level 0 is the head (bold, accent colored); higher
//!levels trail off smaller and paler.
static inline GuiPaint MenuSpinnerDotPaint(int level)
{
	const MenuPalette& p = MenuPaletteCurrent();
	const float t = (float)level / (float)(MENU_SPINNER_DOTS - 1);
	return GuiPaint().color(MenuMix(p.accent, p.outline, t));
}

//!Diameter of the spinner dot at a trail level
static inline float MenuSpinnerDotSize(int level)
{
	return 14.0f - 6.0f * (float)level / (float)(MENU_SPINNER_DOTS - 1);
}

//!Resting and lit colors of the page-turning arrows (gray, turning to the accent)
static inline PixelColor MenuArrowRest() { return MenuPaletteCurrent().outlineStrong; }
static inline PixelColor MenuArrowLit() { return MenuPaletteCurrent().accent; }

//!The one button look for the controller screens (and the bars): white and outlined at rest like the home
//!screen's, a dark face with a white outline and a glow when highlighted (the label turns white with it).
//!`marked` outlines a resting button in the accent color: the choice a control has now.
static inline GuiShapeStyle MenuBarButtonStyle(bool marked = false)
{
	const MenuPalette& p = MenuPaletteCurrent();

	GuiPaint rest = GuiPaint()
		.gradient(p.surfaceRaised, MenuMix(p.surface, p.outline, 0.45f))
		.stroke(marked ? 3.5f : 2.0f, marked ? p.accent : p.chromeDark)
		.shadow(6.0f, MenuRGBA(0, 0, 0, 110), 0.0f, 2.0f);

	GuiPaint lit = GuiPaint()
		.gradient(p.accent, p.accentDark)
		.stroke(2.5f, p.surfaceRaised)
		.glow(12.0f, p.glow, 1.0f);

	GuiPaint pressed = GuiPaint()
		.gradient(p.accentDark, p.accentDark)
		.stroke(2.0f, p.outlineStrong);

	return GuiShapeStyle(rest).onFocus(lit).onPress(pressed);
}

//!A tab that is showing: a pressed-in gray face, so it reads as "this page" and not as the highlight
static inline GuiShapeStyle MenuTabActiveStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	GuiShapeStyle s = MenuBarButtonStyle();
	s.normal = GuiPaint()
		.gradient(MenuMix(p.outlineStrong, p.chromeDark, 0.15f), p.outlineStrong)
		.stroke(2.0f, p.chromeDark);
	return s;
}

//!The one card behind a page's content: opaque, so the game picture never shows through the controls
static inline GuiShapeStyle MenuPageCardStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiShapeStyle(GuiPaint()
		.gradient(p.surfaceRaised, p.surface)
		.stroke(1.5f, p.outline)
		.shadow(14.0f, MenuRGBA(0, 0, 0, 140), 0.0f, 4.0f));
}

//!A flat tinted area that groups things on a card (a player, a section of buttons)
static inline GuiShapeStyle MenuSectionStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	return GuiShapeStyle(GuiPaint().color(MenuMix(p.surface, p.outline, 0.5f)));
}

//!A light tile on a section that can be selected (outline when highlighted)
static inline GuiShapeStyle MenuTileStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	GuiPaint rest = GuiPaint().color(p.surfaceRaised).stroke(1.5f, p.outline);
	GuiPaint lit = GuiPaint().color(p.surfaceRaised).stroke(3.0f, p.accent).glow(8.0f, p.glow, 0.6f);
	GuiPaint pressed = GuiPaint().color(MenuMix(p.accentTint, p.surfaceRaised, 0.5f)).stroke(3.0f, p.accent);
	return GuiShapeStyle(rest).onFocus(lit).onPress(pressed);
}

//!A selectable row on a section: no look at rest, an accent outline over a light fill when highlighted
static inline GuiShapeStyle MenuRowHighlightStyle()
{
	const MenuPalette& p = MenuPaletteCurrent();
	GuiPaint lit = GuiPaint().color(p.surfaceRaised).stroke(2.5f, p.mark);
	GuiPaint pressed = GuiPaint().color(MenuMix(p.mark, p.surfaceRaised, 0.85f)).stroke(2.5f, p.mark);
	return GuiShapeStyle(GuiPaint()).onFocus(lit).onPress(pressed);
}

//!Text on the chrome bars (the page title and the hint line)
static inline PixelColor MenuChromeText() { return MenuRGBA(255, 255, 255); }
static inline PixelColor MenuChromeTextMuted() { return MenuRGBA(206, 206, 206); }
