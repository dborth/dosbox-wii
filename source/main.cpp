/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * main.cpp
 *
 * Program flow: bring the app up, run DOSBox, leave. Also the hand-off
 * between emulation and the home menu.
 *
 * NOTE: DOSBox headers must not be included here; they collide with the
 * platform and libgui headers. RunDOSBox() (dosboxsupport.cpp) is the way in.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/iosupport.h>
#include <sys/stat.h>

#include "dosboxwii.h"
#include "dosboxsupport.h"
#include "fileop.h"
#include "menu.h"
#include "filelist.h"
#include "libgui/Gui.h"
#include "drivers/Platform.h"
#include "drivers/AudioDriver.h"
#include "drivers/VideoDriver.h"
#include "drivers/EmulatorVideoDriver.h"
#include "drivers/InputDriver.h"
#include "drivers/ogc/wii/WiiPlatform.h"
#include "drivers/KeyboardDriver.h"
#include "drivers/ogc/OgcKeyboardDriver.h"
#include "drivers/MouseDriver.h"
#include "drivers/ogc/OgcMouseDriver.h"
#include "videosupport.h"
#include "input.h"

// Platform composition root: the only place that picks a concrete platform.
// Wii U will select WutPlatform here (Stage 7).
static WiiPlatform platformInstance;
Platform* platform = &platformInstance;

// USB keyboard and mouse (the Wiimote IR pointer is part of the pad state)
static OgcKeyboardDriver keyboardInstance;
KeyboardDriver* keyboard = &keyboardInstance;
static OgcMouseDriver mouseInstance;
MouseDriver* usbMouse = &mouseInstance;

/****************************************************************************
 * SwitchAudioMode
 *
 * Switches between menu sound and emulator sound
 ***************************************************************************/
static void SwitchAudioMode(int mode)
{
	if(mode == 0) // emulator
	{
		platform->getAudio()->stopMenuAudio();
		platform->getAudio()->startEmulatorAudio();
	}
	else // menu
	{
		platform->getAudio()->stopEmulatorAudio();
		platform->getAudio()->startMenuAudio();
	}
}

/****************************************************************************
 * InitApp
 *
 * Brings up the platform (thread, video, audio, input, filesystem, logger
 * drivers) and the GUI text system.
 ***************************************************************************/
#define IMAGE_DECODE_SCRATCH_SIZE ((640 * 480 * 4) + (480 * sizeof(void *)))

static void InitApp()
{
	// stdout/stderr go nowhere; diagnostics use the platform Logger
	extern const devoptab_t dotab_stdnull;
	devoptab_list[STD_OUT] = &dotab_stdnull;
	devoptab_list[STD_ERR] = &dotab_stdnull;

	PlatformConfig platformConfig;
	platformConfig.canvasWidth = 640;
	platformConfig.canvasHeight = 480;
	platform->init(platformConfig);

	FindAppDrive();

	GuiImageData::setDecodeScratch(malloc(IMAGE_DECODE_SCRATCH_SIZE), IMAGE_DECODE_SCRATCH_SIZE);

	keyboard->init();
	usbMouse->init();

	fontSystem = new GuiTextRenderer(font_ttf, font_ttf_size,
		platform->getVideo()->getGlyphRenderer(), platform->getVideo()->getUIScale());
	textTranslator = new GuiTextTranslator();
	textTranslator->loadLanguage(en_lang, en_lang_size);

	platform->getVideo()->startMenuVideo();
	InitGUI();

	InitKeyInjection();
}

/****************************************************************************
 * EnterMenu
 *
 * Emulation -> menu -> emulation handoff, on the HAL.
 ***************************************************************************/
void EnterMenu()
{
	// Typing is cut short rather than waited for: it needs the emulator to
	// keep polling, which it won't while the menu is up.
	AbortKeys();

	SwitchAudioMode(1);

	// Waits for the last frame to reach the screen, and keeps a copy of it
	// for the menu background. Must come before the mode switch below.
	GFX_Suspend();
	platform->getVideo()->startMenuVideo();

	HomeMenu();

	SwitchAudioMode(0);
	GFX_Resume();	// also repaints: DOSBox won't present again until something changes

	QueueKeys();
}

/****************************************************************************
 * ExitApp
 *
 * End of main(). Emulator video was already released by GUI_ShutDown().
 * Shuts every driver down and leaves the app (power off, or back to the
 * loader). Does not return.
 ***************************************************************************/
void ExitApp()
{
	platform->requestExit(EXITACTION_WII_AUTO, false);
}

/****************************************************************************
 * main
 ***************************************************************************/
int main(int argc, char* argv[])
{
	InitApp();
	RunDOSBox(argc, argv);
	ExitApp();
	return 0;
}
