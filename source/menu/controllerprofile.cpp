/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controllerprofile.cpp
 ***************************************************************************/
#include "controllerprofile.h"

static ProfileRow row(int control, GLYPH glyph, const char * letter, const char * name)
{
	ProfileRow r;
	r.binding = control;
	r.control = control;
	r.glyph = glyph;
	r.letter = letter;
	r.name = name;
	r.stickMode = false;
	r.fixed = nullptr;
	return r;
}

static ProfileRow stickRow(int control, const char * letter, const char * name, bool click)
{
	ProfileRow r = row(control, GLYPH::STICK, letter, name);
	r.binding = click ? (control | BIND_STICK_CLICK) : control;
	r.stickMode = !click;
	return r;
}

static ProfileRow fixedRow(int control, GLYPH glyph, const char * name, const char * text)
{
	ProfileRow r = row(control, glyph, "", name);
	r.fixed = text;
	return r;
}

// A Pro-style pad: Switch Pro, Wii U Pro and, until it has a drawing of its own, the Classic Controller.
static ControllerProfile buildPro()
{
	ControllerProfile p;
	p.name = "Pro Controller";
	p.layout = &GuiProControllerLayout();
	p.scale = 0.6f;

	p.left.push_back(row(INPUT_TRIGGER_L, GLYPH::BUMPER, "L", "Left Bumper"));
	p.left.push_back(row(INPUT_TRIGGER_ZL, GLYPH::BUMPER, "ZL", "Left Trigger"));
	p.left.push_back(row(INPUT_BTN_MINUS, GLYPH::MINUS, "", "Select"));
	p.left.push_back(row(GUI_CTL_CAPTURE, GLYPH::CAPTURE, "", "Capture"));

	p.right.push_back(row(INPUT_TRIGGER_R, GLYPH::BUMPER, "R", "Right Bumper"));
	p.right.push_back(row(INPUT_TRIGGER_ZR, GLYPH::BUMPER, "ZR", "Right Trigger"));
	p.right.push_back(row(INPUT_BTN_PLUS, GLYPH::PLUS, "", "Start"));
	p.right.push_back(fixedRow(INPUT_BTN_HOME, GLYPH::HOME, "Home", "Opens this menu"));

	ProfileGroup sticks;
	sticks.title = "STICKS";
	sticks.rows.push_back(stickRow(INPUT_THUMB_L, "L", "L Stick", false));
	sticks.rows.push_back(stickRow(INPUT_THUMB_L, "L", "L Click", true));
	sticks.rows.push_back(stickRow(INPUT_THUMB_R, "R", "R Stick", false));
	sticks.rows.push_back(stickRow(INPUT_THUMB_R, "R", "R Click", true));
	p.groups.push_back(sticks);

	ProfileGroup dpad;
	dpad.title = "D-PAD";
	dpad.rows.push_back(row(INPUT_BTN_UP, GLYPH::UP, "", "Up"));
	dpad.rows.push_back(row(INPUT_BTN_DOWN, GLYPH::DOWN, "", "Down"));
	dpad.rows.push_back(row(INPUT_BTN_LEFT, GLYPH::LEFT, "", "Left"));
	dpad.rows.push_back(row(INPUT_BTN_RIGHT, GLYPH::RIGHT, "", "Right"));
	p.groups.push_back(dpad);

	ProfileGroup face;
	face.title = "FACE BUTTONS";
	face.rows.push_back(row(INPUT_BTN_B, GLYPH::BUTTON, "B", "B Button"));
	face.rows.push_back(row(INPUT_BTN_A, GLYPH::BUTTON, "A", "A Button"));
	face.rows.push_back(row(INPUT_BTN_Y, GLYPH::BUTTON, "Y", "Y Button"));
	face.rows.push_back(row(INPUT_BTN_X, GLYPH::BUTTON, "X", "X Button"));
	p.groups.push_back(face);
	return p;
}

const ControllerProfile& Controllers_Profile(PadType)
{
	static const ControllerProfile pro = buildPro();
	return pro;
}
