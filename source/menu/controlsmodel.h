/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsmodel.h
 *
 * What the controller screens show and edit, behind one interface. The screens
 * only talk to ControlsModel; today it is backed by sample data so the UI can
 * be built and tried first. Connecting the real thing means writing another
 * ControlsModel (reading the input drivers and DOSBox's mapper) and handing it
 * to Controls_SetModel().
 ***************************************************************************/
#ifndef _CONTROLSMODEL_H_
#define _CONTROLSMODEL_H_

#include <string>
#include <vector>

/** Kinds of controller hardware. */
enum class PadType {
	NONE,
	WIIMOTE,
	WIIMOTE_NUNCHUK,
	CLASSIC,
	WIIU_PRO,
	GAMECUBE,
	GAMEPAD
};

/** Display name of a hardware type. */
const char * PadType_Name(PadType t);

/** Added to a stick's control id to address its click rather than its movement. */
enum { BIND_STICK_CLICK = 1 << 28 };

/** What a control can be set to do. */
enum class TARGET {
	NONE,		/**< Nothing */
	KEY,		/**< A keyboard key */
	JOYSTICK,	/**< A button on the DOS joystick */
	MOUSE,		/**< A mouse button or a pointer movement */
	HOTKEY,		/**< A DOSBox function (cycles, frameskip, screenshot, ...) */
	STICK_MODE	/**< What an analog stick does as a whole (joystick, arrow keys, mouse, off) */
};

/** The choices for a kind of target, in display order; a target's `code` indexes this list. */
const std::vector<std::string>& Catalog_Names(TARGET kind);

/** Finds a choice by name (-1 if there is none). */
int Catalog_Find(TARGET kind, const char * name);

/** A chosen target. */
struct BindingTarget {
	TARGET kind;
	int code;

	BindingTarget() : kind(TARGET::NONE), code(0) {}
	BindingTarget(TARGET k, int c) : kind(k), code(c) {}

	/** A short description for a list row: "Key: Ctrl", "Joystick button 1", "Not assigned". */
	std::string describe() const;

	bool operator==(const BindingTarget& o) const { return kind == o.kind && (kind == TARGET::NONE || code == o.code); }
	bool operator!=(const BindingTarget& o) const { return !(*this == o); }
};

/** The DOS-side device a controller stands in for. */
enum class Emulated {
	NONE,
	JOYSTICK,		/**< A DOS joystick (PlayerInfo::joystick says which) */
	KEYS_MOUSE		/**< The keyboard and mouse */
};

/** What the Controllers screen shows for one of the four player slots. */
struct PlayerInfo {
	bool connected;
	PadType type;
	std::string device;		/**< "Wiimote + Nunchuk" */
	int battery;			/**< Percent; -1 for wired or unknown */
	std::string role;		/**< What the player does in DOS, in words: "Joystick 1", "Keys + mouse" (for the hint line) */
	Emulated emulated;		/**< The same, for the icons */
	int joystick;			/**< Which DOS joystick (1 or 2) when emulated is JOYSTICK */
	std::string scheme;		/**< The control scheme in use */
};

/** Which shared input devices are present. */
struct DeviceStatus {
	bool keyboard;
	bool mouse;
	bool wiimoteIr;
	bool gamepadPresent;	/**< Wii U only: whether to show the GamePad line at all */
	bool gamepadTouch;
};

/** Everything the controller screens read and write. */
class ControlsModel
{
	public:
		virtual ~ControlsModel() {}

		virtual int playerCount() const = 0;
		virtual PlayerInfo player(int index) const = 0;
		virtual DeviceStatus devices() const = 0;

		virtual std::vector<std::string> schemes() const = 0;
		virtual int currentScheme() const = 0;
		virtual void setScheme(int index) = 0;

		/** What a control does. `binding` is a control id, plus BIND_STICK_CLICK for a stick's click. */
		virtual BindingTarget binding(PadType type, int binding) const = 0;
		virtual void setBinding(PadType type, int binding, const BindingTarget& target) = 0;
		/** Puts every control of a hardware type back to the current scheme's defaults. */
		virtual void resetBindings(PadType type) = 0;
};

/** The model the screens use (sample data until a real one is installed). */
ControlsModel& Controls();

/** Installs a model; null goes back to the sample data. The model must outlive its use. */
void Controls_SetModel(ControlsModel * model);

#endif
