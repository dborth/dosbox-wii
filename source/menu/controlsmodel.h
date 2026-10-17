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

struct InputPadData;

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

/** The kind of controller a channel's pad data describes (NONE when nothing is connected). */
PadType PadType_FromPad(const InputPadData& pad);
/**
 * Every device on a channel, one entry per controller the player could be holding, the one PadType_FromPad names
 * first. A Wiimote with a Nunchuk or Classic on it is one device (the extension), not two; a GameCube pad or the
 * GamePad sharing the channel is another. Empty when nothing is connected.
 */
std::vector<PadType> PadDevices_FromPad(const InputPadData& pad);
/** Battery percent for the pad of that kind, or -1 when it has none to report (GameCube, nothing connected). */
int PadBattery(const InputPadData& pad, PadType type);

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
	PadType type;			/**< The primary device: the first of `devices` */
	std::vector<PadType> devices;	/**< Every device on the channel, primary first. The setup screen draws a rail of them when there are two or more; empty when `connected` is false */
	std::string device;		/**< "Wiimote + Nunchuk" */
	int battery;			/**< Percent; -1 for wired or unknown */
	std::string role;		/**< What the player does in DOS, in words: "Joystick 1", "Keys + mouse" (for the hint line) */
	Emulated emulated;		/**< The same, for the icons */
	int joystick;			/**< Which DOS joystick (1 or 2) when emulated is JOYSTICK */
	std::string preset;		/**< The preset the player's buttons started from (each player has their own) */
	bool sideways = false;		/**< A lone Wiimote held sideways: the setup screen draws it that way (the HAL's InputController::isSideways()) */
	bool stickClicks = false;	/**< The driver reports L3 / R3 for this pad. The Wii U drivers do for the Pro and the GamePad, the Wii one never does; false leaves the click rows off */
};

/** Which shared input devices are present. */
struct DeviceStatus {
	bool keyboard = false;
	bool mouse = false;
	bool wiimoteIr = false;
	bool gamepadPresent = false;	/**< Wii U only: whether to show the GamePad line at all */
	bool gamepadTouch = false;
	int gamepadBattery = -1;		/**< The GamePad's charge in percent; -1 for none to show */
};

/** One player slot as the console reports it, before the model adds what DOS does with it. */
struct PlayerHardware {
	bool connected = false;
	PadType type = PadType::NONE;
	std::vector<PadType> devices;	/**< Every device on the channel, primary (`type`) first; may be left empty when there is just the one */
	int battery = -1;			/**< Percent; -1 for none to show (the primary device's) */
	bool sideways = false;		/**< A lone Wiimote held sideways */
	bool stickClicks = false;	/**< The driver reports L3 / R3 */
};

/** A key or mouse button the user just pressed while the chooser was listening. */
struct CaptureEvent {
	TARGET kind;	/**< KEY or MOUSE */
	int code;		/**< Index in that kind's catalog, or -1 when the press has no target (a key the mapper cannot bind) */
};

/**
 * Everything the model reads from the console, and nothing else: the one seam between the controller screens and
 * the HAL. The app installs the HAL's (controlshal.cpp); tests and previews use the sample one.
 */
class ControlsHardware
{
	public:
		virtual ~ControlsHardware() {}
		virtual PlayerHardware player(int index) const = 0;
		virtual DeviceStatus devices() const = 0;
		/** The buttons held on a player's channel as DOS would get them (INPUT_* bits, a sideways Wiimote's D-pad already turned). */
		virtual uint32_t held(int index) const = 0;
		/**
		 * Starts listening for a key or mouse button press on the USB keyboard and mouse, dropping anything pressed or
		 * pending before. Does nothing by default (hardware with neither device).
		 */
		virtual void beginCapture() {}
		/** The next press since beginCapture(): a key going down, or a mouse button that was not down a moment ago. False when there is none. */
		virtual bool pollCapture(CaptureEvent& event) { return false; }
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
 *
 * Each mode is one joysticktype of the mapper. Players are physical sticks, and the type decides which of them
 * drives a DOS joystick: 4axis_2 hands the one joystick to player 2, not player 1. So a mode says which player
 * has the first joystick (firstPlayer) and a player's DOS joystick is player - firstPlayer.
 */
struct EmulatedMode {
	std::string name;			/**< "Joystick" */
	std::string description;
	int joysticks;				/**< How many DOS joysticks exist: players firstPlayer.. each get one */
	int buttons;				/**< Buttons on each of them */
	int axes;					/**< Axes on each: 2 (one stick) or 4 (two sticks, or a stick and a throttle) */
	std::string type;			/**< The joystick.joysticktype value: 4axis, 2axis, none, 4axis_2, fcs, ch */
	int firstPlayer = 0;		/**< The player (0-based) that has the first DOS joystick */
	bool hat = false;			/**< The joystick has a hat (fcs, ch): the D-pad can drive it */
	bool advanced = false;		/**< Not offered in the Emulating list unless it is the one in effect (set from Settings) */
};

/** A saved setup: a name and, optionally, the program it loads for. */
struct SetupInfo {
	std::string name;
	std::string program;		/**< "KEEN4E.EXE", or empty */
	std::string emulated;		/**< What DOS sees in it ("Joystick"), for the list */
	std::string preset;			/**< The preset it started from ("Mixed" when the players started from different ones) */
};

/**
 * A name for a setup as it will be written to a file (name.conf, name.map): surrounding spaces and characters a file
 * name cannot hold are removed, and it is cut to 24 characters. Empty when nothing usable is left.
 */
std::string Setup_CleanName(const std::string& typed);
/** Setup names are file names, and FAT ignores case: whether two names are the same setup. */
bool Setup_SameName(const std::string& a, const std::string& b);
/** A program name as typed ("c:\\games\\keen4e.exe"): the file name alone, upper case. Empty when there is none. */
std::string Setup_CleanProgram(const std::string& typed);
/** The name offered when saving: the running program without its extension ("KEEN4E"), or "My setup". */
std::string Setup_SuggestedName(const std::string& program);

/**
 * A player slot as the screens show it: the console's hardware plus what DOS does with it under an emulated mode
 * (joystick n is player n while n < mode.joysticks; everyone else is keys + mouse). Every model uses this, so the
 * role wording is in one place. `preset` is the name shown for the player; the role and preset are empty when
 * nothing is connected.
 */
PlayerInfo Controls_ComposePlayer(const PlayerHardware& hardware, int index, const EmulatedMode& mode, const std::string& preset);

/** The catalog index of the key a USB HID usage (keyboard page) stands for, or -1. Left and right Ctrl, Shift and Alt are one key each. */
int Catalog_KeyFromHid(unsigned hidUsage);

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

		/** The player's DOS joystick, 0-based (the n of the mapper's jbutton_n_*), or -1 when the player has none. */
		int joystickSlot(int player) const;
		/** Buttons on the DOS joystick a player drives; 0 when that player has none. */
		int joystickButtons(int player) const;
		/** Axes on it: 4 when both sticks count, 2 for the left stick only, 0 with no joystick. */
		int joystickAxes(int player) const;
		/** Whether a target does anything for a player under the current emulated mode (a joystick button the joystick lacks does not). */
		bool usable(int player, const BindingTarget& target) const;

		// ---- presets
		virtual std::vector<PresetInfo> presets() const = 0;
		/** The preset a player's buttons started from. Each player has their own. */
		virtual int currentPreset(int player) const = 0;
		/** Starts one player's buttons over from a preset; no other player changes. */
		virtual void applyPreset(int player, int index) = 0;
		/** What a preset gives a control (for showing a preset before choosing it). */
		virtual BindingTarget presetBinding(int preset, int control) const = 0;
		/** Whether any of a player's buttons differs from their preset's (applying a preset would throw those changes away). */
		virtual bool modified(int player) const = 0;

		// ---- buttons. A binding belongs to a player slot, as in the mapper's per-pad groups.
		/** Whether a control can be set at all (it has a source in the mapper). `control` is a control id, plus BIND_STICK_CLICK for a stick's click. */
		virtual bool supported(int control) const = 0;
		virtual BindingTarget binding(int player, int control) const = 0;
		virtual void setBinding(int player, int control, const BindingTarget& target) = 0;
		/** What a player's preset gives a control (the chooser's Default). */
		virtual BindingTarget defaultBinding(int player, int control) const = 0;
		/** Puts every control of a player back to their preset's defaults. */
		virtual void resetBindings(int player) = 0;
		/** What a control does, in short words; when the target would do nothing it says why instead ("Joystick off", "No button 3"). */
		std::string describe(int player, int control) const;
		/** The same for a target that is not set yet ("Key: Ctrl", or why it would do nothing for this player). */
		std::string describeTarget(int player, const BindingTarget& target) const;

		// ---- what the chooser offers
		/** The kinds of target a player can be given now, in display order: no Joystick kind when the player has no joystick. */
		std::vector<TARGET> kindsOffered(int player) const;
		/** The catalog indexes of a kind a player can use: joystick buttons the joystick has, everything else. */
		std::vector<int> choices(int player, TARGET kind) const;
		/** Likely targets for a control: what each preset gives it, that the player can use (no duplicates, nothing unassigned). */
		virtual std::vector<BindingTarget> suggestions(int player, int control) const = 0;

		// ---- press to choose
		/** Starts listening for a pressed key or mouse button, dropping anything pressed before. */
		virtual void beginCapture() = 0;
		/** The next key or mouse button pressed since beginCapture(); false when there is none. */
		virtual bool pollCapture(CaptureEvent& event) = 0;

		// ---- setups: a named, saved configuration, optionally tied to a program
		virtual std::vector<SetupInfo> setups() const = 0;
		/** Saves the current configuration under a name (replacing one of the same name). */
		virtual bool saveSetup(const std::string& name, const std::string& program) = 0;
		virtual bool loadSetup(int index) = 0;
		/** The DOS program running now ("" at the prompt or when unknown). */
		virtual std::string runningProgram() const = 0;
		/** The setup last loaded or saved (empty when there is none yet). */
		virtual std::string setupName() const = 0;
		/** Whether anything (a button, a preset, what DOS sees) changed since that setup was loaded or saved, or since the start when there is none. */
		virtual bool setupModified() const = 0;
};

/** The model the screens use (sample data until a real one is installed). */
ControlsModel& Controls();

/** Installs a model; null goes back to the sample data. The model must outlive its use. */
void Controls_SetModel(ControlsModel * model);

/** Installs where the model reads the console from; null goes back to the sample players. Must outlive its use. */
void Controls_SetHardware(ControlsHardware * hardware);

/** Sets what the sample hardware reports as held (for tests and previews; does nothing once another model is installed). */
void SampleControls_SetHeld(uint32_t bits);
/** Queues a press for the sample model's pollCapture() (tests and previews). */
void SampleControls_PushCapture(const CaptureEvent& event);
/** Makes a sample player a different kind of hardware (PadType::NONE: nothing connected) for tests and previews. */
void SampleControls_SetHardware(int player, PadType type, bool sideways = false, bool stickClicks = false);
/** Puts the sample players back to their usual hardware. */
void SampleControls_ResetHardware();
/** Makes a sample player hold several devices on one channel (a Wiimote and a GameCube pad), primary first; the first sets the type. */
void SampleControls_SetDevices(int player, const std::vector<PadType>& devices, bool sideways = false, bool stickClicks = false);

#endif
