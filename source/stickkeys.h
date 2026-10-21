/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * stickkeys.h
 *
 * An analog stick that presses the arrow keys. Each of the stick's four
 * directions is a digital source: it is pressed once the stick is pushed past
 * a threshold and let go when it falls back, and it presses Up / Right / Down /
 * Left. Which sticks do it is kept per player (one value per player, so it is a
 * plain [sdl] property that rides in the game's conf), and the decision of which
 * directions are held is here so the host tests can run it.
 * Nothing in this header depends on DOSBox, the HAL or libgui.
 ***************************************************************************/
#ifndef _STICKKEYS_H_
#define _STICKKEYS_H_

#include "stickmouse.h"	// STICK_LEFT / STICK_RIGHT

enum { STICKKEYS_PLAYERS = 4 };

/** The directions of a stick. A diagonal is two of them. */
enum { STICKDIR_UP = 1, STICKDIR_RIGHT = 2, STICKDIR_DOWN = 4, STICKDIR_LEFT = 8 };

/** How far a stick has to be pushed along an axis (0..1) to press that direction: the same engage point as the menu (InputController::STICK_PRESS_THRESHOLD). */
extern const float STICKKEYS_PRESS;
/** How far it may fall back before the direction is let go (the menu's release point), so a stick resting near the edge does not chatter. */
extern const float STICKKEYS_RELEASE;

/** Whether a player's stick (STICK_LEFT / STICK_RIGHT) is set to press the arrow keys. */
bool StickKeys_Get(int player, int stick);

/** Sets it, and tells the saver (below) so the setting reaches the config. */
void StickKeys_Set(int player, int stick, bool on);

/** The setting as the [sdl] stickkeysN property holds it: "off", "left", "right" or "both". */
const char * StickKeys_Value(int player);

/** Takes a property value (as the config has it) without saving. False, and the player set to off, for a value that is none of the four. */
bool StickKeys_SetValue(int player, const char * value);

/** Called with the player and the new property value whenever StickKeys_Set() changes something. The app installs one that writes the config; none by default. */
typedef void (*StickKeysSaver)(int player, const char * value);
void StickKeys_SetSaver(StickKeysSaver saver);

/** Whether any player has a stick set (so the poll can skip all of it otherwise). */
bool StickKeys_Any();

/** What a stick has pressed so far (a mask of STICKDIR_*), carried from step to step for the release point. */
struct StickKeysState
{
	unsigned held;
	StickKeysState() : held(0) {}
	void reset() { held = 0; }
};

/**
 * One step of a stick at (x, y), each -1..1 with y up. Each axis is judged on its own, so a stick pushed
 * into a corner holds two directions. Returns the directions held now; a direction already held stays
 * until its axis falls under STICKKEYS_RELEASE, one not held needs STICKKEYS_PRESS.
 */
unsigned StickKeys_Step(StickKeysState& state, float x, float y);

#endif
