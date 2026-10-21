/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * dosboxsupport.h
 *
 * What the DOSBox core needs from the app, and what the app asks of the core.
 * Included by main.cpp, which must stay free of DOSBox headers.
 ***************************************************************************/

#ifndef _DOSBOXSUPPORT_H_
#define _DOSBOXSUPPORT_H_

#include <stddef.h>

//! Reads the config and runs DOSBox until the user exits it. The platform
//! and GUI must already be up. Returns when DOSBox has shut down.
void RunDOSBox(int argc, char* argv[]);

//! Asks for DOSBox to be shut down and started again (RunDOSBox() returns,
//! then main() calls it again with the same arguments). Takes effect when the
//! menu closes, or at once from CONFIG -r. Nothing is saved: whatever is
//! running is lost.
void RequestRestart();

//! The conf of the game DOSBox is set to run: a path relative to the DOSBox
//! folder, or absolute. It is parsed after dosbox.conf, so it only has to hold
//! what differs. Empty when there is none. Kept across restarts, so Restart
//! starts the same game again; changing it only takes effect at the next start.
const char * GetGameConf();

//! Sets (or, with NULL or "", clears) the game conf for the next start.
void SetGameConf(const char * conf);

//! SetGameConf() and RequestRestart(): leaves what is running and starts the
//! game (or plain DOSBox, with NULL or "").
void RequestRestartInto(const char * conf);

//! Which start of DOSBox this is: 1 for the first, one more for each restart. What the menu keeps about the
//! running DOSBox (the game that was loaded) is not true of the next one.
int RunNumber();

//! True between RequestRestart() and the core leaving.
bool RestartRequested();

//! Returns whether a restart was asked for, and clears the request. For main().
bool TakeRestartRequest();

//! True when DOSBox is at the DOS prompt rather than inside a program, which
//! is when it is safe to change hardware settings (sound card, joystick, CPU
//! core) under it. False before the first shell exists.
bool IsShellIdle();

//! The CPU cycles the core is running now, as GFX_SetTitle() last heard them.
//! The home screen's +/- buttons and the hotkeys change this; the config does
//! not follow by itself (see Settings_SyncRunning()). The value is a percentage of the host while *autoAdjust is true, otherwise
//! cycles per millisecond; 0 until the core has reported one.
int GetRunningCycles(bool * autoAdjust);

//! The frameskip the core is running now (the home screen's +/- and the hotkeys
//! change it; the config does not follow by itself); 0 until the core has
//! reported one.
int GetRunningFrameskip();

//! The fixed output format of the emulator audio driver, as DOSBox's mixer
//! runs it: sample rate in Hz and stereo frames per buffer. Returns false
//! when there is no emulator audio driver (nothing to play through), in
//! which case neither value is set.
bool GetEmulatorAudioInfo(int * sampleRate, int * framesPerBuffer);

//! What the emulator video driver can do, so the Video page can leave out rows
//! that would do nothing here. Any of the three may be NULL. Returns false when
//! there is no emulator video driver, in which case all three are false.
bool GetEmulatorVideoCapabilities(bool * scanlines, bool * sharpFilter, bool * widescreenSetting);

/**
 * What DOSBox is actually doing, as opposed to what the config says, for the
 * Status page. Everything is a plain value read from the running core; the
 * page decides how to show it and compares it with the config itself.
 */
struct EmulationStatus
{
	//! The core running now: "normal", "full" or "dynamic";
	//! empty if it cannot be told (the decoder is momentarily
	//! something else, eg. while the guest is halted)
	char core[16];
	bool autoAdjust;		//!< cycles are a share of the host (max), not a fixed count
	int cycleMax;			//!< cycles per millisecond now; measured while autoAdjust
	int cyclePercent;		//!< the share of the host used while autoAdjust
	bool atPrompt;			//!< the DOS prompt, not a program (IsShellIdle)

	bool haveMode;			//!< a video mode has been set
	bool textMode;
	int srcWidth, srcHeight;	//!< what the emulated card draws
	double pixelRatio;		//!< shape of one source pixel (taller than wide when above 1)
	bool aspectOn;			//!< the display is applying the source's pixel shape ([display] aspect = original)
	int frameWidth, frameHeight;	//!< what is handed to the display; 0 before a mode is set
	int frameskip;			//!< the running value, which the home screen's +/- changes

	char sbType[8];			//!< the Sound Blaster type running: sb1, sb2, sbpro1, sbpro2, sb16, gb or none
	char oplMode[12];		//!< none, cms, opl2, dualopl2, opl3 or opl3gold
	bool tandyOn;			//!< Tandy sound is installed

	int memoryMB;			//!< total memory the machine has (MEM_TotalPages)
	bool umbActive;			//!< upper memory blocks are available to DOS
	int emsType;			//!< 0 off, 1 true, 2 emsboard, 3 emm386

	char joystick[10];		//!< the joystick type in effect: none, 2axis, 4axis, 4axis_2, fcs or ch (auto only before the mapper has built its groups)
};

//! The joystick type in effect, which is not the config's when that says auto
//! (the mapper replaces auto with a type when it builds its groups; 4axis, as
//! things are). Never NULL.
const char * GetJoystickTypeName(void);

//! The mapper file the core is using: [sdl] mapperfile as the mapper resolved it
//! at start (the path MAPPER_SaveBindsNow() writes). False when it cannot be
//! told. Read from the config property, so it is right unless that was changed
//! with CONFIG -set after start, which the mapper does not follow either.
bool GetMapperFilePath(char * path, size_t size);

//! The DOS program running now, as DOS recorded it when it was started: the
//! file name with its extension ("KEEN4E.EXE", read from the environment block
//! DOS_Execute writes), or just the short name the MCB keeps ("KEEN4E") when
//! that is not there. False at the DOS prompt, or before the first shell.
//! Main thread, between decoder runs (as the menu is).
bool GetRunningProgram(char * name, size_t size);

//! The same program's whole DOS path, as DOS recorded it ("C:\GAMES\KEEN4\KEEN4E.EXE"):
//! what saving a game needs to write the lines that start it. False at the DOS
//! prompt, or when the environment block does not have it. Main thread, between
//! decoder runs.
bool GetRunningProgramPath(char * path, size_t size);

//! Makes the joystick settings in the config take effect: builds the mapper's
//! joystick bind groups again, as the type, button wrapping and the like are
//! read when they are built. Call after the joystick section has been
//! initialised again with the new values. Main thread.
void ReinitJoysticks(void);

//! Call when the menu has closed and emulation is about to run again: what the
//! pads' binds (the controller screens) held down is let go, and a button still held
//! (the one that closed the menu) does nothing until it is released. Main thread.
void ResyncPadBinds(void);

//! Fills in the state above. Returns false before DOSBox is up, when there
//! is nothing to read. Call from the main thread (the menu's), which is
//! between decoder runs.
bool GetEmulationStatus(EmulationStatus * status);

//! True when the DOS section (XMS, EMS, UMB, keyboard layout) can be
//! re-initialised without damage: the upper memory blocks are untouched and
//! enough callback slots are left for EMS, which keeps one per init. When
//! false, *reason (if not NULL) is a short sentence saying why.
bool CanReinitDosSection(const char ** reason);

//! ---- What the app added to the core
//!
//! Small getters and the mapper's pad calls that the core exports but none of
//! its own headers declares. Declared here, once, so no file repeats them and
//! files that must stay free of DOSBox headers can still reach them.

// the mapper (dosbox/gui/sdl_mapper.cpp)
bool MAPPER_EventExists(const char * name);
int MAPPER_PadGetEvents(int pad, int source, char names[][16], int max, bool * from_stick);
bool MAPPER_PadSetEvent(int pad, int source, const char * event);
bool MAPPER_SaveBindsNow(void);
void MAPPER_RebuildJoysticks(void);
void MAPPER_PadsResync(void);

// what is effectively running (hardware/sblaster.cpp, ints/ems.cpp)
const char * SBLASTER_EffectiveType(void);
const char * SBLASTER_EffectiveOpl(void);
int EMS_EffectiveType(void);

// whether the dos section can be set up again without harm (dos/dos_memory.cpp)
bool DOS_UMBChainIsPristine(void);

// the dynamic core's code cache, which the memory manager borrows while the menu is open (cpu/core_dynrec.cpp)
bool CPU_Core_Dynrec_LendCache(void ** base, size_t * size);
void CPU_Core_Dynrec_ReclaimCache(void);

// the helper MOUNT -u and IMGMOUNT -u share (dos/dos_programs.cpp)
const char * UnmountHelper(char umount);

#endif
