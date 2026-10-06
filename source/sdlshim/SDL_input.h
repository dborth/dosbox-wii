/****************************************************************************
 * DOSBox Wii
 * SDL_input.h
 *
 * DOSBox's input layer, implemented on the platform HAL (InputDriver,
 * KeyboardDriver, MouseDriver). Replaces the SDL 1.2 events, keyboard,
 * mouse and joystick the core used to get from the SDL Wii port.
 *
 * The core's input code (sdl_mapper.cpp, sdlmain.cpp) is written in terms
 * of SDL 1.2's event structures and function names, and the mapper's
 * saved bindings in terms of its key and joystick numbering. Those names
 * and numbers are kept, so that code and users' mapper files stay as they
 * are, but everything below is implemented here: no SDL library is
 * linked or included.
 *
 * Deliberately includes only <stdint.h>: it is used by DOSBox core files
 * and platform files whose headers collide (see gfx_hal.h).
 ***************************************************************************/
#ifndef SDL_INPUT_H
#define SDL_INPUT_H

#include <stdint.h>

typedef uint8_t  Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int16_t  Sint16;
typedef int32_t  Sint32;

#include "input_keys.h"

#define SDL_PRESSED   1
#define SDL_RELEASED  0
#define SDL_ENABLE    1
#define SDL_DISABLE   0
#define SDL_QUERY    (-1)
#define SDL_IGNORE    0

/* ---- Events --------------------------------------------------------- */

enum {
	SDL_NOEVENT = 0,
	SDL_ACTIVEEVENT,        /* application loses/gains focus */
	SDL_KEYDOWN,
	SDL_KEYUP,
	SDL_MOUSEMOTION,
	SDL_MOUSEBUTTONDOWN,
	SDL_MOUSEBUTTONUP,
	SDL_JOYAXISMOTION,
	SDL_JOYBALLMOTION,
	SDL_JOYHATMOTION,
	SDL_JOYBUTTONDOWN,
	SDL_JOYBUTTONUP,
	SDL_QUIT,
	SDL_SYSWMEVENT,
	SDL_EVENT_RESERVEDA,
	SDL_EVENT_RESERVEDB,
	SDL_VIDEORESIZE,
	SDL_VIDEOEXPOSE,
	SDL_USEREVENT = 24,
	SDL_NUMEVENTS = 32
};

#define SDL_APPMOUSEFOCUS 0x01
#define SDL_APPINPUTFOCUS 0x02
#define SDL_APPACTIVE     0x04

#define SDL_BUTTON(X)        (1 << ((X)-1))
#define SDL_BUTTON_LEFT      1
#define SDL_BUTTON_MIDDLE    2
#define SDL_BUTTON_RIGHT     3
#define SDL_BUTTON_LMASK     SDL_BUTTON(SDL_BUTTON_LEFT)
#define SDL_BUTTON_MMASK     SDL_BUTTON(SDL_BUTTON_MIDDLE)
#define SDL_BUTTON_RMASK     SDL_BUTTON(SDL_BUTTON_RIGHT)

#define SDL_HAT_CENTERED 0x00
#define SDL_HAT_UP       0x01
#define SDL_HAT_RIGHT    0x02
#define SDL_HAT_DOWN     0x04
#define SDL_HAT_LEFT     0x08
#define SDL_HAT_RIGHTUP    (SDL_HAT_RIGHT|SDL_HAT_UP)
#define SDL_HAT_RIGHTDOWN  (SDL_HAT_RIGHT|SDL_HAT_DOWN)
#define SDL_HAT_LEFTUP     (SDL_HAT_LEFT|SDL_HAT_UP)
#define SDL_HAT_LEFTDOWN   (SDL_HAT_LEFT|SDL_HAT_DOWN)

typedef struct SDL_keysym {
	Uint8  scancode;   /* USB HID usage of the key */
	SDLKey sym;
	SDLMod mod;
	Uint16 unicode;
} SDL_keysym;

typedef struct { Uint8 type; Uint8 gain; Uint8 state; } SDL_ActiveEvent;
typedef struct { Uint8 type; Uint8 which; Uint8 state; SDL_keysym keysym; } SDL_KeyboardEvent;
typedef struct { Uint8 type; Uint8 which; Uint8 state; Uint16 x, y; Sint16 xrel, yrel; } SDL_MouseMotionEvent;
typedef struct { Uint8 type; Uint8 which; Uint8 button; Uint8 state; Uint16 x, y; } SDL_MouseButtonEvent;
typedef struct { Uint8 type; Uint8 which; Uint8 axis; Sint16 value; } SDL_JoyAxisEvent;
typedef struct { Uint8 type; Uint8 which; Uint8 hat; Uint8 value; } SDL_JoyHatEvent;
typedef struct { Uint8 type; Uint8 which; Uint8 button; Uint8 state; } SDL_JoyButtonEvent;
typedef struct { Uint8 type; int w, h; } SDL_ResizeEvent;
typedef struct { Uint8 type; } SDL_ExposeEvent;
typedef struct { Uint8 type; } SDL_QuitEvent;

typedef union SDL_Event {
	Uint8 type;
	SDL_ActiveEvent active;
	SDL_KeyboardEvent key;
	SDL_MouseMotionEvent motion;
	SDL_MouseButtonEvent button;
	SDL_JoyAxisEvent jaxis;
	SDL_JoyHatEvent jhat;
	SDL_JoyButtonEvent jbutton;
	SDL_ResizeEvent resize;
	SDL_ExposeEvent expose;
	SDL_QuitEvent quit;
} SDL_Event;

/* Returns 1 and fills *event if one was waiting, else 0 */
int SDL_PollEvent(SDL_Event * event);
/* Blocks until an event arrives. Returns 1. */
int SDL_WaitEvent(SDL_Event * event);
/* Queues an event. Safe from any thread. Returns 0, or -1 if the queue is full. */
int SDL_PushEvent(SDL_Event * event);

/* ---- Keyboard / mouse ------------------------------------------------ */

SDLMod SDL_GetModState(void);
const char * SDL_GetKeyName(SDLKey key);

#define SDL_GRAB_QUERY (-1)
#define SDL_GRAB_OFF    0
#define SDL_GRAB_ON     1
typedef int SDL_GrabMode;

Uint8 SDL_GetMouseState(int * x, int * y);
int SDL_ShowCursor(int toggle);
SDL_GrabMode SDL_WM_GrabInput(SDL_GrabMode mode);

/* ---- Joysticks ------------------------------------------------------- *
 * Eight, always present: 0-3 are the Wiimotes (with Nunchuk or Classic
 * Controller), 4-7 are GameCube pads. Button/axis/hat numbering is the one
 * the SDL Wii port used, so existing mapper files keep working:
 *   Wiimote  buttons: 0 A, 1 B, 2 1, 3 2, 4 -, 5 +, 6 Home, 7 Nunchuk Z,
 *            8 Nunchuk C, 9-19 Classic A B X Y L R ZL ZR - + Home
 *            axes: 0-1 left stick (Nunchuk or Classic), 2-3 Classic right
 *            stick, 4 Classic R trigger, 5 Classic L trigger,
 *            6-8 Wiimote pitch, roll, yaw
 *            hat: Classic D-pad, or the Wiimote D-pad held sideways
 *   GameCube buttons: 0 A, 1 B, 2 X, 3 Y, 4 Z, 5 R, 6 L, 7 Start
 *            axes: 0-1 stick, 2-3 C-stick, 4 L trigger, 5 R trigger
 *            hat: D-pad
 */
typedef struct SDL_Joystick SDL_Joystick;

int SDL_NumJoysticks(void);
const char * SDL_JoystickName(int index);
SDL_Joystick * SDL_JoystickOpen(int index);
void SDL_JoystickClose(SDL_Joystick * joystick);
int SDL_JoystickNumAxes(SDL_Joystick * joystick);
int SDL_JoystickNumButtons(SDL_Joystick * joystick);
int SDL_JoystickNumHats(SDL_Joystick * joystick);
Sint16 SDL_JoystickGetAxis(SDL_Joystick * joystick, int axis);
Uint8 SDL_JoystickGetButton(SDL_Joystick * joystick, int button);
Uint8 SDL_JoystickGetHat(SDL_Joystick * joystick, int hat);
/* Refreshes joystick state (and posts joystick events). See InputHal_Update(). */
void SDL_JoystickUpdate(void);
/* SDL_ENABLE / SDL_DISABLE / SDL_QUERY: whether joystick events are posted */
int SDL_JoystickEventState(int state);

/* ---- HAL entry points ------------------------------------------------ */

//! Sets up the input layer. The platform and keyboard/mouse drivers must
//! already be initialised (WiiInit()).
void InputHal_Init(void);

//! Scans the HAL pads, keyboard and mice and posts the resulting events.
//! Call once per emulation event pass (GFX_Events). Scanning is
//! rate-limited internally, so this is cheap to call often. It is the one
//! place that calls InputDriver::update() during emulation.
void InputHal_Update(void);

#endif
