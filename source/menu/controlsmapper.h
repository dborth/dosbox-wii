/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsmapper.h
 *
 * The ControlsModel over DOSBox's mapper: what each button of each player's
 * pad does in DOS, read from and written to the mapper's per-player pad
 * groups (CPadBindGroup, "pad_N" in the mapper file). A change takes effect at
 * once and is written to the mapper file.
 *
 * The mapper is reached through PadMapper, so the logic here builds and tests
 * on a PC with a fake; controlslive.cpp is the one that calls the
 * mapper. What is not real yet is listed on MapperControlsModel.
 ***************************************************************************/
#ifndef _CONTROLSMAPPER_H_
#define _CONTROLSMAPPER_H_

#include "controlsmodel.h"
#include "controlssetups.h"
#include <string>
#include <vector>

/** What the model needs of the mapper: the MAPPER_Pad* calls of sdl_mapper.cpp, and the joystick type in effect. */
class PadMapper
{
	public:
		virtual ~PadMapper() {}
		/** Whether the mapper has an event of that name (key_enter, jbutton_1_0, hand_cycleup, ...). */
		virtual bool eventExists(const char * name) const = 0;
		/** What a source does now, as event names: its binds on the pad group first, then those on the stick group (what the shipped binds give it). */
		virtual std::vector<std::string> events(int pad, int source) const = 0;
		/** Makes a source do one thing: whatever it did is let go and removed, then it is bound to the event ("" binds nothing). False when it could not. */
		virtual bool setEvent(int pad, int source, const char * event) = 0;
		/** Writes the mapper file. */
		virtual bool save() = 0;
		/** The joystick type in effect: none, 2axis, 4axis, 4axis_2, fcs or ch. */
		virtual std::string joystickType() const = 0;
		/**
		 * Makes a joystick type the one in effect: the joystick.joysticktype setting is changed and the mapper's joystick
		 * groups are built again (the same path as Settings > Joystick type). False when DOSBox did not take it.
		 */
		virtual bool setJoystickType(const std::string& type) = 0;
};

/**
 * The mapper event a target stands for when it is bound to a player's button, or false when the mapper cannot
 * express it (a joystick button beyond the first four, or a joystick slot that does not exist: only the DOS
 * joysticks 0 and 1 have jbutton_N_* events). `slot` is the player's DOS joystick (ControlsModel::joystickSlot(),
 * -1 for none), which is the player's number only under 4axis, 2axis, fcs and ch. An unassigned target gives
 * true and an empty name.
 */
bool Mapper_EventFor(int slot, const BindingTarget& target, std::string& event);
/** The target an event stands for on a player's button, or false when it is none the catalogs hold (a joystick hat or axis, another joystick's button). */
bool Mapper_TargetFor(int slot, const std::string& event, BindingTarget& target);

/**
 * After the joystick type changed: pad binds to a joystick button that is not the player's own are moved to the
 * player's joystick (player 2 under 4axis_2 is joystick 1, so jbutton_1_0 becomes jbutton_0_0) or removed when
 * the player has none (player 1 under 4axis_2 would otherwise fire player 2's joystick). A bind to a joystick
 * nobody uses is left alone, so going back to two joysticks finds player 2's buttons as they were. Safe to call
 * again: a second call finds nothing to move. Returns whether anything was rebound; the caller saves.
 */
bool Mapper_MigrateJoystickBinds(PadMapper& mapper);

/**
 * The model the controller screens use once DOSBox is running. Bindings, presets and "modified" are real, for all
 * four players, and so is the emulated mode: setEmulated() sets joystick.joysticktype and the mapper's joystick
 * groups follow, currentEmulated() reads the type in effect (4axis_2, fcs and ch show as their own advanced
 * modes, with the right player holding the joystick), and the Settings > Joystick type row goes through the same
 * path. Key and mouse capture go through the ControlsHardware (the USB keyboard and mouse drivers).
 *
 * Setups on disk (controlssetups.h) need a SetupHost; without one (null) setups() is empty, saveSetup() and
 * loadSetup() fail and runningProgram() is empty.
 *
 *  - saveSetup(): the mapper file is written (MAPPER_SaveBindsNow), copied to <name>.map in the setups folder, and
 *    <name>.conf is written beside it with [sdl] mapperfile, [joystick] joysticktype, the program and each connected
 *    player's preset. The map goes first, so a save cut short never lists a setup with no map.
 *  - loadSetup(): the joystick type is set first (the mapper's joystick groups are built again), then every player's
 *    sixteen sources are given what <name>.map holds for them (its pad_N and stick_N binds), then binds to a joystick
 *    that is not the player's own are moved (Mapper_MigrateJoystickBinds), and the result is written to the mapper
 *    file as any change is. Only the pad sources are applied: keys, hotkeys' own keys and joystick axes in the .map
 *    take effect when DOSBox starts with that file as [sdl] mapperfile, not on load.
 *  - the preset a player "started from" is not in the mapper file: a setup keeps it in its .conf, and a load puts it
 *    back; otherwise it is the closest preset to the player's buttons when first asked, then whatever applyPreset() chose.
 *
 * While the mapper file DOSBox is using lives in the setups folder (DOSBox was started with a setup's conf),
 * changes are not written through to it: that file is a setup, and a load or an edit would silently change the
 * setup it belongs to. They are held in the running mapper and kept by saveSetup(), as setupModified() shows.
 * saveSetup() itself always writes the mapper file (it is the only way to get the running binds out), so in that
 * one situation saving under another name also brings the file's own setup up to date. Closing that needs the
 * mapper to be able to write elsewhere than the file it started with (a core change).
 *
 * A binding the catalogs cannot hold (a joystick hat, an axis, a fifth joystick button) reads as Not assigned and is
 * left alone until the user changes that button.
 */
class MapperControlsModel : public ControlsModel
{
	public:
		MapperControlsModel(ControlsHardware& hardware, PadMapper& mapper, SetupHost * setupHost = nullptr);

		int playerCount() const override { return 4; }
		PlayerInfo player(int index) const override;
		DeviceStatus devices() const override;
		uint32_t held(int player) const override;

		std::vector<EmulatedMode> emulatedModes() const override;
		int currentEmulated() const override;
		void setEmulated(int index) override;

		std::vector<PresetInfo> presets() const override;
		int currentPreset(int player) const override;
		void applyPreset(int player, int index) override;
		BindingTarget presetBinding(int preset, int control) const override;
		bool modified(int player) const override;

		bool supported(int control) const override;
		BindingTarget binding(int player, int control) const override;
		void setBinding(int player, int control, const BindingTarget& target) override;
		BindingTarget defaultBinding(int player, int control) const override;
		void resetBindings(int player) override;
		std::vector<BindingTarget> suggestions(int player, int control) const override;

		void beginCapture() override;
		bool pollCapture(CaptureEvent& event) override;

		std::vector<SetupInfo> setups() const override;
		bool saveSetup(const std::string& name, const std::string& program) override;
		bool loadSetup(int index) override;
		std::string runningProgram() const override;
		std::string setupName() const override;
		bool setupModified() const override;

		/** Whether the last change could not be written to the mapper file (it is in effect, but lost when DOSBox closes). */
		bool saveFailed() const { return failedSave; }

	private:
		static bool validPlayer(int player) { return player >= 0 && player < 4; }
		/** What the mapper has on a button now (Not assigned for a bind the catalogs cannot hold). */
		BindingTarget current(int player, int control) const;
		/** A joystick button the player's joystick does not have does nothing: it counts as Not assigned. */
		BindingTarget effective(int player, const BindingTarget& target) const;
		bool write(int player, int control, const BindingTarget& target);
		bool writePreset(int player, int preset);
		int differences(int player, int preset) const;
		void saved();
		/** Whether the mapper file DOSBox is using is one of the setups' own (it is in the setups folder). */
		bool mapperFileIsSetup() const;
		/** Writes the mapper file unless it is a setup's own; false when it was due and failed. */
		bool writeThrough();

		ControlsHardware& hw;
		PadMapper& mapper;
		SetupHost * host;
		std::string currentSetup;
		/** A type change leaves a cached preset that needs a joystick the player no longer has: forget it so it is chosen again. */
		void followJoystickType() const;

		mutable int presetOf[4];
		mutable std::string seenType;
		bool dirty;
		bool failedSave;
};

#endif
