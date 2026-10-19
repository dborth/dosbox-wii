/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * dosboxsupport.cpp
 *
 * The two sides of the app and the core. For the core: the GFX_* title and
 * message hooks, restart, a few libc functions, and RunDOSBox(), which is what
 * main() hands over to. For the menu: what is running and what it is set to,
 * and re-initialising sections safely (declared in dosboxsupport.h).
 ***************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <string>
#include <vector>

#include "dosbox.h"
#include "dos_inc.h"
#include "shell.h"
#include "video.h"
#include "cpu.h"
#include "vga.h"
#include "render.h"
#include "mem.h"
#include "joystick.h"
#include "setup.h"
#include "support.h"
#include "debug.h"
#include "mapper.h"
#include "cross.h"
#include "control.h"
#include "SDL.h"
#include "SDL_input.h"
#include "drivers/Platform.h"
#include "drivers/AudioDriver.h"
#include "drivers/EmulatorAudioDriver.h"
#include "drivers/VideoDriver.h"
#include "drivers/EmulatorVideoDriver.h"
#include "drivers/Logger.h"
#include "drivers/Time.h"
#include "dosboxsupport.h"
#include "preferences.h"
#include "input.h"
#include "storage.h"

// What the core last reported through GFX_SetTitle()
static int runningCycles = 0;
static int runningFrameskip = 0;

void GFX_SetTitle(Bit32s cycles,int frameskip,bool paused){
	if (cycles != -1)
		runningCycles = cycles;
	if (frameskip != -1)
		runningFrameskip = frameskip;
}

int GetRunningCycles(bool * autoAdjust)
{
	if(autoAdjust)
		*autoAdjust = CPU_CycleAutoAdjust;

	return runningCycles;
}

int GetRunningFrameskip()
{
	return runningFrameskip;
}


/****************************************************************************
 * GFX_ShowMsg
 *
 * Routed to the platform Logger (a no-op unless built with LOGGING_ENABLED).
 ***************************************************************************/
void GFX_ShowMsg(char const* format,...) {
	char buf[512];
	va_list msg;
	va_start(msg,format);
	vsnprintf(buf,sizeof(buf),format,msg);
	va_end(msg);
	Log_Printf(LOG_LEVEL_INFO, "%s", buf);
}

#if C_DEBUG
extern void DEBUG_ShutDown(Section * /*sec*/);
#endif

static bool restartRequested = false;
static int runNumber = 0;	// 1 for the first RunDOSBox()
static Ticks lastMark = 0;

void RestartLog(const char * what) {
	const Ticks now = SystemTime::now();
	const uint32_t ms = lastMark ? SystemTime::diffMillisecs(lastMark, now) : 0;
	lastMark = now;
	Log_Printf(LOG_LEVEL_INFO, "RESTART run %d: %s (+%u ms) %s", runNumber, what, (unsigned)ms,
		(platform && platform->getMemoryFreeInfo()) ? platform->getMemoryFreeInfo() : "memory unknown");
}

void RequestRestart() {
	restartRequested = true;
	RestartLog("restart requested");
}

bool RestartRequested() {
	return restartRequested;
}

bool TakeRestartRequest() {
	const bool r = restartRequested;
	restartRequested = false;
	return r;
}

void restart_program(std::vector<std::string> & parameters) {
	// CONFIG -r. The arguments are not used yet: the restart runs with the
	// arguments DOSBox was started with. Leaves the core the way the
	// killswitch does (RunDOSBox() catches it).
	(void)parameters;
	RequestRestart();
	throw 1;
}


/****************************************************************************
 * IsShellIdle
 *
 * Used before a hardware setting change
 ***************************************************************************/
bool IsShellIdle() {
	return first_shell != NULL && (DOS_PSP(dos.psp()).GetSegment() == DOS_PSP(dos.psp()).GetParent());
}

/****************************************************************************
 * GetMapperFilePath, GetRunningProgram
 *
 * What the setups on disk need to know about the core: where the mapper's file
 * is, and which program is running.
 ***************************************************************************/
std::string Setup_ProgramFromEnvironment(const unsigned char * env, size_t size);	// menu/controlssetups.cpp

bool GetMapperFilePath(char * path, size_t size)
{
	if(!path || size == 0)
		return false;
	path[0] = 0;

	Section_prop * section = control ? static_cast<Section_prop *>(control->GetSection("sdl")) : NULL;
	Prop_path * property = section ? section->Get_path("mapperfile") : NULL;

	if(!property || property->realpath.empty())
		return false;

	safe_strncpy(path, property->realpath.c_str(), size);
	return true;
}

bool GetRunningProgram(char * name, size_t size)
{
	if(!name || size == 0)
		return false;
	name[0] = 0;

	if(first_shell == NULL || IsShellIdle())
		return false;

	// DOS_Execute leaves the program's full path after the environment's variables
	std::string program;
	const Bit16u envseg = DOS_PSP(dos.psp()).GetEnvironment();

	if(envseg != 0)
	{
		Bitu bytes = (Bitu)DOS_MCB((Bit16u)(envseg - 1)).GetSize() * 16;

		if(bytes > 32768)
			bytes = 32768;

		std::vector<unsigned char> env(bytes);

		for(Bitu i = 0; i < bytes; i++)
			env[i] = mem_readb(PhysMake(envseg, 0) + i);

		program = Setup_ProgramFromEnvironment(env.empty() ? NULL : &env[0], env.size());
	}

	if(program.empty())
	{
		// no full path: the short name the MCB keeps (the file name up to 8 characters, no extension)
		char shortName[9];

		DOS_MCB((Bit16u)(dos.psp() - 1)).GetFileName(shortName);
		program = shortName;
	}

	if(program.empty())
		return false;

	safe_strncpy(name, program.c_str(), size);
	return true;
}

/****************************************************************************
 * CanReinitDosSection
 *
 * Re-initialising the dos section re-runs XMS_Init and EMS_Init. XMS builds
 * the UMB chain again (DOS_UMBChainIsPristine says when that is harmless) and
 * EMS takes a callback slot with CALLBACK_Allocate that ~EMS never returns,
 * so after enough re-inits the next one ends in E_Exit. EMS_RESERVE keeps
 * some slots back for everything else that allocates them.
 ***************************************************************************/
#define EMS_RESERVE	8

Bitu CALLBACK_FreeCount(void);	// not in dosboxsupport.h: it returns the core's Bitu

bool CanReinitDosSection(const char ** reason)
{
	if(!DOS_UMBChainIsPristine())
	{
		if(reason)
			*reason = "Upper memory is in use by a program.";
		return false;
	}

	if(CALLBACK_FreeCount() < EMS_RESERVE)
	{
		if(reason)
			*reason = "DOSBox is out of internal handlers for memory changes. Edit dosbox.conf and restart.";
		return false;
	}

	return true;
}

/****************************************************************************
 * GetEmulatorAudioInfo
 *
 * What the mixer is fixed to (see MIXER_Init): the driver's rate and buffer
 * size, never a number assumed here, so a platform with a different output
 * format needs no change.
 ***************************************************************************/
bool GetEmulatorAudioInfo(int * sampleRate, int * framesPerBuffer)
{
	if(!platform || !platform->getAudio())
		return false;

	EmulatorAudioDriver * audio = platform->getAudio()->getEmulatorAudio();

	if(!audio)
		return false;

	*sampleRate = audio->getSampleRate();
	*framesPerBuffer = audio->getFramesPerBuffer();
	return true;
}

/****************************************************************************
 * GetEmulatorVideoCapabilities
 *
 * Asked of the driver every time: what it can do depends on the console and
 * the video mode it found, not on anything written here.
 ***************************************************************************/
bool GetEmulatorVideoCapabilities(bool * scanlines, bool * sharpFilter, bool * widescreenSetting)
{
	EmulatorVideoCapabilities caps;
	EmulatorVideoDriver * video = (platform && platform->getVideo()) ? platform->getVideo()->getEmulatorVideo() : NULL;

	if(video)
		caps = video->getCapabilities();

	if(scanlines)
		*scanlines = caps.scanlines;
	if(sharpFilter)
		*sharpFilter = caps.sharpFilter;
	if(widescreenSetting)
		*widescreenSetting = caps.widescreenSetting;

	return video != NULL;
}

/****************************************************************************
 * GetEmulationStatus
 *
 * What is running, for the Status page. The effective values that live in
 * the core's own files come through small getters defined there; everything
 * else is read from what the core already exports.
 ***************************************************************************/
const char * GetJoystickTypeName(void)
{
	switch(joytype)
	{
		case JOY_NONE: return "none";
		case JOY_2AXIS: return "2axis";
		case JOY_4AXIS: return "4axis";
		case JOY_4AXIS_2: return "4axis_2";
		case JOY_FCS: return "fcs";
		case JOY_CH: return "ch";
		case JOY_AUTO: break;
	}
	// JoystickType is set to auto by JOYSTICK_Init and replaced by what it
	// stands for when the mapper builds its groups, so this is only seen
	// between the two
	return "auto";
}

void ReinitJoysticks(void)
{
	MAPPER_RebuildJoysticks();
}

void ResyncPadBinds(void)
{
	MAPPER_PadsResync();
	InputHal_RefreshPadRouting();
}

//! The core cpudecoder points at, or NULL if it points at something else.
static const char * CoreName(CPU_Decoder * decoder)
{
	if(decoder == CPU_Core_Normal_Run || decoder == CPU_Core_Normal_Trap_Run)
		return "normal";
	if(decoder == CPU_Core_Full_Run)
		return "full";
#if C_DYNREC
	if(decoder == CPU_Core_Dynrec_Run || decoder == CPU_Core_Dynrec_Trap_Run)
		return "dynamic";
#endif
	return NULL;
}

bool GetEmulationStatus(EmulationStatus * status)
{
	if(!status || !control || !first_shell)
		return false;

	memset(status, 0, sizeof(*status));

	// While the guest is halted cpudecoder is the halt handler (cpu.cpp) and
	// the core that will run again is the one it saved. That is only
	// meaningful then, so it is only looked at when cpudecoder is not a core.
	const char * core = CoreName(cpudecoder);

	if(!core)
		core = CoreName(cpu.hlt.old_decoder);

	if(core)
		safe_strncpy(status->core, core, sizeof(status->core));

	status->autoAdjust = CPU_CycleAutoAdjust;
	status->cycleMax = (int)CPU_CycleMax;
	status->cyclePercent = (int)CPU_CyclePercUsed;
	status->atPrompt = IsShellIdle();

	status->haveMode = render.src.width > 0 && render.src.height > 0;
	status->textMode = (vga.mode == M_TEXT || vga.mode == M_HERC_TEXT || vga.mode == M_TANDY_TEXT);
	status->srcWidth = (int)render.src.width;
	status->srcHeight = (int)render.src.height;
	status->pixelRatio = render.src.ratio;

	// Whether the source's pixel shape is applied is the display's choice
	// ([display] aspect); what counts is what the driver has been given
	EmulatorVideoDriver * video = (platform && platform->getVideo()) ? platform->getVideo()->getEmulatorVideo() : NULL;

	status->aspectOn = video ? (video->getSettings().aspect == VideoAspect::Corrected) : true;
	status->frameskip = runningFrameskip;

	int frameWidth = 0, frameHeight = 0;
	bool fullscreen = true;

	GFX_GetSize(frameWidth, frameHeight, fullscreen);
	status->frameWidth = frameWidth;
	status->frameHeight = frameHeight;

	const char * sb = SBLASTER_EffectiveType();
	const char * opl = SBLASTER_EffectiveOpl();

	safe_strncpy(status->sbType, sb ? sb : "", sizeof(status->sbType));
	safe_strncpy(status->oplMode, opl ? opl : "", sizeof(status->oplMode));

	// tandy_sound.cpp: on a Tandy or PCjr "true", "on" and "auto" turn the
	// sound on; on any other machine only "true" and "on" do
	Section_prop * speaker = Config_GetSection("speaker");

	if(speaker)
	{
		const std::string tandy = speaker->Get_string("tandy");

		status->tandyOn = (tandy == "true" || tandy == "on" || (IS_TANDY_ARCH && tandy == "auto"));
	}

	status->memoryMB = (int)(MEM_TotalPages() / 256);	// 4 KB pages
	status->umbActive = (dos_infoblock.GetStartOfUMBChain() != 0xffff);	// DOS_BuildUMBChain: 0xffff means none
	status->emsType = EMS_EffectiveType();
	safe_strncpy(status->joystick, GetJoystickTypeName(), sizeof(status->joystick));

	return true;
}

/****************************************************************************
 * C library functions newlib does not have. Both are used by the core's
 * path handling.
 ***************************************************************************/
static char tmp[MAXPATHLEN];

char * dirname(char * file)
{
	if(!file || file[0] == 0)
		return ".";

	char * sep = strrchr(file, '/');
	if (sep == NULL)
		sep = strrchr(file, '\\');
	if (sep == NULL)
		return ".";

	int len = (int)(sep - file);
	safe_strncpy(tmp, file, len+1);
	return tmp;
}
int access(const char *path, int amode)
{
	struct stat st;
	bool folderExists = (stat(path, &st) == 0);
	if (folderExists) return 0;
	else return ENOENT;
}

/****************************************************************************
 * RunDOSBox
 *
 * Reads the config, then runs DOSBox until the user exits it. The platform
 * and GUI are already up. Returns when DOSBox has shut down.
 ***************************************************************************/
void RunDOSBox(int argc, char* argv[]) {
	runNumber++;
	RestartLog("core starting");
	try {
		CommandLine com_line(argc,argv);
		Config myconf(&com_line);
		control=&myconf;
		/* Init the configuration system and add default values */
		Config_Add_SDL();
		Config_Add_Display();
		DOSBOX_Init();

		ResetPrefs();

#if C_DEBUG
		DEBUG_SetupConsole();
#endif

		/* Display Welcometext in the console */
		LOG_MSG("DOSBox version %s",VERSION);
		LOG_MSG("Copyright 2002-2019 DOSBox Team, published under GNU GPL.");
		LOG_MSG("---");

		InitInput();
		LoadPrefs();

		/* Init all the sections */
		control->Init();
		/* Init the keyMapper */
		MAPPER_Init();
		if (control->cmdline->FindExist("-startmapper")) MAPPER_RunInternal();

		std::string config_path;
		Cross::GetPlatformConfigDir(config_path);
		MountDOSBoxDir('C', config_path.c_str());

		RestartLog("sections initialised");

		/* Start up main machine */
		control->StartUp();
		/* Shutdown everything */
	} catch (char * error) {
		GFX_ShowMsg("Exit to error: %s",error);
		fflush(NULL);
	}
	catch (int){
		; //nothing, pressed killswitch
	}
	catch(...){
		; // Unknown error, let's just exit.
	}

	// Config (and with it every section's destroy function) is gone by now. The
	// pointer is not, and the mapper keeps its events and bind groups in statics
	// that MAPPER_Init() would add to again on the next run.
	control = NULL;
	MAPPER_Reset();
	RestartLog("core shut down");
}
