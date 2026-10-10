/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * dosboxsupport.cpp
 *
 * What the DOSBox core needs from the app: the GFX_* title and message
 * hooks, restart, a few libc functions, and RunDOSBox(), which is what main()
 * hands over to.
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
#include "setup.h"
#include "support.h"
#include "debug.h"
#include "mapper.h"
#include "cross.h"
#include "control.h"
#include "SDL.h"
#include "drivers/Platform.h"
#include "drivers/AudioDriver.h"
#include "drivers/EmulatorAudioDriver.h"
#include "drivers/VideoDriver.h"
#include "drivers/EmulatorVideoDriver.h"
#include "drivers/Logger.h"
#include "dosboxsupport.h"
#include "preferences.h"
#include "displayconfig.h"
#include "input.h"
#include "fileop.h"
#include "menu.h"

void GFX_SetTitle(Bit32s cycles,int frameskip,bool paused){
	static Bit32s internal_cycles = 0;
	static int internal_frameskip = 0;
	if (cycles != -1)
	{
		internal_cycles = cycles;
		MENU_CyclesDisplay = cycles;
	}
	if (frameskip != -1)
	{
		internal_frameskip = frameskip;
		MENU_FrameskipDisplay = frameskip;
	}
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

void restart_program(std::vector<std::string> & parameters) {
	// There is no relaunch path on this platform (see the Status and Settings
	// pages: changes that need one take effect next launch). It used to stop
	// the emulator audio and return, which left DOSBox running silent for
	// good after CONFIG -r or Ctrl+Alt+Home.
	(void)parameters;
	Log_Printf(LOG_LEVEL_INFO, "Restart is not available here; settings that need one apply next launch");
}
void Restart(bool pressed) { // mapper handler
	restart_program(control->startup_params);
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
 * CanReinitDosSection
 *
 * Re-initialising the dos section re-runs XMS_Init and EMS_Init. XMS builds
 * the UMB chain again (DOS_UMBChainIsPristine says when that is harmless) and
 * EMS takes a callback slot with CALLBACK_Allocate that ~EMS never returns,
 * so after enough re-inits the next one ends in E_Exit. EMS_RESERVE keeps
 * some slots back for everything else that allocates them.
 ***************************************************************************/
#define EMS_RESERVE	8

bool DOS_UMBChainIsPristine(void);
Bitu CALLBACK_FreeCount(void);

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
const char * SBLASTER_EffectiveType(void);
const char * SBLASTER_EffectiveOpl(void);
int EMS_EffectiveType(void);

//! The core cpudecoder points at, or NULL if it points at something else.
static const char * CoreName(CPU_Decoder * decoder)
{
	if(decoder == CPU_Core_Normal_Run || decoder == CPU_Core_Normal_Trap_Run)
		return "normal";
	if(decoder == CPU_Core_Simple_Run || decoder == CPU_Core_Simple_Trap_Run)
		return "simple";
	if(decoder == CPU_Core_Full_Run)
		return "full";
	if(decoder == CPU_Core_Prefetch_Run || decoder == CPU_Core_Prefetch_Trap_Run)
		return "prefetch";
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
	status->frameskip = MENU_FrameskipDisplay;

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
	Section_prop * speaker = dynamic_cast<Section_prop *>(control->GetSection("speaker"));

	if(speaker)
	{
		const std::string tandy = speaker->Get_string("tandy");

		status->tandyOn = (tandy == "true" || tandy == "on" || (IS_TANDY_ARCH && tandy == "auto"));
	}

	status->memoryMB = (int)(MEM_TotalPages() / 256);	// 4 KB pages
	status->umbActive = (dos_infoblock.GetStartOfUMBChain() != 0xffff);	// DOS_BuildUMBChain: 0xffff means none
	status->emsType = EMS_EffectiveType();

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
}
