/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * input.cpp
 *
 * Input: the DOSBox GFX_Events() pump (mouse, focus, pause), the HOME and
 * power button checks, and the key injection that types a command typed on
 * the on-screen keyboard into DOS.
 *
 * Pads, keyboard and mouse themselves come from the platform HAL, through
 * the SDL 1.2 event API that sdlshim/SDL_input.cpp implements.
 ***************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dosbox.h"
#include "video.h"
#include "mouse.h"
#include "timer.h"
#include "setup.h"
#include "mapper.h"
#include "keyboard.h"
#include "control.h"
#include "cpu.h"
#include "SDL.h"
#include "drivers/Platform.h"
#include "drivers/InputData.h"
#include "drivers/InputController.h"
#include "input.h"
#include "preferences.h"
#include "inputlogic.h"
#include "videosupport.h"
#include "dosboxwii.h"
#include "dosboxsupport.h"

void GFX_SetTitle(Bit32s cycles,int frameskip,bool paused);

struct SDL_Block {
	struct {
		bool autolock;
		bool autoenable;
		bool requestlock;
		bool locked;
		int xsensitivity;
		int ysensitivity;
	} mouse;
	Bitu num_joysticks;
};

static SDL_Block sdl;

//Globals for keyboard initialisation
bool startup_state_numlock=false;
bool startup_state_capslock=false;

/****************************************************************************
 * Pause and kill switch
 ***************************************************************************/
static void KillSwitch(bool pressed) {
	if (!pressed)
		return;
	throw 1;
}

static void PauseDOSBox(bool pressed) {
	if (!pressed)
		return;
	GFX_SetTitle(-1,-1,true);
	bool paused = true;
	KEYBOARD_ClrBuffer();
	SDL_Delay(500);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// flush event queue.
	}

	while (paused) {
		// Not SDL_WaitEvent(): it never looks at the power button, and nothing
		// else does while paused (GFX_Events() is not running).
		if (!SDL_PollEvent(&event)) {
			InputHal_Update();
			CheckExit();
			SDL_Delay(1);
			continue;
		}
		switch (event.type) {

			case SDL_QUIT: KillSwitch(true); break;
			case SDL_KEYDOWN:   // Must use Pause/Break Key to resume.
			case SDL_KEYUP:
			if(event.key.keysym.sym == SDLK_PAUSE) {

				paused = false;
				GFX_SetTitle(-1,-1,false);
				break;
			}
		}
	}
}

/****************************************************************************
 * Mouse capture
 ***************************************************************************/
void GFX_CaptureMouse(void) {
	sdl.mouse.locked=!sdl.mouse.locked;
	if (sdl.mouse.locked) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
        mouselocked=sdl.mouse.locked;
}

bool mouselocked; //Global variable for mapper
//! Takes the mouse sensitivity from the [sdl] section (NULL is no section).
static void ReadSensitivity(Section_prop * section) {
	Prop_multival * sensitivity = section ? section->Get_multival("sensitivity") : NULL;

	if(!sensitivity)
		return;

	sdl.mouse.xsensitivity = sensitivity->GetSection()->Get_int("xsens");
	sdl.mouse.ysensitivity = sensitivity->GetSection()->Get_int("ysens");
}

/****************************************************************************
 * The [sdl] config section: start up and shut down
 ***************************************************************************/
static void GUI_ShutDown(Section * /*sec*/) {
	GFX_HalShutdown();
	if (sdl.mouse.locked) GFX_CaptureMouse();
}


/* The mouse buttons as mapper events, so that the controller screens can put one on a pad button (a bind to a button
 * of a pad group, like a bind to a key). The pointer's own buttons, the Wiimote's A and B and a USB mouse's
 * buttons, reach DOS through the SDL events as before. */
static void MouseLeftButton(bool pressed) { if(pressed) Mouse_ButtonPressed(0); else Mouse_ButtonReleased(0); }
static void MouseRightButton(bool pressed) { if(pressed) Mouse_ButtonPressed(1); else Mouse_ButtonReleased(1); }
static void MouseMiddleButton(bool pressed) { if(pressed) Mouse_ButtonPressed(2); else Mouse_ButtonReleased(2); }

void GUI_StartUp(Section * sec) {
	sec->AddDestroyFunction(&GUI_ShutDown);
	Section_prop * section=static_cast<Section_prop *>(sec);

	// Take the display for the emulator (InitApp() left the menu on it)
	GFX_HalInit();

	sdl.mouse.locked=false;
	mouselocked=false; //Global for mapper
	sdl.mouse.requestlock=false;

	// Always captured: motion reaches DOS as relative movement and the first
	// click is not swallowed to capture. [sdl] autolock is ignored, so an old
	// dosbox.conf with autolock=true behaves the same as a new one.
	sdl.mouse.autoenable=true;
	sdl.mouse.autolock=false;

	ReadSensitivity(section);

	/* Setup Mouse correctly if fullscreen */
	if(GFX_IsFullscreen()) GFX_CaptureMouse();

	/* Captured from the start (see above); GFX_CaptureMouse() toggles */
	if(!sdl.mouse.locked) GFX_CaptureMouse();

	/* Get some Event handlers */
	MAPPER_AddHandler(KillSwitch,MK_f9,MMOD1,"shutdown","ShutDown");
	MAPPER_AddHandler(MouseLeftButton,MK_none,0,"mouseleft","Mouse L");
	MAPPER_AddHandler(MouseRightButton,MK_none,0,"mouseright","Mouse R");
	MAPPER_AddHandler(MouseMiddleButton,MK_none,0,"mousemid","Mouse M");
#if C_DEBUG
	/* Pause binds with activate-debugger */
#else
	MAPPER_AddHandler(&PauseDOSBox, MK_pause, MMOD2, "pause", "Pause DBox");
#endif
	/* Get Keyboard state of numlock and capslock */
	SDLMod keystate = SDL_GetModState();
	if(keystate&KMOD_NUM) startup_state_numlock = true;
	if(keystate&KMOD_CAPS) startup_state_capslock = true;
}

/****************************************************************************
 * Mouse events
 ***************************************************************************/
void Mouse_AutoLock(bool enable) {
	sdl.mouse.autolock=enable;
	if (sdl.mouse.autoenable) sdl.mouse.requestlock=enable;
	else {
		SDL_ShowCursor(enable?SDL_DISABLE:SDL_ENABLE);
		sdl.mouse.requestlock=false;
	}
}

void Mouse_SensitivityChanged(void) {
	ReadSensitivity(Config_GetSection("sdl"));
}

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) {
	int width, height;
	bool fullscreen;
	GFX_GetSize(width, height, fullscreen);
	if (MouseMotionForwarded(sdl.mouse.locked, sdl.mouse.autoenable))
		Mouse_CursorMoved((float)motion->xrel*sdl.mouse.xsensitivity/100.0f,
						  (float)motion->yrel*sdl.mouse.ysensitivity/100.0f,
						  (float)motion->x/(width-1)*sdl.mouse.xsensitivity/100.0f,
						  (float)motion->y/(height-1)*sdl.mouse.ysensitivity/100.0f,
						  sdl.mouse.locked);
}

static void HandleMouseButton(SDL_MouseButtonEvent * button) {
	switch (button->state) {
	case SDL_PRESSED:
		if (MouseClickSwallowed(sdl.mouse.requestlock, sdl.mouse.locked)) {
			GFX_CaptureMouse();
			// Don't pass click to mouse handler
			break;
		}
		if (!sdl.mouse.autoenable && sdl.mouse.autolock && button->button == SDL_BUTTON_MIDDLE) {
			GFX_CaptureMouse();
			break;
		}
		switch (button->button) {
		case SDL_BUTTON_LEFT:
			Mouse_ButtonPressed(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonPressed(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonPressed(2);
			break;
		}
		break;
	case SDL_RELEASED:
		switch (button->button) {
		case SDL_BUTTON_LEFT:
			Mouse_ButtonReleased(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonReleased(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonReleased(2);
			break;
		}
		break;
	}
}

/****************************************************************************
 * GFX_Events
 *
 * Called by the core to poll input.
 ***************************************************************************/
void GFX_LosingFocus(void) {
	MAPPER_LosingFocus();
}

void GFX_Events() {
	// Scan the pads, keyboard and mice; posts the events polled below
	InputHal_Update();

	CheckExit();

	// HOME, a controller combo or the keyboard's GUI key: hand the display to the menu.
	// Both are read before either is acted on, so a tap is never left pending.
	const bool padRequest = isMenuRequested();
	const bool keyRequest = InputHal_TakeMenuKeyTap() != 0;

	if(padRequest || keyRequest)
	{
		EnterMenu();
		InputHal_MenuClosed();

		// Restart was pressed: the menu has closed and handed the machine back,
		// so leave the core the way the killswitch does (see RunDOSBox())
		if(RestartRequested())
			throw 1;
	}

	SDL_Event event;
#if defined (REDUCE_JOYSTICK_POLLING)
	static int poll_delay = 0;
	int time = GetTicks();
	if (time - poll_delay > 20) {
		poll_delay = time;
		if (sdl.num_joysticks > 0) SDL_JoystickUpdate();
		MAPPER_UpdateJoysticks();
	}
#endif
	while (SDL_PollEvent(&event)) {

		switch (event.type) {
		case SDL_MOUSEMOTION:
			HandleMouseMotion(&event.motion);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			HandleMouseButton(&event.button);
			break;
		case SDL_QUIT:
			throw(0);
			break;
		default:
			void MAPPER_CheckEvent(SDL_Event * event);
			MAPPER_CheckEvent(&event);
		}
	}
}


/****************************************************************************
 * InitInput
 *
 * Keyboard, mouse and joysticks come from the HAL.
 ***************************************************************************/
void InitInput()
{
	InputHal_Init();
	sdl.num_joysticks=SDL_NumJoysticks();
}

/****************************************************************************
 * isMenuRequested
 *
 * Polled once per emulation event pass (GFX_Events), after InputHal_Update()
 * has scanned the platform input. See MenuRequestFromPad() for what counts.
 ***************************************************************************/
bool isMenuRequested()
{
	for(int i = 0; i < 4; i++)
	{
		if(MenuRequestFromPad(controller[i]->getPadData()))
			return true;
	}
	return false;
}

/****************************************************************************
 * GetControllerSummary
 *
 * The pad data says which kinds of controller make up the channel (a Wiimote
 * with a Nunchuk is one channel with two profiles connected).
 ***************************************************************************/
bool GetControllerSummary(int channel, char * buf, size_t size)
{
	static const struct { uint32_t profile; const char * name; } kinds[] =
	{
		{ INPUT_HW_WIIMOTE, "Wiimote" },
		{ INPUT_HW_NUNCHUK, "Nunchuk" },
		{ INPUT_HW_CLASSIC, "Classic" },
		{ INPUT_HW_GAMECUBE, "GameCube" },
		{ INPUT_HW_WUPC, "Wii U Pro" },
		{ INPUT_HW_DRC, "GamePad" },
	};

	if(channel < 0 || channel >= 4 || !controller[channel] || !buf || size == 0)
		return false;

	const InputPadData & pad = controller[channel]->getPadData();
	size_t used = 0;
	char text[64];

	text[0] = 0;

	for(size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
	{
		if(!pad.hw_connected[kinds[i].profile])
			continue;

		used += snprintf(text + used, sizeof(text) - used, "%s%s", used ? " + " : "", kinds[i].name);

		if(used >= sizeof(text))
			break;
	}

	if(text[0] == 0)
		return false;

	snprintf(buf, size, "%s", text);
	return true;
}

/****************************************************************************
 * CheckExit
 *
 * Polled once per emulation event pass. Without this, the power button
 * (console or Wiimote) is ignored while a game is running.
 ***************************************************************************/
void CheckExit()
{
	if(platform->shouldExit())
		ExitApp();
}
