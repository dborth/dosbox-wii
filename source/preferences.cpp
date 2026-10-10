/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * preferences.cpp
 *
 * Preferences: the [sdl] config section, and finding, loading and generating
 * dosbox.conf.
 ***************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <unistd.h>

#include "dosbox.h"
#include "control.h"
#include "setup.h"
#include "cross.h"
#include "support.h"
#include "mapper.h"
#include "drivers/Logger.h"
#include "preferences.h"
#include "input.h"

#define MAPPERFILE "mapper-" VERSION ".map"

/****************************************************************************
 * Config_Add_SDL
 *
 * Declares the [sdl] section. GUI_StartUp() (input.cpp) reads it.
 ***************************************************************************/
void Config_Add_SDL() {
	Section_prop * sdl_sec=control->AddSection_prop("sdl",&GUI_StartUp);
	sdl_sec->AddInitFunction(&MAPPER_StartUp);
	Prop_bool* Pbool;
	Prop_string* Pstring;
	Prop_int* Pint;
	Prop_multival* Pmulti;

	Pbool = sdl_sec->Add_bool("fullscreen",Property::Changeable::Always,true);
	Pbool->Set_help("Start dosbox directly in fullscreen. (Press ALT-Enter to go back)");

	Pbool = sdl_sec->Add_bool("fulldouble",Property::Changeable::Always,true);
	Pbool->Set_help("Use double buffering in fullscreen. It can reduce screen flickering, but it can also result in a slow DOSBox.");

	Pstring = sdl_sec->Add_string("fullresolution",Property::Changeable::Always,"original");
	Pstring->Set_help("What resolution to use for fullscreen: original, desktop or a fixed size (e.g. 1024x768).\n"
	                  "Using your monitor's native resolution with aspect=true might give the best results.\n"
			  "If you end up with small window on a large screen, try an output different from surface."
	                  "On Windows 10 with display scaling (Scale and layout) set to a value above 100%, it is recommended\n"
	                  "to use a lower full/windowresolution, in order to avoid window size problems.");

	Pstring = sdl_sec->Add_string("windowresolution",Property::Changeable::Always,"original");
	Pstring->Set_help("Scale the window to this size IF the output device supports hardware scaling.\n"
	                  "(output=surface does not!)");

	const char* outputs[] = {
		"surface", "overlay",
		0 };
	Pstring = sdl_sec->Add_string("output",Property::Changeable::Always,"surface");
	Pstring->Set_help("What video system to use for output.");
	Pstring->Set_values(outputs);

	Pbool = sdl_sec->Add_bool("autolock",Property::Changeable::Always,true);
	Pbool->Set_help("Ignored on this build: the mouse is always captured, so the first click is not used up capturing it.");

	Pmulti = sdl_sec->Add_multi("sensitivity",Property::Changeable::Always, ",");
	Pmulti->Set_help("Mouse sensitivity. The optional second parameter specifies vertical sensitivity (e.g. 100,-50).");
	Pmulti->SetValue("100");
	Pint = Pmulti->GetSection()->Add_int("xsens",Property::Changeable::Always,100);
	Pint->SetMinMax(-1000,1000);
	Pint = Pmulti->GetSection()->Add_int("ysens",Property::Changeable::Always,100);
	Pint->SetMinMax(-1000,1000);

	Pbool = sdl_sec->Add_bool("waitonerror",Property::Changeable::Always, true);
	Pbool->Set_help("Wait before closing the console if dosbox has an error.");

	Pmulti = sdl_sec->Add_multi("priority", Property::Changeable::Always, ",");
	Pmulti->SetValue("higher,normal");
	Pmulti->Set_help("Priority levels for dosbox. Second entry behind the comma is for when dosbox is not focused/minimized.\n"
	                 "pause is only valid for the second entry.");

	const char* actt[] = { "lowest", "lower", "normal", "higher", "highest", "pause", 0};
	Pstring = Pmulti->GetSection()->Add_string("active",Property::Changeable::Always,"higher");
	Pstring->Set_values(actt);

	const char* inactt[] = { "lowest", "lower", "normal", "higher", "highest", "pause", 0};
	Pstring = Pmulti->GetSection()->Add_string("inactive",Property::Changeable::Always,"normal");
	Pstring->Set_values(inactt);

	Pstring = sdl_sec->Add_path("mapperfile",Property::Changeable::Always,MAPPERFILE);
	Pstring->Set_help("File used to load/save the key/event mappings from. Resetmapper only works with the default value.");
}


static void show_warning(char const * const message) {
	// Used to be drawn on the SDL surface for a few seconds. Now logged.
	Log_Printf(LOG_LEVEL_INFO, "%s", message);
}

static void eraseconfigfile() {
	FILE* f = fopen("dosbox.conf","r");
	if(f) {
		fclose(f);
		show_warning("Warning: dosbox.conf exists in current working directory.\nThis will override the configuration file at runtime.\n");
	}
	std::string path,file;
	Cross::GetPlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;
	f = fopen(path.c_str(),"r");
	if(!f) exit(0);
	fclose(f);
	unlink(path.c_str());
	exit(0);
}

static void erasemapperfile() {
	FILE* g = fopen("dosbox.conf","r");
	if(g) {
		fclose(g);
		show_warning("Warning: dosbox.conf exists in current working directory.\nKeymapping might not be properly reset.\n"
		             "Please reset configuration as well and delete the dosbox.conf.\n");
	}

	std::string path,file=MAPPERFILE;
	Cross::GetPlatformConfigDir(path);
	path += file;
	FILE* f = fopen(path.c_str(),"r");
	if(!f) exit(0);
	fclose(f);
	unlink(path.c_str());
	exit(0);
}

/****************************************************************************
 * ResetPrefs
 *
 * Handles the -eraseconf/-resetconf and -erasemapper/-resetmapper switches.
 * Exits after erasing.
 ***************************************************************************/
void ResetPrefs() {
	if(control->cmdline->FindExist("-eraseconf")) eraseconfigfile();
	if(control->cmdline->FindExist("-resetconf")) eraseconfigfile();
	if(control->cmdline->FindExist("-erasemapper")) erasemapperfile();
	if(control->cmdline->FindExist("-resetmapper")) erasemapperfile();
}

/****************************************************************************
 * LoadPrefs
 *
 * Parses the config files: -userconf, then -conf, then dosbox.conf in the
 * working directory, then the one in the dosbox folder. Generates a default
 * if there is none.
 ***************************************************************************/
void LoadPrefs() {
	/* Parse configuration files */
	std::string config_file, config_path, config_combined;
	Cross::GetPlatformConfigDir(config_path);

	//First parse -userconf
	if(control->cmdline->FindExist("-userconf",true)){
		config_file.clear();
		Cross::GetPlatformConfigDir(config_path);
		Cross::GetPlatformConfigName(config_file);
		config_combined = config_path + config_file;
		control->ParseConfigFile(config_combined.c_str());
		if(!control->configfiles.size()) {
			//Try to create the userlevel configfile.
			config_file.clear();
			Cross::CreatePlatformConfigDir(config_path);
			Cross::GetPlatformConfigName(config_file);
			config_combined = config_path + config_file;
			if(control->PrintConfig(config_combined.c_str())) {
				LOG_MSG("CONFIG: Generating default configuration.\nWriting it to %s",config_combined.c_str());
				//Load them as well. Makes relative paths much easier
				control->ParseConfigFile(config_combined.c_str());
			}
		}
	}

	//Second parse -conf switches
	while(control->cmdline->FindString("-conf",config_file,true)) {
		if (!control->ParseConfigFile(config_file.c_str())) {
			// try to load it from the user directory
			if (!control->ParseConfigFile((config_path + config_file).c_str())) {
				LOG_MSG("CONFIG: Can't open specified config file: %s",config_file.c_str());
			}
		}
	}
	// if none found => parse localdir conf
	if(!control->configfiles.size()) control->ParseConfigFile("dosbox.conf");

	// if none found => parse userlevel conf
	if(!control->configfiles.size()) {
		config_file.clear();
		Cross::GetPlatformConfigName(config_file);
		control->ParseConfigFile((config_path + config_file).c_str());
	}

	if(!control->configfiles.size()) {
		//Try to create the userlevel configfile.
		config_file.clear();
		Cross::CreatePlatformConfigDir(config_path);
		Cross::GetPlatformConfigName(config_file);
		config_combined = config_path + config_file;
		if(control->PrintConfig(config_combined.c_str())) {
			LOG_MSG("CONFIG: Generating default configuration.\nWriting it to %s",config_combined.c_str());
			//Load them as well. Makes relative paths much easier
			control->ParseConfigFile(config_combined.c_str());
		} else {
			LOG_MSG("CONFIG: Using default settings. Create a configfile to change them");
		}
	}
}
