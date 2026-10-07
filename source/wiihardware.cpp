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
#include <sys/stat.h>

#include "wiihardware.h"
#include "menu.h"
#include "filelist.h"
#include "libgui/Gui.h"
#include "drivers/Platform.h"
#include "drivers/Thread.h"
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
#include "sdlshim/SDL_input.h"

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

/****************************************************************************
 * Key injection (on-screen keyboard -> DOS)
 *
 * A worker thread types a command by posting key events to the SDL event
 * queue. They are consumed by GFX_Events() on the emulation thread, so the
 * mapper is only ever touched from one thread. The thread posts one
 * character at a time and waits for the queue to drain, so it never gets
 * ahead of the emulator.
 ***************************************************************************/
// Allocated once and never deleted: ~Thread() joins, and this thread is
// parked on keyCond at exit.
static Thread * keyThread = NULL;
static Mutex * keyMutex = NULL;
static Cond * keyCond = NULL;
static char keyPending[sizeof(dosboxCommand)];
static bool keyHasPending = false;	// guarded by keyMutex
static bool keyTyping = false;		// pending or being typed. guarded by keyMutex
static volatile bool keyAbort = false;

static void WakeKeyThread(void)
{
	keyMutex->lock();
	keyCond->signal();
	keyMutex->unlock();
}

static bool PostKey(int type, int sym)
{
	SDL_Event event;
	memset(&event, 0, sizeof(event));
	event.type = type;
	event.key.state = (type == SDL_KEYDOWN) ? SDL_PRESSED : SDL_RELEASED;
	event.key.keysym.sym = (SDLKey)sym;

	while(SDL_PushEvent(&event) != 0)
	{
		if(keyAbort || keyThread->stopRequested())
			return false;
		platform->getThread()->sleepMilliseconds(1);
	}
	return true;
}

static void TypeCommand(const char * command)
{
	for(size_t i=0; command[i] != 0 && !keyAbort && !keyThread->stopRequested(); i++)
	{
		unsigned char c = (unsigned char)command[i];
		int sym = c;
		bool shift = false;

		if(c >= 65 && c <= 90)
		{
			sym = c + 32;
			shift = true;
		}
		else if(c > 0 && c < 130 && shiftkey[c] > 0)
		{
			sym = shiftkey[c];
			shift = true;
		}

		// hack to allow mappings of SDL keys > 127
		if(sym >= 14 && sym <= 25)
			sym += 268; // F1-F12 (282-293)

		// One character is posted whole, so a shift is never left held
		if(shift)
			PostKey(SDL_KEYDOWN, SDLK_LSHIFT);
		PostKey(SDL_KEYDOWN, sym);
		PostKey(SDL_KEYUP, sym);
		if(shift)
			PostKey(SDL_KEYUP, SDLK_LSHIFT);

		// Wait for the emulator to take them. Bounded: it may not be polling.
		for(int waited = 0; waited < 500 && InputHal_PendingEvents() > 0
			&& !keyAbort && !keyThread->stopRequested(); waited++)
			platform->getThread()->sleepMilliseconds(1);
	}
}

static void * PressKeys(void *arg)
{
	char command[sizeof(keyPending)];

	while(true)
	{
		keyMutex->lock();
		while(!keyHasPending && !keyThread->stopRequested())
			keyCond->wait(*keyMutex);

		if(keyThread->stopRequested())
		{
			keyMutex->unlock();
			break;
		}

		memcpy(command, keyPending, sizeof(command));
		keyHasPending = false;
		keyMutex->unlock();

		TypeCommand(command);

		keyMutex->lock();
		keyTyping = false;
		keyMutex->unlock();
	}
	return NULL;
}

static void InitKeyInjection()
{
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

	keyMutex = new Mutex();
	keyCond = new Cond();
	keyThread = new Thread();
	keyThread->start(PressKeys, NULL, 16384, ThreadPriority::Normal, WakeKeyThread);
}

//! Hands the command the on-screen keyboard left in dosboxCommand to the key thread.
static void QueueKeys()
{
	if(dosboxCommand[0] == 0)
		return;

	keyMutex->lock();
	memcpy(keyPending, dosboxCommand, sizeof(keyPending));
	keyPending[sizeof(keyPending) - 1] = 0;
	keyHasPending = true;
	keyTyping = true;
	keyCond->signal();
	keyMutex->unlock();

	dosboxCommand[0] = 0;
}

//! Cuts any typing short and waits for the key thread to go idle.
static void AbortKeys()
{
	keyAbort = true;
	while(true)
	{
		keyMutex->lock();
		keyHasPending = false;
		bool typing = keyTyping;
		keyMutex->unlock();

		if(!typing)
			break;
		platform->getThread()->sleepMilliseconds(1);
	}
	keyAbort = false;
}

/****************************************************************************
 * WiiInit
 *
 * Brings up the platform (thread, video, audio, input, filesystem, logger
 * drivers) and the GUI text system.
 ***************************************************************************/
#define IMAGE_DECODE_SCRATCH_SIZE ((640 * 480 * 4) + (480 * sizeof(void *)))

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
	appPath[0] = 0;
}

/****************************************************************************
 * FindAppDrive
 *
 * Storage is found by looking, not by where the app was launched from.
 ***************************************************************************/
void FindAppDrive()
{
	static const int devices[] = { DEVICE_SD, DEVICE_USB, DEVICE_USB2, DEVICE_USB3 };
	const int count = sizeof(devices) / sizeof(devices[0]);
	FileSystemDriver * fs = platform->getFileSystem();

	appDrive[0] = 0;

	// Pass 0 wants a folder that already exists; pass 1 creates one
	for(int pass = 0; pass < 2; pass++)
	{
		for(int i = 0; i < count; i++)
		{
			const char * mount = fs->getMountPath(devices[i]); // eg. "sd:/"
			if(!mount || mount[0] == 0)
				continue;

			char dir[MAX_APP_DRIVE_LEN + 16];
			snprintf(dir, sizeof(dir), "%s%s", mount, DOSBOX_DIR_NAME);

			struct stat st;
			bool usable = (stat(dir, &st) == 0 && S_ISDIR(st.st_mode));

			if(!usable && pass == 1)
			{
				mkdir(dir, 0777);
				usable = (stat(dir, &st) == 0 && S_ISDIR(st.st_mode));
			}

			if(!usable)
				continue;

			// appDrive is the device without the trailing slash: "sd:"
			snprintf(appDrive, MAX_APP_DRIVE_LEN, "%s", mount);
			size_t len = strlen(appDrive);
			if(len > 0 && appDrive[len - 1] == '/')
				appDrive[len - 1] = 0;
			return;
		}
	}

	LOG_ERROR("No storage device with a %s folder, and none could create one", DOSBOX_DIR_NAME);
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
