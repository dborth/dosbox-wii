/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controllerprofile.cpp
 *
 * One builder makes every profile. A kind of hardware is described by a Spec:
 * which drawing, how big, which controls go left of it, right of it and in
 * the groups underneath, and which tables to look names up in. The names are
 * layered: the Wii U Pro's are the standard, and a kind lists only what it
 * calls differently (a GameCube's Z, a Nunchuk's C), so a Wiimote with a
 * Nunchuk is just {Nunchuk, Wiimote, standard}.
 ***************************************************************************/
#include "controllerprofile.h"

namespace {

// A stick's click is listed under the stick's id plus this, as the model addresses it
const int CLICK_L = INPUT_THUMB_L | BIND_STICK_CLICK;
const int CLICK_R = INPUT_THUMB_R | BIND_STICK_CLICK;

/** What one control is called and drawn as on a kind of hardware. */
struct Label {
	int key;				/**< The id the lists below use: a control, plus BIND_STICK_CLICK for a stick's click. */
	int bind;				/**< The id the model stores its target under when that is not the key; 0 for the key itself. */
	GLYPH glyph;
	const char * letter;
	const char * name;
	int stick;				/**< 1 or 2 for a stick's movement (see ProfileRow::stick). */
	const char * fixed;		/**< Text shown instead of a target, for a control that cannot change. */
};

struct LabelSet {
	const Label * labels;
	size_t count;
};
#define LABELS(table) { table, sizeof(table) / sizeof(table[0]) }

// The Wii U Pro and the GamePad, and what every other kind falls back to
const Label kStandard[] = {
	{ INPUT_TRIGGER_L,	0, GLYPH::BUMPER, "L",	"Left Bumper",	0, nullptr },
	{ INPUT_TRIGGER_ZL,	0, GLYPH::BUMPER, "ZL",	"Left Trigger",	0, nullptr },
	{ INPUT_TRIGGER_R,	0, GLYPH::BUMPER, "R",	"Right Bumper",	0, nullptr },
	{ INPUT_TRIGGER_ZR,	0, GLYPH::BUMPER, "ZR",	"Right Trigger",	0, nullptr },
	{ INPUT_BTN_MINUS,	0, GLYPH::MINUS,  "",	"Select",		0, nullptr },
	{ INPUT_BTN_PLUS,	0, GLYPH::PLUS,   "",	"Start",		0, nullptr },
	{ INPUT_BTN_HOME,	0, GLYPH::HOME,   "",	"Home",			0, "Opens this menu" },
	{ INPUT_THUMB_L,	0, GLYPH::STICK,  "L",	"L Stick",		1, nullptr },
	{ INPUT_THUMB_R,	0, GLYPH::STICK,  "R",	"R Stick",		2, nullptr },
	// a stick's click is a round button, so it does not look like the stick's movement
	{ CLICK_L,			0, GLYPH::BUTTON,  "L3",	"L Click",		0, nullptr },
	{ CLICK_R,			0, GLYPH::BUTTON,  "R3",	"R Click",		0, nullptr },
	{ INPUT_BTN_UP,		0, GLYPH::UP,     "",	"Up",			0, nullptr },
	{ INPUT_BTN_DOWN,	0, GLYPH::DOWN,   "",	"Down",			0, nullptr },
	{ INPUT_BTN_LEFT,	0, GLYPH::LEFT,   "",	"Left",			0, nullptr },
	{ INPUT_BTN_RIGHT,	0, GLYPH::RIGHT,  "",	"Right",		0, nullptr },
	{ INPUT_BTN_B,		0, GLYPH::BUTTON,  "B",	"B Button",		0, nullptr },
	{ INPUT_BTN_A,		0, GLYPH::BUTTON,  "A",	"A Button",		0, nullptr },
	{ INPUT_BTN_Y,		0, GLYPH::BUTTON,  "Y",	"Y Button",		0, nullptr },
	{ INPUT_BTN_X,		0, GLYPH::BUTTON,  "X",	"X Button",		0, nullptr },
};

// Classic: L and R are the analog triggers, ZL and ZR the small buttons beside them
const Label kClassic[] = {
	{ INPUT_TRIGGER_L,	0, GLYPH::BUMPER, "L",	"L Trigger",	0, nullptr },
	{ INPUT_TRIGGER_R,	0, GLYPH::BUMPER, "R",	"R Trigger",	0, nullptr },
	{ INPUT_TRIGGER_ZL,	0, GLYPH::BUMPER, "ZL",	"ZL Button",	0, nullptr },
	{ INPUT_TRIGGER_ZR,	0, GLYPH::BUMPER, "ZR",	"ZR Button",	0, nullptr },
	{ INPUT_BTN_MINUS,	0, GLYPH::MINUS,  "",	"Minus",		0, nullptr },
	{ INPUT_BTN_PLUS,	0, GLYPH::PLUS,   "",	"Plus",		0, nullptr },
};

// GameCube: Start arrives as Plus and Z as ZR; there is no Home, so the menu is the L + R + Start combo
const Label kGameCube[] = {
	{ INPUT_TRIGGER_L,	0, GLYPH::BUMPER, "L",	"L Trigger",	0, nullptr },
	{ INPUT_TRIGGER_R,	0, GLYPH::BUMPER, "R",	"R Trigger",	0, nullptr },
	{ INPUT_TRIGGER_ZR,	0, GLYPH::BUMPER, "Z",	"Z Button",	0, nullptr },
	{ INPUT_BTN_PLUS,	0, GLYPH::BUTTON, "S",	"Start",		0, nullptr },
	{ INPUT_BTN_HOME,	0, GLYPH::HOME,   "",	"Menu",		0, "Hold L + R + Start" },
	{ INPUT_THUMB_L,	0, GLYPH::STICK,  "L",	"Main Stick",	1, nullptr },
	{ INPUT_THUMB_R,	0, GLYPH::STICK,  "C",	"C-Stick",	2, nullptr },
};

// Wiimote: the 1 and 2 buttons share the engine's X and Y sources (a Wiimote has no X or Y, so there is no clash)
const Label kWiimote[] = {
	{ INPUT_BTN_B,		0,				GLYPH::BUTTON, "B", "B Trigger",	0, nullptr },
	{ INPUT_BTN_1,		INPUT_BTN_X,	GLYPH::BUTTON, "1", "1 Button",		0, nullptr },
	{ INPUT_BTN_2,		INPUT_BTN_Y,	GLYPH::BUTTON, "2", "2 Button",		0, nullptr },
	{ INPUT_BTN_MINUS,	0,				GLYPH::MINUS,  "",	"Minus",			0, nullptr },
	{ INPUT_BTN_PLUS,	0,				GLYPH::PLUS,   "",	"Plus",			0, nullptr },
};

// Nunchuk: C arrives as L and Z as ZL
const Label kNunchuk[] = {
	{ INPUT_TRIGGER_L,	0, GLYPH::BUTTON, "C",	"C Button",		0, nullptr },
	{ INPUT_TRIGGER_ZL,	0, GLYPH::BUTTON, "Z",	"Z Button",		0, nullptr },
	{ INPUT_THUMB_L,	0, GLYPH::STICK,  "L",	"Nunchuk Stick",	1, nullptr },
};

const std::vector<LabelSet> kStandardNames = { LABELS(kStandard) };

/** A titled column of controls, by key. */
struct GroupSpec {
	const char * title;
	std::vector<int> keys;
};

/** Everything that makes one kind of hardware's profile: the rest is the shared builder. */
struct Spec {
	const ControllerLayout * layout;
	float scale;
	std::vector<LabelSet> names;		// searched in order
	std::vector<int> left, right;
	std::vector<GroupSpec> groups;
};

// ---- pieces shared by every kind

std::vector<int> dpad()
{
	return { INPUT_BTN_UP, INPUT_BTN_DOWN, INPUT_BTN_LEFT, INPUT_BTN_RIGHT };
}

std::vector<int> faceButtons()
{
	return { INPUT_BTN_B, INPUT_BTN_A, INPUT_BTN_Y, INPUT_BTN_X };
}

// Both sticks' movement, and their clicks only on hardware that has them and a driver that reports them
std::vector<int> sticks(bool clicks)
{
	std::vector<int> k;
	k.push_back(INPUT_THUMB_L);
	if(clicks)
		k.push_back(CLICK_L);
	k.push_back(INPUT_THUMB_R);
	if(clicks)
		k.push_back(CLICK_R);
	return k;
}

// Every pad with two sticks (or a GameCube's stick and C-stick): shoulders and system buttons beside the
// drawing; sticks, D-pad and face buttons underneath
Spec padSpec(const ControllerLayout& layout, float scale, const std::vector<LabelSet>& names,
	const std::vector<int>& left, const std::vector<int>& right, bool clicks)
{
	Spec s;
	s.layout = &layout;
	s.scale = scale;
	s.names = names;
	s.left = left;
	s.right = right;
	s.groups.push_back({ "STICKS", sticks(clicks) });
	s.groups.push_back({ "D-PAD", dpad() });
	s.groups.push_back({ "FACE BUTTONS", faceButtons() });
	return s;
}

// The shoulder and system buttons the Pro-style pads share
const std::vector<int> kProLeft = { INPUT_TRIGGER_L, INPUT_TRIGGER_ZL, INPUT_BTN_MINUS };
const std::vector<int> kProRight = { INPUT_TRIGGER_R, INPUT_TRIGGER_ZR, INPUT_BTN_PLUS, INPUT_BTN_HOME };

// ---- one Spec per kind of hardware

Spec proSpec(const ControllerLayout& layout, float scale, bool clicks)
{
	return padSpec(layout, scale, kStandardNames, kProLeft, kProRight, clicks);
}

Spec classicSpec()
{
	return padSpec(ClassicControllerLayout(), 0.4f, { LABELS(kClassic), LABELS(kStandard) }, kProLeft, kProRight, false);
}

Spec gameCubeSpec()
{
	return padSpec(GameCubeControllerLayout(), 0.4f, { LABELS(kGameCube), LABELS(kStandard) },
		{ INPUT_TRIGGER_L }, { INPUT_TRIGGER_R, INPUT_TRIGGER_ZR, INPUT_BTN_PLUS, INPUT_BTN_HOME }, false);
}

// A Wiimote held upright is tall and narrow: with the rows underneath it would be a few pixels wide, so every
// control is listed beside it instead, top to bottom in the order they sit on the drawing
Spec wiimoteUprightSpec()
{
	Spec s;
	s.layout = &WiimoteControllerLayout();
	s.scale = 0.55f;
	s.names = { LABELS(kWiimote), LABELS(kStandard) };
	s.left = { INPUT_BTN_UP, INPUT_BTN_DOWN, INPUT_BTN_LEFT, INPUT_BTN_RIGHT, INPUT_BTN_MINUS, INPUT_BTN_1 };
	s.right = { INPUT_BTN_B, INPUT_BTN_A, INPUT_BTN_PLUS, INPUT_BTN_HOME, INPUT_BTN_2 };
	return s;
}

// Held sideways it is wide: the usual arrangement works. The directions are the ones the player holds it by.
Spec wiimoteSidewaysSpec()
{
	Spec s;
	s.layout = &WiimoteSidewaysControllerLayout();
	s.scale = 0.48f;
	s.names = { LABELS(kWiimote), LABELS(kStandard) };
	s.left = { INPUT_BTN_MINUS };
	s.right = { INPUT_BTN_PLUS, INPUT_BTN_HOME };
	s.groups.push_back({ "D-PAD", dpad() });
	s.groups.push_back({ "BUTTONS", { INPUT_BTN_B, INPUT_BTN_A, INPUT_BTN_1, INPUT_BTN_2 } });
	return s;
}

Spec nunchukSpec()
{
	Spec s;
	s.layout = &WiimoteNunchukControllerLayout();
	s.scale = 0.32f;
	s.names = { LABELS(kNunchuk), LABELS(kWiimote), LABELS(kStandard) };
	s.left = { INPUT_TRIGGER_L, INPUT_TRIGGER_ZL };
	s.right = { INPUT_BTN_MINUS, INPUT_BTN_PLUS, INPUT_BTN_HOME };
	s.groups.push_back({ "STICK", { INPUT_THUMB_L } });
	s.groups.push_back({ "D-PAD", dpad() });
	s.groups.push_back({ "BUTTONS", { INPUT_BTN_B, INPUT_BTN_A, INPUT_BTN_1, INPUT_BTN_2 } });
	return s;
}

// ---- the builder

const Label * findLabel(const Spec& spec, int key)
{
	for(size_t t = 0; t < spec.names.size(); t++)
		for(size_t i = 0; i < spec.names[t].count; i++)
			if(spec.names[t].labels[i].key == key)
				return &spec.names[t].labels[i];
	return nullptr;
}

ProfileRow makeRow(const Spec& spec, int key)
{
	const Label * label = findLabel(spec, key);
	ProfileRow r;
	r.binding = label && label->bind ? label->bind : key;
	// the picture highlights the control itself (a click lights its stick); a control the drawing lacks lights nothing
	r.control = key & ~BIND_STICK_CLICK;
	if(!spec.layout->find(r.control))
		r.control = 0;
	r.glyph = label ? label->glyph : GLYPH::BUTTON;
	r.letter = label ? label->letter : "";
	r.name = label ? label->name : "Control";
	r.stick = label ? label->stick : 0;
	r.fixed = label ? label->fixed : nullptr;
	return r;
}

void addRows(std::vector<ProfileRow>& out, const Spec& spec, const std::vector<int>& keys)
{
	for(size_t i = 0; i < keys.size(); i++)
		out.push_back(makeRow(spec, keys[i]));
}

ControllerProfile build(const char * name, const Spec& spec)
{
	ControllerProfile p;
	p.name = name;
	p.layout = spec.layout;
	p.scale = spec.scale;
	addRows(p.left, spec, spec.left);
	addRows(p.right, spec, spec.right);
	for(size_t g = 0; g < spec.groups.size(); g++)
	{
		ProfileGroup group;
		group.title = spec.groups[g].title;
		addRows(group.rows, spec, spec.groups[g].keys);
		p.groups.push_back(group);
	}
	return p;
}

} // namespace

ControllerProfile Controllers_Profile(PadType type, const ProfileOptions& options)
{
	const char * name = PadType_Name(type);
	switch(type)
	{
		case PadType::WIIMOTE:
			return build(name, options.sideways ? wiimoteSidewaysSpec() : wiimoteUprightSpec());
		case PadType::WIIMOTE_NUNCHUK:
			return build(name, nunchukSpec());
		case PadType::CLASSIC:
			return build(name, classicSpec());
		case PadType::GAMECUBE:
			return build(name, gameCubeSpec());
		case PadType::GAMEPAD:
			return build(name, proSpec(GamePadControllerLayout(), 0.42f, options.stickClicks));
		case PadType::WIIU_PRO:
		case PadType::NONE:
		default:
			return build(type == PadType::NONE ? "Controller" : name, proSpec(WiiUProControllerLayout(), 0.4f, options.stickClicks));
	}
}

ControllerProfile Controllers_Profile(const PlayerInfo& player, int shown)
{
	ProfileOptions options;
	options.sideways = player.sideways;
	options.stickClicks = player.stickClicks;
	const PadType type = shown >= 0 && (size_t)shown < player.devices.size() ? player.devices[(size_t)shown] : player.type;
	return Controllers_Profile(type, options);
}
