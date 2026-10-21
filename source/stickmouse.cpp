/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * stickmouse.cpp
 ***************************************************************************/
#include <math.h>
#include <string.h>
#include <string>
#include "stickmouse.h"
#include "menu/controlsmodel.h"

namespace {

bool on[STICKMOUSE_PLAYERS][2];
StickMouseSaver saver = nullptr;

const float DEADZONE = 0.2f;		// the stick's push that is not yet a push
const float MAX_STEP_SECONDS = 0.05f;	// a hitch is not a jump

bool valid(int player, int stick) { return player >= 0 && player < STICKMOUSE_PLAYERS && (stick == STICK_LEFT || stick == STICK_RIGHT); }

}

bool StickMouse_Get(int player, int stick)
{
	return valid(player, stick) && on[player][stick];
}

const char * StickMouse_Value(int player)
{
	if(player < 0 || player >= STICKMOUSE_PLAYERS)
		return "off";
	const bool l = on[player][STICK_LEFT], r = on[player][STICK_RIGHT];
	return l && r ? "both" : (l ? "left" : (r ? "right" : "off"));
}

void StickMouse_Set(int player, int stick, bool value)
{
	if(!valid(player, stick) || on[player][stick] == value)
		return;
	on[player][stick] = value;
	if(saver)
		saver(player, StickMouse_Value(player));
}

bool StickMouse_SetValue(int player, const char * value)
{
	if(player < 0 || player >= STICKMOUSE_PLAYERS)
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

void StickMouse_SetSaver(StickMouseSaver s)
{
	saver = s;
}

bool StickMouse_Any()
{
	for(int p = 0; p < STICKMOUSE_PLAYERS; p++)
		if(on[p][STICK_LEFT] || on[p][STICK_RIGHT])
			return true;
	return false;
}

bool StickMouse_Active(int player, int stick, const char * joystickType)
{
	if(!StickMouse_Get(player, stick))
		return false;
	const int index = Emulated_IndexOfType(joystickType ? joystickType : "");
	// a type the table does not know: the stick is left to the joystick rather than doing both
	if(index < 0)
		return false;
	return !Emulated_StickUsed(Emulated_Modes()[(size_t)index], player, stick);
}

void StickMouse_Step(StickMouseMotion& motion, float x, float y, float seconds, int width, int& dx, int& dy)
{
	dx = dy = 0;

	const float magnitude = sqrtf(x * x + y * y);
	if(magnitude <= DEADZONE)
	{
		motion.reset();
		return;
	}

	if(seconds > MAX_STEP_SECONDS)
		seconds = MAX_STEP_SECONDS;
	if(seconds < 0)
		seconds = 0;

	float push = (magnitude - DEADZONE) / (1.0f - DEADZONE);
	if(push > 1.0f)
		push = 1.0f;
	const float speed = (float)width * push * push;	// units per second along the stick's direction

	const float fx = (x / magnitude) * speed * seconds + motion.remX;
	const float fy = (-y / magnitude) * speed * seconds + motion.remY;
	dx = (int)fx;
	dy = (int)fy;
	motion.remX = fx - (float)dx;
	motion.remY = fy - (float)dy;
}
