/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * stickkeys.cpp
 ***************************************************************************/
#include <string.h>
#include "stickkeys.h"

const float STICKKEYS_PRESS = 0.39f;
const float STICKKEYS_RELEASE = 0.20f;

namespace {

bool on[STICKKEYS_PLAYERS][2];
StickKeysSaver saver = nullptr;

bool valid(int player, int stick) { return player >= 0 && player < STICKKEYS_PLAYERS && (stick == STICK_LEFT || stick == STICK_RIGHT); }

// one axis: the negative and positive direction it presses, given what was held
unsigned axis(float value, unsigned held, unsigned negative, unsigned positive)
{
	const float press = STICKKEYS_PRESS, release = STICKKEYS_RELEASE;
	unsigned result = 0;
	if(value >= (held & positive ? release : press))
		result |= positive;
	else if(value <= -(held & negative ? release : press))
		result |= negative;
	return result;
}

}

bool StickKeys_Get(int player, int stick)
{
	return valid(player, stick) && on[player][stick];
}

const char * StickKeys_Value(int player)
{
	if(player < 0 || player >= STICKKEYS_PLAYERS)
		return "off";
	const bool l = on[player][STICK_LEFT], r = on[player][STICK_RIGHT];
	return l && r ? "both" : (l ? "left" : (r ? "right" : "off"));
}

void StickKeys_Set(int player, int stick, bool value)
{
	if(!valid(player, stick) || on[player][stick] == value)
		return;
	on[player][stick] = value;
	if(saver)
		saver(player, StickKeys_Value(player));
}

bool StickKeys_SetValue(int player, const char * value)
{
	if(player < 0 || player >= STICKKEYS_PLAYERS)
		return false;
	on[player][STICK_LEFT] = on[player][STICK_RIGHT] = false;
	if(!value)
		return false;
	if(!strcmp(value, "off"))
		return true;
	if(!strcmp(value, "left") || !strcmp(value, "both"))
		on[player][STICK_LEFT] = true;
	if(!strcmp(value, "right") || !strcmp(value, "both"))
		on[player][STICK_RIGHT] = true;
	return on[player][STICK_LEFT] || on[player][STICK_RIGHT];
}

void StickKeys_SetSaver(StickKeysSaver s)
{
	saver = s;
}

bool StickKeys_Any()
{
	for(int p = 0; p < STICKKEYS_PLAYERS; p++)
		if(on[p][STICK_LEFT] || on[p][STICK_RIGHT])
			return true;
	return false;
}

unsigned StickKeys_Step(StickKeysState& state, float x, float y)
{
	// y is up, so up is the positive direction of that axis
	const unsigned now = axis(x, state.held, STICKDIR_LEFT, STICKDIR_RIGHT) | axis(y, state.held, STICKDIR_DOWN, STICKDIR_UP);
	state.held = now;
	return now;
}
