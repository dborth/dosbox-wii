/****************************************************************************
 * DOSBox Wii
 * Tantric 2009-2010
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/iosupport.h>

#include "wiihardware.h"
#include "menu.h"
#include "filelist.h"
#include "libgui/Gui.h"
#include "drivers/Platform.h"
#include "drivers/AudioDriver.h"
#include "drivers/VideoDriver.h"
#include "drivers/EmulatorVideoDriver.h"
#include "drivers/InputDriver.h"
#include "drivers/InputController.h"
#include "drivers/ogc/wii/WiiPlatform.h"
#include "drivers/KeyboardDriver.h"
#include "drivers/ogc/OgcKeyboardDriver.h"
#include "drivers/MouseDriver.h"
#include "drivers/ogc/OgcMouseDriver.h"
#include "dosbox/gui/gfx_hal.h"
#include "dosbox/include/input_hal.h"

void MAPPER_CheckEvent(SDL_Event * event);

// Platform composition root: the only place that picks a concrete platform.
// Wii U will select WutPlatform here (Stage 7).
static WiiPlatform platformInstance;
Platform* platform = &platformInstance;

// USB keyboard and mouse (the Wiimote IR pointer is part of the pad state)
static OgcKeyboardDriver keyboardInstance;
KeyboardDriver* keyboard = &keyboardInstance;
static OgcMouseDriver mouseInstance;
MouseDriver* usbMouse = &mouseInstance;

char appDrive[MAX_APP_DRIVE_LEN];
char appPath[MAX_APP_PATH_LEN];
char dosboxCommand[1024] = { 0 };
static lwp_t keythread = LWP_THREAD_NULL;
static char shiftkey[130];

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

static void * PressKeys (void *arg)
{
	SDL_Event event;
	int shift;
	u16 i;
	
	memset(shiftkey, 0, sizeof(shiftkey));
	shiftkey[33] = 49;
	shiftkey[34] = 39;
	shiftkey[35] = 51;
	shiftkey[36] = 52;
	shiftkey[37] = 53;
	shiftkey[38] = 55;
	shiftkey[40] = 57;
	shiftkey[41] = 48;
	shiftkey[42] = 56;
	shiftkey[58] = 59;
	shiftkey[60] = 44;
	shiftkey[62] = 46;
	shiftkey[63] = 47;
	shiftkey[64] = 50;
	shiftkey[94] = 54;
	shiftkey[95] = 45;
	shiftkey[126] = 96;

	while(1)
	{
		LWP_SuspendThread(keythread);
		usleep(1200);

		for(i=0; i<strlen(dosboxCommand); i++)
		{
			shift=0;

			if((dosboxCommand[i] >= 65 && dosboxCommand[i] <= 90))
			{
				dosboxCommand[i] += 32;
				shift = 1;
			}
			else if(dosboxCommand[i] > 0 && dosboxCommand[i] < 130 && 
					shiftkey[(int)dosboxCommand[i]] > 0)
			{
				dosboxCommand[i] = shiftkey[(int)dosboxCommand[i]];
				shift = 1;
			}

			if(shift)
			{
				event.type = SDL_KEYDOWN;
				event.key.keysym.sym = SDLK_LSHIFT;
				MAPPER_CheckEvent(&event);
				usleep(600);
			}
			
			// hack to allow mappings of SDL keys > 127
			int keyoffset = 0;
			if(dosboxCommand[i] >= 14 && dosboxCommand[i] <= 25)
				keyoffset = 268; // F1-F12 (282-293)

			event.type = SDL_KEYDOWN;
			event.key.keysym.sym = (SDLKey)((int)dosboxCommand[i]+keyoffset);
			MAPPER_CheckEvent(&event);
			usleep(600);

			event.type = SDL_KEYUP;
			event.key.keysym.sym = (SDLKey)((int)dosboxCommand[i]+keyoffset);
			MAPPER_CheckEvent(&event);
			usleep(600);

			if(shift)
			{
				event.type = SDL_KEYUP;
				event.key.keysym.sym = SDLK_LSHIFT;
				MAPPER_CheckEvent(&event);
				usleep(600);
			}
		}
		dosboxCommand[0] = 0;
	}
	return NULL;
}

/****************************************************************************
 * WiiInit
 *
 * Brings up the platform (thread, video, audio, input, filesystem, logger
 * drivers) and the GUI text system.
 ***************************************************************************/
void WiiInit()
{
	// stdout/stderr go nowhere; diagnostics use the platform Logger
	extern const devoptab_t dotab_stdnull;
	devoptab_list[STD_OUT] = &dotab_stdnull;
	devoptab_list[STD_ERR] = &dotab_stdnull;

	PlatformConfig platformConfig;
	platformConfig.canvasWidth = 640;
	platformConfig.canvasHeight = 480;
	platform->init(platformConfig);

	keyboard->init();
	usbMouse->init();

	fontSystem = new GuiTextRenderer(font_ttf, font_ttf_size,
		platform->getVideo()->getGlyphRenderer(), platform->getVideo()->getUIScale());
	textTranslator = new GuiTextTranslator();
	textTranslator->loadLanguage(en_lang, en_lang_size);

	platform->getVideo()->startMenuVideo();
	InitGUI();

	LWP_CreateThread (&keythread, PressKeys, NULL, NULL, 0, 65);
	appPath[0] = 0;
}

void CreateAppPath(char origpath[])
{
	char * path = strdup(origpath); // make a copy so we don't mess up original

	if(!path)
		return;

	char * loc = strrchr(path,'/');
	if (loc != NULL)
		*loc = 0; // strip file name

	strncpy(appPath, path, MAX_APP_PATH_LEN);
	appPath[MAX_APP_PATH_LEN - 1] = 0;

	loc = strchr(path,'/');
	if (loc != NULL)
		*loc = 0; // strip path

	strncpy(appDrive, path, MAX_APP_DRIVE_LEN);
	appDrive[MAX_APP_DRIVE_LEN - 1] = 0;

	free(path);
}

/****************************************************************************
 * MenuRequested
 *
 * Polled once per emulation event pass (GFX_Events), after InputHal_Update()
 * has scanned the platform input.
 ***************************************************************************/
bool MenuRequested()
{
	for(int i = 0; i < 4; i++)
	{
		if(controller[i]->getPadData().buttons_h & INPUT_BTN_HOME)
			return true;
	}
	return false;
}

/****************************************************************************
 * WiiCheckExit
 *
 * Polled once per emulation event pass. Without this, the power button
 * (console or Wiimote) is ignored while a game is running.
 ***************************************************************************/
void WiiCheckExit()
{
	if(platform->shouldExit())
		platform->requestExit(EXITACTION_WII_AUTO, false);
}

/****************************************************************************
 * WiiMenu
 *
 * Emulation -> menu -> emulation handoff, on the HAL.
 ***************************************************************************/
void WiiMenu()
{
	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(keythread))
		usleep(100);

	SwitchAudioMode(1);

	// Waits for the last frame to reach the screen, and keeps a copy of it
	// for the menu background. Must come before the mode switch below.
	GFX_Suspend();
	platform->getVideo()->startMenuVideo();

	HomeMenu();

	SwitchAudioMode(0);
	GFX_Resume();	// also repaints: DOSBox won't present again until something changes

	if(dosboxCommand[0] != 0)
		LWP_ResumeThread(keythread);
}

/****************************************************************************
 * WiiFinished
 *
 * End of main(). Emulator video was already released by GUI_ShutDown().
 * Shuts every driver down and leaves the app (power off, or back to the
 * loader). Does not return.
 ***************************************************************************/
void WiiFinished()
{
	platform->requestExit(EXITACTION_WII_AUTO, false);
}
