/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * dosboxsupport.h
 *
 * What the DOSBox core needs from the app. Included by main.cpp, which
 * must stay free of DOSBox headers.
 ***************************************************************************/

#ifndef _DOSBOXSUPPORT_H_
#define _DOSBOXSUPPORT_H_

//! Reads the config and runs DOSBox until the user exits it. The platform
//! and GUI must already be up. Returns when DOSBox has shut down.
void RunDOSBox(int argc, char* argv[]);

//! True when DOSBox is at the DOS prompt rather than inside a program, which
//! is when it is safe to change hardware settings (sound card, joystick, CPU
//! core) under it. False before the first shell exists.
bool IsShellIdle();

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
	bool aspectOn;			//!< the display is applying the source's pixel shape ([display] aspect = corrected)
	int frameWidth, frameHeight;	//!< what is handed to the display; 0 before a mode is set
	int frameskip;			//!< the running value, which the home screen's +/- changes

	char sbType[8];			//!< the Sound Blaster type running: sb1, sb2, sbpro1, sbpro2, sb16, gb or none
	char oplMode[12];		//!< none, cms, opl2, dualopl2, opl3 or opl3gold
	bool tandyOn;			//!< Tandy sound is installed

	int memoryMB;			//!< total memory the machine has (MEM_TotalPages)
	bool umbActive;			//!< upper memory blocks are available to DOS
	int emsType;			//!< 0 off, 1 true, 2 emsboard, 3 emm386
};

//! Fills in the state above. Returns false before DOSBox is up, when there
//! is nothing to read. Call from the main thread (the menu's), which is
//! between decoder runs.
bool GetEmulationStatus(EmulationStatus * status);

//! True when the DOS section (XMS, EMS, UMB, keyboard layout) can be
//! re-initialised without damage: the upper memory blocks are untouched and
//! enough callback slots are left for EMS, which keeps one per init. When
//! false, *reason (if not NULL) is a short sentence saying why.
bool CanReinitDosSection(const char ** reason);

#endif
