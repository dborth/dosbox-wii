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

#include <stdint.h>
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
	MOUSE,		/**< A mouse button */
	HOTKEY		/**< A DOSBox function (cycles, frameskip, swap disk image, ...) */
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

/** What the Controllers screen shows for one of the four player slots. A slot is a HAL channel: player n is controller[n]. */
struct PlayerInfo {
	bool connected;
	PadType type;
	std::string device;		/**< "Wiimote + Nunchuk" */
	int battery;			/**< Percent; -1 for wired or unknown */
	std::string role;		/**< What the player does in DOS, in words: "Joystick 1", "Keys + mouse" (for the hint line) */
	Emulated emulated;		/**< The same, for the icons */
	int joystick;			/**< Which DOS joystick (1 or 2) when emulated is JOYSTICK */
	std::string preset;		/**< The preset the player's buttons started from */
};

/** Which shared input devices are present. */
struct DeviceStatus {
	bool keyboard;
	bool mouse;
	bool wiimoteIr;
	bool gamepadPresent;	/**< Wii U only: whether to show the GamePad line at all */
	bool gamepadTouch;
};

/** A built-in set of targets to start a player's buttons from. */
struct PresetInfo {
	std::string name;			/**< "Arrow keys" */
	std::string description;	/**< One line: what it is for */
	bool needsJoystick;			/**< Uses joystick buttons, so it means nothing with no joystick */
};

/**
 * What the program on the DOS side sees. Keyboard and mouse are always there (DOSBox here cannot turn them
 * off), so the only choice is the gamepad port: none, one joystick, or two.
 */
struct EmulatedMode {
	std::string name;			/**< "Joystick" */
	std::string description;
	int joysticks;				/**< How many DOS joysticks exist: players 1..n each get one */
	int buttons;				/**< Buttons on each of them */
	int axes;					/**< Axes on each: 2 (one stick) or 4 (two sticks, or a stick and a throttle) */
};

/** A saved setup: a name and, optionally, the program it loads for. */
struct SetupInfo {
	std::string name;
	std::string program;		/**< "KEEN4E.EXE", or empty */
};

/** Everything the controller screens read and write. */
class ControlsModel
{
	public:
		virtual ~ControlsModel() {}

		virtual int playerCount() const = 0;
		virtual PlayerInfo player(int index) const = 0;
		virtual DeviceStatus devices() const = 0;

		/** The buttons currently held on a player's controllers (INPUT_* bits), for lighting the control on the drawing. */
		virtual uint32_t held(int player) const = 0;

		// ---- the emulated controller (what DOS sees)
		virtual std::vector<EmulatedMode> emulatedModes() const = 0;
		virtual int currentEmulated() const = 0;
		virtual void setEmulated(int index) = 0;

		/** Buttons on the DOS joystick a player drives; 0 when that player has none. */
		int joystickButtons(int player) const;
		/** Axes on it: 4 when both sticks count, 2 for the left stick only, 0 with no joystick. */
		int joystickAxes(int player) const;
		/** Whether a target does anything for a player under the current emulated mode (a joystick button the joystick lacks does not). */
		bool usable(int player, const BindingTarget& target) const;

		// ---- presets
		virtual std::vector<PresetInfo> presets() const = 0;
		virtual int currentPreset() const = 0;
		/** Starts every player's buttons over from a preset. */
		virtual void applyPreset(int index) = 0;

		// ---- buttons. A binding belongs to a player slot, as in the mapper's per-pad groups.
		/** Whether a control can be set at all (it has a source in the mapper). `control` is a control id, plus BIND_STICK_CLICK for a stick's click. */
		virtual bool supported(int control) const = 0;
		virtual BindingTarget binding(int player, int control) const = 0;
		virtual void setBinding(int player, int control, const BindingTarget& target) = 0;
		/** What the current preset gives a control (the chooser's Default). */
		virtual BindingTarget defaultBinding(int control) const = 0;
		/** Puts every control of a player back to the current preset's defaults. */
		virtual void resetBindings(int player) = 0;
		/** What a control does, in short words; when the target would do nothing it says why instead ("Joystick off", "No button 3"). */
		std::string describe(int player, int control) const;

		// ---- setups: a named, saved configuration, optionally tied to a program
		virtual std::vector<SetupInfo> setups() const = 0;
		/** Saves the current configuration under a name (replacing one of the same name). */
		virtual bool saveSetup(const std::string& name, const std::string& program) = 0;
		virtual bool loadSetup(int index) = 0;
		/** The DOS program running now ("" at the prompt or when unknown). */
		virtual std::string runningProgram() const = 0;
};

/** The model the screens use (sample data until a real one is installed). */
ControlsModel& Controls();

/** Installs a model; null goes back to the sample data. The model must outlive its use. */
void Controls_SetModel(ControlsModel * model);

/** Sets what the sample model reports as held (for tests and previews; does nothing once another model is installed). */
void SampleControls_SetHeld(uint32_t bits);

#endif
