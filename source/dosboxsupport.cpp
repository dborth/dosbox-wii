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
#include "video.h"
#include "setup.h"
#include "support.h"
#include "debug.h"
#include "mapper.h"
#include "cross.h"
#include "control.h"
#include "SDL.h"
#include "drivers/Platform.h"
#include "drivers/AudioDriver.h"
#include "drivers/Logger.h"
#include "dosboxsupport.h"
#include "preferences.h"
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
	char** newargs = new char* [parameters.size() + 1];
	// parameter 0 is the executable path
	// contents of the vector follow
	// last one is NULL
	for(Bitu i = 0; i < parameters.size(); i++) newargs[i] = (char*)parameters[i].c_str();
	newargs[parameters.size()] = NULL;
	platform->getAudio()->stopEmulatorAudio();
	SDL_Delay(50);
#if C_DEBUG
	// shutdown curses
	DEBUG_ShutDown(NULL);
#endif

	delete [] newargs;
}
void Restart(bool pressed) { // mapper handler
	restart_program(control->startup_params);
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
