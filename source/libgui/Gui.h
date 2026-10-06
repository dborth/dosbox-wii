/*!\mainpage libwiigui Documentation
 *
 * \section Introduction
 * libwiigui is a GUI library for the Wii, created to help structure the
 * design of a complicated GUI interface, and to enable an author to create
 * a sophisticated, feature-rich GUI. It was originally conceived and written
 * after I started to design a GUI for Snes9x GX, and found libwiisprite and
 * GRRLIB inadequate for the purpose. It uses GX for drawing, and makes use
 * of PNGU for displaying images and FreeTypeGX for text. It was designed to
 * be flexible and is easy to modify - don't be afraid to change the way it
 * works or expand it to suit your GUI's purposes! If you do, and you think
 * your changes might benefit others, please share them so they might be
 * added to the project!
 *
 * \section Quickstart
 * Start from the supplied template example. For more advanced uses, see the
 * source code for Snes9x GX, FCE Ultra GX, and Visual Boy Advance GX.

 * \section Contact
 * If you have any suggestions for the library or documentation, or want to
 * contribute, please visit the libwiigui website:
 * http://code.google.com/p/libwiigui/

 * \section Credits
 * This library was wholly designed and written by Tantric. Thanks to the
 * authors of PNGU and FreeTypeGX, of which this library makes use. Thanks
 * also to the authors of GRRLIB and libwiisprite for laying the foundations.
 *
*/

#ifndef LIBWIIGUI_H
#define LIBWIIGUI_H

#include <gccore.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <exception>
#include <wchar.h>
#include <math.h>
#include <asndlib.h>
#include <wiiuse/wpad.h>

#include "pngu.h"
#include "FreeTypeGX.h"
#include "wiivideo.h"
#include "filelist.h"
#include "input.h"
#include "oggplayer.h"

extern FreeTypeGX *fontSystem[];

#define SCROLL_DELAY_INITIAL	200000
#define SCROLL_DELAY_LOOP		30000
#define SCROLL_DELAY_DECREASE	300
#define FILE_PAGESIZE 			8
#define PAGESIZE 				8
#define MAX_OPTIONS 			150
#define MAX_KEYBOARD_DISPLAY	32

typedef void (*UpdateCallback)(void * e);

enum
{
	ALIGN_LEFT,
	ALIGN_RIGHT,
	ALIGN_CENTRE,
	ALIGN_TOP,
	ALIGN_BOTTOM,
	ALIGN_MIDDLE
};

enum
{
	STATE_DEFAULT,
	STATE_SELECTED,
	STATE_CLICKED,
	STATE_HELD,
	STATE_DISABLED
};

enum
{
	SOUND_PCM,
	SOUND_OGG
};

enum
{
	IMAGE_TEXTURE,
	IMAGE_COLOR,
	IMAGE_DATA
};

enum
{
	TRIGGER_SIMPLE,
	TRIGGER_HELD,
	TRIGGER_BUTTON_ONLY,
	TRIGGER_BUTTON_ONLY_IN_FOCUS
};

enum
{
	SCROLL_NONE,
	SCROLL_HORIZONTAL
};

typedef struct _paddata {
	u16 btns_d;
	u16 btns_u;
	u16 btns_h;
	s8 stickX;
	s8 stickY;
	s8 substickX;
	s8 substickY;
	u8 triggerL;
	u8 triggerR;
} PADData;

#define EFFECT_SLIDE_TOP			1
#define EFFECT_SLIDE_BOTTOM			2
#define EFFECT_SLIDE_RIGHT			4
#define EFFECT_SLIDE_LEFT			8
#define EFFECT_SLIDE_IN				16
#define EFFECT_SLIDE_OUT			32
#define EFFECT_FADE					64
#define EFFECT_SCALE				128
#define EFFECT_COLOR_TRANSITION		256

#include "GuiTrigger.h"
#include "GuiSound.h"
#include "GuiElement.h"
#include "GuiWindow.h"
#include "GuiImageData.h"
#include "GuiImage.h"
#include "GuiMonoText.h"
#include "GuiText.h"
#include "GuiTooltip.h"
#include "GuiButton.h"
#include "GuiKeyboard.h"

#endif
