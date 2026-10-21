/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * stickmouse.h
 *
 * An analog stick that moves the DOS mouse. Which sticks do it is kept per
 * player (one value per player, so it is a plain [sdl] property that rides in
 * the game's conf), and the movement maths is here so the host tests can run it.
 * Nothing in this header depends on DOSBox, the HAL or libgui.
 ***************************************************************************/
#ifndef _STICKMOUSE_H_
#define _STICKMOUSE_H_

enum { STICKMOUSE_PLAYERS = 4 };
enum { STICK_LEFT = 0, STICK_RIGHT = 1 };

/** Whether a player's stick (STICK_LEFT / STICK_RIGHT) is set to move the mouse. Says nothing of whether the DOS joystick also uses it: see StickMouse_Active(). */
bool StickMouse_Get(int player, int stick);

/** Sets it, and tells the saver (below) so the setting reaches the config. */
void StickMouse_Set(int player, int stick, bool on);

/** The setting as the [sdl] stickmouseN property holds it: "off", "left", "right" or "both". */
const char * StickMouse_Value(int player);

/** Takes a property value (as the config has it) without saving. False, and the player set to off, for a value that is none of the four. */
bool StickMouse_SetValue(int player, const char * value);

/** Called with the player and the new property value whenever StickMouse_Set() changes something. The app installs one that writes the config; none by default. */
typedef void (*StickMouseSaver)(int player, const char * value);
void StickMouse_SetSaver(StickMouseSaver saver);

/** Whether any player has a stick set (so the poll can skip all of it otherwise). */
bool StickMouse_Any();

/**
 * Whether the stick moves the mouse now: it is set to, and the DOS joystick of the type in effect (a
 * joysticktype value) does not also use it. Left stick: the first two axes; right stick: the last two (4 axes).
 */
bool StickMouse_Active(int player, int stick, const char * joystickType);

/** Movement smaller than a whole mouse unit, carried to the next step. */
struct StickMouseMotion
{
	float remX, remY;
	StickMouseMotion() : remX(0), remY(0) {}
	void reset() { remX = remY = 0; }
};

/**
 * One step of a stick held at (x, y), each -1..1 with y up, for `seconds` since the last step. Inside the
 * deadzone nothing moves; beyond it the speed grows with the square of the push, so small pushes place the
 * cursor and a full push crosses `width` units in about a second. dx / dy are mouse units, y down.
 */
void StickMouse_Step(StickMouseMotion& motion, float x, float y, float seconds, int width, int& dx, int& dy);

#endif
