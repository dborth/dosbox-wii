/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * preferences.cpp
 *
 * Preferences: everything about dosbox.conf.
 ***************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
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
#include "videosupport.h"

#define MAPPERFILE "mapper.map"

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

	Pbool = sdl_sec->Add_bool("autolock",Property::Changeable::Always,true);
	Pbool->Set_help("Ignored on this build: the mouse is always captured, so the first click is not used up capturing it.");

	Pmulti = sdl_sec->Add_multi("sensitivity",Property::Changeable::Always, ",");
	Pmulti->Set_help("Mouse sensitivity. The optional second parameter specifies vertical sensitivity (e.g. 100,-50).");
	Pmulti->SetValue("100");
	Pint = Pmulti->GetSection()->Add_int("xsens",Property::Changeable::Always,100);
	Pint->SetMinMax(-1000,1000);
	Pint = Pmulti->GetSection()->Add_int("ysens",Property::Changeable::Always,100);
	Pint->SetMinMax(-1000,1000);

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
 * if there is none. Then the game's conf (-gameconf, see SetGameConf()), which
 * is layered over all of that.
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
	// The game's conf is read after the base config below, so it only has to
	// hold what differs from it
	std::string game_file;
	const bool have_game = control->cmdline->FindString("-gameconf",game_file,true);

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

	// Last, so its values win over everything above. Tried as given, then in
	// the DOSBox folder, like -conf.
	if(have_game && !game_file.empty()) {
		if (!control->ParseConfigFile(game_file.c_str())) {
			if (!control->ParseConfigFile((config_path + game_file).c_str())) {
				LOG_MSG("CONFIG: Can't open game config file: %s",game_file.c_str());
			}
		}
	}
}

/****************************************************************************
 * Saving the config
 *
 * Writing the running config back to dosbox.conf, without ever leaving the
 * player with a half-written or missing file. Plain stdio and DOSBox's own
 * config.
 ***************************************************************************/

static bool Exists(const std::string & path, off_t * size = NULL)
{
	struct stat st;

	if(stat(path.c_str(), &st) != 0)
		return false;

	if(size)
		*size = st.st_size;

	return true;
}

//! The whole file, or false if it cannot be read
static bool ReadAll(const std::string & path, std::string * out)
{
	FILE * f = fopen(path.c_str(), "rb");
	char buf[1024];
	size_t n;

	if(!f)
		return false;

	out->clear();
	while((n = fread(buf, 1, sizeof(buf), f)) > 0)
		out->append(buf, n);

	fclose(f);
	return true;
}

/**
 * PrintConfig() ignores write errors (a full card makes it stop part way and
 * still return true), so what it left is looked at before it replaces
 * anything: it must be there and not empty, and the header of the last
 * section, which is written last, must be in it. That catches a card that
 * filled up; it cannot prove a file complete.
 */
static bool LooksComplete(const std::string & path)
{
	off_t size = 0;
	std::string text;

	if(!Exists(path, &size) || size <= 0 || !ReadAll(path, &text))
		return false;

	int last = 0;

	while(control->GetSection(last + 1) != NULL)
		last++;

	Section * section = control->GetSection(last);

	if(!section)
		return false;

	std::string name = section->GetName();

	for(size_t i = 0; i < name.size(); i++)
		name[i] = (char)tolower((unsigned char)name[i]);	// PrintConfig writes it lower case

	return text.find("[" + name + "]\n") != std::string::npos;
}

std::string ConfigSavePath()
{
	if(control && !control->configfiles.empty())
		return control->configfiles[0];

	std::string dir, name;

	Cross::CreatePlatformConfigDir(dir);	// creates the folder; ends with a separator
	Cross::GetPlatformConfigName(name);
	return dir + name;
}

bool ConfigSave(const char * path, std::string * error)
{
	std::string ignored;

	if(!error)
		error = &ignored;

	if(!control || !path || !*path)
	{
		*error = "There is no config to save.";
		return false;
	}

	const std::string target(path);
	const std::string tmp = target + ".tmp";
	const std::string bak = target + ".bak";

	remove(tmp.c_str());	// left by a save that was cut short

	if(!control->PrintConfig(tmp.c_str()))
	{
		*error = "Could not write " + tmp + ".";
		return false;
	}

	if(!LooksComplete(tmp))
	{
		remove(tmp.c_str());
		*error = "The new file was not written in full. Is the card full?";
		return false;
	}

	if(Exists(target))
	{
		remove(bak.c_str());

		if(Exists(bak) || rename(target.c_str(), bak.c_str()) != 0)
		{
			remove(tmp.c_str());
			*error = "Could not keep the old file as " + bak + ". Nothing was changed.";
			return false;
		}
	}

	if(rename(tmp.c_str(), target.c_str()) != 0)
	{
		const std::string why = strerror(errno);

		if(!Exists(target) && Exists(bak))
			rename(bak.c_str(), target.c_str());	// put the old one back

		remove(tmp.c_str());
		*error = "Could not put the new file in place (" + why + ").";
		return false;
	}

	return true;
}

/****************************************************************************
 * The [display] section
 ***************************************************************************/

//! A ratio of the picture as the whole-number percentage the config holds
static int ToPercent(float ratio)
{
	return (int)(ratio * 100.0f + 0.5f);
}

/****************************************************************************
 * Config_Add_Display
 *
 * Every default is EmulatorVideoSettings' own, and the limits are its own
 * limits, so a value the config accepts is one the driver accepts too.
 *
 * Replaces [render] aspect, which no longer exists: whether the source's
 * pixel aspect is used is a display choice now (see RENDER_Reset). A
 * dosbox.conf that still has it in [render] is read without complaint and
 * the line is ignored. It could not be honoured: the default config DOSBox
 * generated wrote aspect=false into every one of them.
 ***************************************************************************/
void Config_Add_Display()
{
	const EmulatorVideoSettings defaults;

	Section_prop * display_sec = control->AddSection_prop("display", &GFX_DisplayInit, true);
	Prop_string * Pstring;
	Prop_int * Pint;

	const char * fits[] = { "fit", "integer", "fill", 0 };
	Pstring = display_sec->Add_string("fit", Property::Changeable::Always, "fit");
	Pstring->Set_values(fits);
	Pstring->Set_help("How big the picture is. fit: as large as fits, keeping its shape. "
		"integer: the largest whole-number multiple that fits, which keeps pixels even. "
		"fill: stretched over the whole screen.");

	const char * aspects[] = { "corrected", "square", 0 };
	Pstring = display_sec->Add_string("aspect", Property::Changeable::Always, "corrected");
	Pstring->Set_values(aspects);
	Pstring->Set_help("corrected: shows the picture in the shape a monitor did, so 320x200 fills a 4:3 screen. "
		"square: every pixel is square.");

	const char * filters[] = { "sharp", "bilinear", "nearest", 0 };
	Pstring = display_sec->Add_string("filter", Property::Changeable::Always, "bilinear");
	Pstring->Set_values(filters);
	Pstring->Set_help("How the picture is smoothed when it is enlarged. sharp: crisp pixels with soft edges between them. "
		"bilinear: smooth. nearest: no blending.");

	Pint = display_sec->Add_int("scanlines", Property::Changeable::Always, ToPercent(defaults.scanlines));
	Pint->SetMinMax(0, 100);
	Pint->Set_help("How dark the gaps between lines are, in percent. 0 is off.");

	Pint = display_sec->Add_int("zoomx", Property::Changeable::Always, ToPercent(defaults.zoomX));
	Pint->SetMinMax(ToPercent(EmulatorVideoSettings::ZOOM_MIN), ToPercent(EmulatorVideoSettings::ZOOM_MAX));
	Pint->Set_help("Width of the picture in percent of its normal size. Used by the fit and fill sizes. 100 is normal.");

	Pint = display_sec->Add_int("zoomy", Property::Changeable::Always, ToPercent(defaults.zoomY));
	Pint->SetMinMax(ToPercent(EmulatorVideoSettings::ZOOM_MIN), ToPercent(EmulatorVideoSettings::ZOOM_MAX));
	Pint->Set_help("Height of the picture in percent of its normal size. Used by the fit and fill sizes. 100 is normal.");

	Pint = display_sec->Add_int("shiftx", Property::Changeable::Always, defaults.shiftX);
	Pint->SetMinMax(EmulatorVideoSettings::SHIFT_MIN, EmulatorVideoSettings::SHIFT_MAX);
	Pint->Set_help("Moves the picture right (negative: left). One unit is 1/640 of the screen width.");

	Pint = display_sec->Add_int("shifty", Property::Changeable::Always, defaults.shiftY);
	Pint->SetMinMax(EmulatorVideoSettings::SHIFT_MIN, EmulatorVideoSettings::SHIFT_MAX);
	Pint->Set_help("Moves the picture down (negative: up). One unit is 1/480 of the screen height.");

	const char * widescreens[] = { "auto", "4x3", "16x9", 0 };
	Pstring = display_sec->Add_string("widescreen", Property::Changeable::Always, "auto");
	Pstring->Set_values(widescreens);
	Pstring->Set_help("For consoles that cannot tell what shape the TV is. auto follows the console's own aspect ratio setting. "
		"4x3 and 16x9 say which it is, so the picture is not stretched on a widescreen TV.");
}

EmulatorVideoSettings DisplaySettingsFromConfig(Section_prop * section)
{
	EmulatorVideoSettings out;

	const std::string fit = section->Get_string("fit");
	out.fit = (fit == "integer") ? VideoFit::Integer : (fit == "fill") ? VideoFit::Fill : VideoFit::Fit;

	// Get_string() gives a const char *, so it is copied into a string before it is compared
	const std::string aspect = section->Get_string("aspect");
	out.aspect = (aspect == "square") ? VideoAspect::Square : VideoAspect::Corrected;

	const std::string filter = section->Get_string("filter");
	out.filter = (filter == "sharp") ? VideoFilter::Sharp : (filter == "nearest") ? VideoFilter::Nearest : VideoFilter::Bilinear;

	const std::string widescreen = section->Get_string("widescreen");
	out.widescreen = (widescreen == "4x3") ? VideoWidescreen::Display4x3
		: (widescreen == "16x9") ? VideoWidescreen::Display16x9 : VideoWidescreen::Auto;

	out.scanlines = section->Get_int("scanlines") / 100.0f;
	out.zoomX = section->Get_int("zoomx") / 100.0f;
	out.zoomY = section->Get_int("zoomy") / 100.0f;
	out.shiftX = section->Get_int("shiftx");
	out.shiftY = section->Get_int("shifty");

	out.clamp();
	return out;
}


/****************************************************************************
 * Reading the running config
 ***************************************************************************/
Section_prop * Config_GetSection(const char * name)
{
	if(!control)
		return NULL;

	return dynamic_cast<Section_prop *>(control->GetSection(name));
}

Property * Config_FindProp(Section_prop * sec, const char * name)
{
	Property * p;

	for(int i = 0; sec && (p = sec->Get_prop(i)) != NULL; i++)
	{
		if(strcasecmp(p->propname.c_str(), name) == 0)
			return p;
	}
	return NULL;
}

std::string Config_Get(const char * section, const char * prop)
{
	Section_prop * sec = Config_GetSection(section);

	if(!sec)
		return "";

	const std::string value = sec->GetPropValue(prop);

	return (value == NO_SUCH_PROPERTY) ? std::string("") : value;
}

CyclesConfig Config_ReadCycles()
{
	CyclesConfig state = { CYCLES_AUTO, 0, false };
	Section_prop * sec = Config_GetSection("cpu");
	Prop_multival * cycles = sec ? sec->Get_multival("cycles") : NULL;

	if(!cycles)
		return state;

	const std::string type = cycles->GetSection()->Get_string("type");
	const std::string params = cycles->GetSection()->Get_string("parameters");

	if(type == "max")
	{
		state.mode = CYCLES_MAX;

		// the percentage is the parameter that ends in '%'; any other
		// parameter is a cycle limit
		size_t pos = 0;
		while(pos < params.size())
		{
			size_t end = params.find(' ', pos);
			if(end == std::string::npos)
				end = params.size();

			if(end > pos)
			{
				if(params[end - 1] == '%')
					state.amount = atoi(params.substr(pos, end - pos).c_str());
				else
					state.limited = true;
			}

			pos = end + 1;
		}
	}
	else if(type == "fixed")
	{
		state.mode = CYCLES_FIXED;
		state.amount = atoi(params.c_str());
	}
	else if(atoi(type.c_str()) > 0)
	{
		// "cycles = 4000": a bare number is a fixed amount
		state.mode = CYCLES_FIXED;
		state.amount = atoi(type.c_str());
	}

	return state;
}
