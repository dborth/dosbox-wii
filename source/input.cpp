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
#include "cpu.h"
#include "SDL.h"
#include "drivers/Platform.h"
#include "drivers/InputData.h"
#include "drivers/InputController.h"
#include "input.h"
#include "videosupport.h"
#include "dosboxwii.h"

void GFX_SetTitle(Bit32s cycles,int frameskip,bool paused);

enum PRIORITY_LEVELS {
	PRIORITY_LEVEL_PAUSE,
	PRIORITY_LEVEL_LOWEST,
	PRIORITY_LEVEL_LOWER,
	PRIORITY_LEVEL_NORMAL,
	PRIORITY_LEVEL_HIGHER,
	PRIORITY_LEVEL_HIGHEST
};


struct SDL_Block {
	struct {
		PRIORITY_LEVELS focus;
		PRIORITY_LEVELS nofocus;
	} priority;
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

char dosboxCommand[1024] = { 0 };
static char shiftkey[130];

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
		SDL_WaitEvent(&event);    // since we're not polling, cpu usage drops to 0.
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

void GFX_UpdateSDLCaptureState(void) {
	if (sdl.mouse.locked) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
	CPU_Reset_AutoAdjust();
	GFX_SetTitle(-1,-1,false);
}

bool mouselocked; //Global variable for mapper
static void CaptureMouse(bool pressed) {
	if (!pressed)
		return;
	GFX_CaptureMouse();
}

/****************************************************************************
 * The [sdl] config section: start up and shut down
 ***************************************************************************/
static void GUI_ShutDown(Section * /*sec*/) {
	GFX_HalShutdown();
	if (sdl.mouse.locked) GFX_CaptureMouse();
}


void Restart(bool pressed);

void GUI_StartUp(Section * sec) {
	sec->AddDestroyFunction(&GUI_ShutDown);
	Section_prop * section=static_cast<Section_prop *>(sec);

	// Take the display for the emulator (InitApp() left the menu on it)
	GFX_HalInit();

	Prop_multival* p=section->Get_multival("priority");
	std::string focus = p->GetSection()->Get_string("active");
	std::string notfocus = p->GetSection()->Get_string("inactive");

	if      (focus == "lowest")  { sdl.priority.focus = PRIORITY_LEVEL_LOWEST;  }
	else if (focus == "lower")   { sdl.priority.focus = PRIORITY_LEVEL_LOWER;   }
	else if (focus == "normal")  { sdl.priority.focus = PRIORITY_LEVEL_NORMAL;  }
	else if (focus == "higher")  { sdl.priority.focus = PRIORITY_LEVEL_HIGHER;  }
	else if (focus == "highest") { sdl.priority.focus = PRIORITY_LEVEL_HIGHEST; }

	if      (notfocus == "lowest")  { sdl.priority.nofocus=PRIORITY_LEVEL_LOWEST;  }
	else if (notfocus == "lower")   { sdl.priority.nofocus=PRIORITY_LEVEL_LOWER;   }
	else if (notfocus == "normal")  { sdl.priority.nofocus=PRIORITY_LEVEL_NORMAL;  }
	else if (notfocus == "higher")  { sdl.priority.nofocus=PRIORITY_LEVEL_HIGHER;  }
	else if (notfocus == "highest") { sdl.priority.nofocus=PRIORITY_LEVEL_HIGHEST; }
	else if (notfocus == "pause")   {
		/* we only check for pause here, because it makes no sense
		 * for DOSBox to be paused while it has focus
		 */
		sdl.priority.nofocus=PRIORITY_LEVEL_PAUSE;
	}

	sdl.mouse.locked=false;
	mouselocked=false; //Global for mapper
	sdl.mouse.requestlock=false;

	sdl.mouse.autoenable=section->Get_bool("autolock");
	if (!sdl.mouse.autoenable) SDL_ShowCursor(SDL_DISABLE);
	sdl.mouse.autolock=false;

	Prop_multival* p3 = section->Get_multival("sensitivity");
	sdl.mouse.xsensitivity = p3->GetSection()->Get_int("xsens");
	sdl.mouse.ysensitivity = p3->GetSection()->Get_int("ysens");

	/* Setup Mouse correctly if fullscreen */
	if(GFX_IsFullscreen()) GFX_CaptureMouse();

	/* Get some Event handlers */
	MAPPER_AddHandler(KillSwitch,MK_f9,MMOD1,"shutdown","ShutDown");
	MAPPER_AddHandler(CaptureMouse,MK_f10,MMOD1,"capmouse","Cap Mouse");
	MAPPER_AddHandler(Restart,MK_home,MMOD1|MMOD2,"restart","Restart");
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

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) {
	int width, height;
	bool fullscreen;
	GFX_GetSize(width, height, fullscreen);
	if (sdl.mouse.locked || !sdl.mouse.autoenable)
		Mouse_CursorMoved((float)motion->xrel*sdl.mouse.xsensitivity/100.0f,
						  (float)motion->yrel*sdl.mouse.ysensitivity/100.0f,
						  (float)motion->x/(width-1)*sdl.mouse.xsensitivity/100.0f,
						  (float)motion->y/(height-1)*sdl.mouse.ysensitivity/100.0f,
						  sdl.mouse.locked);
}

static void HandleMouseButton(SDL_MouseButtonEvent * button) {
	switch (button->state) {
	case SDL_PRESSED:
		if (sdl.mouse.requestlock && !sdl.mouse.locked) {
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

	// HOME pressed: hand the display to the menu
	if(isMenuRequested())
		EnterMenu();

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
		case SDL_ACTIVEEVENT:
			if (event.active.state & SDL_APPINPUTFOCUS) {
				if (event.active.gain) {
					if (GFX_IsFullscreen() && !sdl.mouse.locked)
						GFX_CaptureMouse();
					CPU_Disable_SkipAutoAdjust();
				} else {
					if (sdl.mouse.locked) {
						GFX_CaptureMouse();
					}
					GFX_LosingFocus();
					CPU_Enable_SkipAutoAdjust();
				}
			}

			/* Non-focus priority is set to pause; check to see if we've lost window or input focus
			 * i.e. has the window been minimised or made inactive?
			 */
			if (sdl.priority.nofocus == PRIORITY_LEVEL_PAUSE) {
				if ((event.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) && (!event.active.gain)) {
					/* Window has lost focus, pause the emulator.
					 * This is similar to what PauseDOSBox() does, but the exit criteria is different.
					 * Instead of waiting for the user to hit Alt-Break, we wait for the window to
					 * regain window or input focus.
					 */
					bool paused = true;
					SDL_Event ev;

					GFX_SetTitle(-1,-1,true);
					KEYBOARD_ClrBuffer();
//					SDL_Delay(500);
//					while (SDL_PollEvent(&ev)) {
						// flush event queue.
//					}

					while (paused) {
						// WaitEvent waits for an event rather than polling, so CPU usage drops to zero
						SDL_WaitEvent(&ev);

						switch (ev.type) {
						case SDL_QUIT: throw(0); break; // a bit redundant at linux at least as the active events gets before the quit event.
						case SDL_ACTIVEEVENT:     // wait until we get window focus back
							if (ev.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) {
								// We've got focus back, so unpause and break out of the loop
								if (ev.active.gain) {
									paused = false;
									GFX_SetTitle(-1,-1,false);
								}

								/* Now poke a "release ALT" command into the keyboard buffer
								 * we have to do this, otherwise ALT will 'stick' and cause
								 * problems with the app running in the DOSBox.
								 */
								KEYBOARD_AddKey(KBD_leftalt, false);
								KEYBOARD_AddKey(KBD_rightalt, false);
							}
							break;
						}
					}
				}
			}
			break;
		case SDL_MOUSEMOTION:
			HandleMouseMotion(&event.motion);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			HandleMouseButton(&event.button);
			break;
		case SDL_VIDEORESIZE:
//			HandleVideoResize(&event.resize);
			break;
		case SDL_QUIT:
			throw(0);
			break;
		default:
			void MAPPER_CheckEvent(SDL_Event * event);
			MAPPER_CheckEvent(&event);
		}
	}

	// Typed after the queue is drained, so the character is taken next pass
	PumpKeys();
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
 * has scanned the platform input.
 ***************************************************************************/
bool isMenuRequested()
{
	for(int i = 0; i < 4; i++)
	{
		if(controller[i]->getPadData().buttons_h & INPUT_BTN_HOME)
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

/****************************************************************************
 * Key injection (on-screen keyboard -> DOS)
 *
 * The command typed on the on-screen keyboard is typed into DOS by posting
 * key events to the SDL event queue, one character every few emulated
 * milliseconds. It is driven from GFX_Events() (PumpKeys), on the emulation
 * thread, so it only ever runs while the emulator is polling and the
 * mapper is only ever touched from one thread. There is no typing thread to
 * be starved, woken late or raced.
 *
 * A '\n' in the command is typed as Return.
 ***************************************************************************/
#define KEY_PACE_TICKS 5 // GFX_Events() calls (about 1 ms of DOS time each) per character

static char keyBuffer[sizeof(dosboxCommand)];
static size_t keyPos = 0;
static bool keyActive = false;
static int keyWait = 0;

static void PostKey(int type, int sym)
{
	SDL_Event event;
	memset(&event, 0, sizeof(event));
	event.type = type;
	event.key.state = (type == SDL_KEYDOWN) ? SDL_PRESSED : SDL_RELEASED;
	event.key.keysym.sym = (SDLKey)sym;
	SDL_PushEvent(&event);
}

static void TypeChar(unsigned char c)
{
	int sym = c;
	bool shift = false;

	if(c == '\n' || c == '\r')
	{
		sym = SDLK_RETURN;
	}
	else if(c >= 65 && c <= 90)
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
}

//! Types the next character of the pending command. Called from GFX_Events().
void PumpKeys()
{
	if(!keyActive)
		return;

	if(keyWait > 0)
	{
		keyWait--;
		return;
	}

	// A character is up to four events, posted all or nothing. Never post
	// into a backlog (the queue is 256 deep): wait for the emulator to drain it.
	if(InputHal_PendingEvents() > 128)
		return;

	unsigned char c = (unsigned char)keyBuffer[keyPos];
	if(c == 0)
	{
		keyActive = false;
		return;
	}

	TypeChar(c);
	keyPos++;
	keyWait = KEY_PACE_TICKS;
}

void InitKeyInjection()
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
}

//! Takes the command the on-screen keyboard left in dosboxCommand and starts typing it.
void QueueKeys()
{
	if(dosboxCommand[0] == 0)
		return;

	snprintf(keyBuffer, sizeof(keyBuffer), "%s", dosboxCommand);
	keyPos = 0;
	keyWait = 0;
	keyActive = true;

	dosboxCommand[0] = 0;
}

//! Cuts any typing short. Characters already posted are whole, so nothing is left held.
void AbortKeys()
{
	keyActive = false;
}
