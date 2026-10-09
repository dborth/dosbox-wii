/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * settings.cpp
 *
 * The settings registry. Each page is a table of rows; a row names a DOSBox
 * property, says how it is shown and stepped, and when it may be changed.
 * Adding a page is adding a table and an entry in pages[]; the menu only
 * ever sees the text this file returns.
 *
 * Applying a change follows the path the DOS CONFIG command uses
 * (misc/programs.cpp, P_SETPROP): the section's changeable destroy
 * functions, the new value, then the section's changeable init functions.
 * Only sections registered as changeable in DOSBOX_Init() re-initialise, so
 * a row for any other section has to wait for a restart.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <string>
#include <vector>

#include "dosbox.h"
#include "setup.h"
#include "control.h"
#include "cpu.h"
#include "settings.h"
#include "dosboxsupport.h"
#include "menu.h"

/****************************************************************************
 * Row table types
 ***************************************************************************/
enum RowKind
{
	ROW_CHOICE,			//!< a string property: cycles through DOSBox's own list of values
	ROW_INT,			//!< an integer property: cycles through a list of presets
	ROW_CYCLES_MODE,	//!< cpu.cycles: auto / fixed / max
	ROW_CYCLES_AMOUNT,	//!< cpu.cycles: the number that goes with fixed or max
	ROW_BOOL,			//!< a bool property: On / Off
	ROW_RATE,			//!< a device's source sample rate: Fast / Matched / Native presets
	ROW_AUDIO_OUTPUT	//!< read-only: the audio driver's fixed output format
};

//! When a row may be changed. A row that has to wait for a restart will be a
//! third tier, once there is a page that needs one.
enum RowTier
{
	TIER_LIVE,			//!< any time the menu is open
	TIER_AT_PROMPT		//!< only at the DOS prompt, not while a program is running
};

enum IntFormat
{
	FMT_NUMBER,
	FMT_CYCLE_STEP		//!< below 100 is a percentage, otherwise a number of cycles
};

struct SettingRow
{
	const char * label;
	const char * section;
	const char * prop;
	RowKind kind;
	RowTier tier;
	const int * presets;			//!< ROW_INT: the values to cycle through, ascending
	int presetCount;
	IntFormat format;				//!< ROW_INT
	int (*effective)();				//!< ROW_INT: the live value where it can differ from the config; NULL = the config
	bool (*allowed)(const char *);	//!< ROW_CHOICE: refuses a value the core cannot take right now; NULL = any
	const char * help;				//!< NULL = DOSBox's own help for the property
	int nativeRate;					//!< ROW_RATE: a further preset, the device's own rate; 0 = none
};

struct PageDef
{
	const char * title;
	const SettingRow * rows;
	int count;
};

/****************************************************************************
 * Config access
 ***************************************************************************/
static Section_prop * GetSection(const char * name)
{
	if(!control)
		return NULL;

	return dynamic_cast<Section_prop *>(control->GetSection(name));
}

static Property * FindProp(Section_prop * sec, const char * name)
{
	Property * p;

	for(int i = 0; (p = sec->Get_prop(i)) != NULL; i++)
	{
		if(strcasecmp(p->propname.c_str(), name) == 0)
			return p;
	}
	return NULL;
}

//! The property's value as it stands in the config, or "" if there isn't one.
static std::string GetConfig(const char * section, const char * prop)
{
	Section_prop * sec = GetSection(section);

	if(!sec)
		return "";

	std::string value = sec->GetPropValue(prop);

	return (value == NO_SUCH_PROPERTY) ? std::string("") : value;
}

/**
 * Sets one property on the running core.
 *
 * Whatever a section's changeable init function reads is re-read, so this
 * has side effects beyond the property: changing any cpu property re-reads
 * cpu.cycles too, which puts the cycles back to the configured value and
 * drops what the home screen's +/- buttons had made of it. That is how the
 * DOS CONFIG command behaves as well.
 *
 * If DOSBox rejects the value the property falls back to its default. Every
 * value passed in comes from DOSBox's own list or a preset inside its range,
 * so that is not expected.
 *
 * @return true if DOSBox accepted the value
 */
static bool Apply(const char * section, const char * prop, const std::string & value)
{
	Section * sec = control ? control->GetSection(section) : NULL;

	if(!sec)
		return false;

	const std::string line = std::string(prop) + "=" + value;

	sec->ExecuteDestroy(false);
	const bool accepted = sec->HandleInputline(line);
	sec->ExecuteInit(false);

	return accepted;
}

static bool IsLocked(const SettingRow & row)
{
	return row.tier == TIER_AT_PROMPT && !IsShellIdle();
}

//! The next value in the list from current: the smallest above it going up,
//! the largest below it going down, wrapping at the ends.
static int StepPreset(const int * presets, int count, int current, int direction)
{
	if(direction >= 0)
	{
		for(int i = 0; i < count; i++)
		{
			if(presets[i] > current)
				return presets[i];
		}
		return presets[0];
	}

	for(int i = count - 1; i >= 0; i--)
	{
		if(presets[i] < current)
			return presets[i];
	}
	return presets[count - 1];
}

//! Whitespace runs, newlines included, become single spaces.
static void CollapseSpaces(const char * in, char * out, size_t size)
{
	size_t n = 0;
	bool pendingSpace = false;

	if(size == 0)
		return;

	for(; in && *in && n + 1 < size; in++)
	{
		if(*in == ' ' || *in == '\n' || *in == '\t' || *in == '\r')
		{
			pendingSpace = (n > 0);
			continue;
		}
		if(pendingSpace && n + 2 < size)
			out[n++] = ' ';
		pendingSpace = false;
		out[n++] = *in;
	}
	out[n] = 0;
}

/****************************************************************************
 * Audio output and source rates
 *
 * The mixer is fixed to the audio driver's rate (MIXER_Init). A device
 * (OPL, GUS, PC speaker, Tandy) whose own rate equals it is mixed as is;
 * any other rate makes MixerChannel::SetFreq turn interpolation on for
 * the channel (mixer.cpp). So the presets are built from the driver's rate
 * every time, never from a number written here.
 ***************************************************************************/
#define RATE_FAST	22050	//!< half-way down: cheaper to generate, resampled by the mixer

static bool OutputInfo(int * rate, int * frames)
{
	*rate = 0;
	*frames = 0;

	return GetEmulatorAudioInfo(rate, frames) && *rate > 0;
}

//! The rate the mixer runs at: the driver's, or the configured one when
//! there is no driver (nosound), which is what MIXER_Init then keeps.
static int OutputRate()
{
	int rate, frames;

	if(OutputInfo(&rate, &frames))
		return rate;

	Section_prop * mixer = GetSection("mixer");

	return mixer ? mixer->Get_int("rate") : 0;
}

static bool RateInValues(Property * p, int rate)
{
	char text[16];

	snprintf(text, sizeof(text), "%d", rate);

	const std::vector<Value> & values = p->GetValues();

	for(size_t i = 0; i < values.size(); i++)
	{
		if(values[i].ToString() == text)
			return true;
	}
	return false;
}

//! The presets for a rate row, ascending, without duplicates and without
//! any rate DOSBox would not accept for the property. Room for 3.
static int RatePresets(const SettingRow & row, int * out)
{
	Section_prop * sec = GetSection(row.section);
	Property * p = sec ? FindProp(sec, row.prop) : NULL;
	const int candidates[3] = { RATE_FAST, OutputRate(), row.nativeRate };
	int n = 0;

	if(!p)
		return 0;

	for(int c = 0; c < 3; c++)
	{
		const int rate = candidates[c];
		bool known = false;

		if(rate <= 0 || !RateInValues(p, rate))
			continue;

		for(int i = 0; i < n; i++)
			known = known || (out[i] == rate);

		if(known)
			continue;

		int at = n++;
		while(at > 0 && out[at - 1] > rate)
		{
			out[at] = out[at - 1];
			at--;
		}
		out[at] = rate;
	}
	return n;
}

static void FormatRate(const SettingRow & row, int rate, char * buf, size_t size)
{
	const char * name = NULL;

	if(rate == OutputRate())
		name = "Matched";
	else if(rate == RATE_FAST)
		name = "Fast";
	else if(row.nativeRate > 0 && rate == row.nativeRate)
		name = "Native";

	if(name)
		snprintf(buf, size, "%s (%d Hz)", name, rate);
	else
		snprintf(buf, size, "%d Hz", rate);
}

static void AudioOutputValue(char * buf, size_t size)
{
	int rate, frames;

	if(OutputInfo(&rate, &frames) && frames > 0)
		snprintf(buf, size, "%d Hz, %d frames (%d ms)", rate, frames, (frames * 1000) / rate);
	else if(rate > 0)
		snprintf(buf, size, "%d Hz", rate);
	else
		snprintf(buf, size, "No audio output"); // nosound: MIXER_Init found no driver
}

/****************************************************************************
 * cpu.cycles
 *
 * One multival property ("type" plus "parameters", see CPU::Change_Config)
 * shown as two rows: the mode, and the number that goes with it.
 ***************************************************************************/
enum CyclesMode { CYCLES_AUTO = 0, CYCLES_FIXED, CYCLES_MAX };

struct CyclesState
{
	CyclesMode mode;
	int amount;		//!< as configured: cycles for fixed, percent for max; 0 if not given
};

//! A fixed amount to start from when there is no number to carry over.
//! The same figure Change_Config falls back to.
#define CYCLES_FIXED_START	3000

static const int fixedCyclesPresets[] = { 500, 1000, 2000, 3000, 4000, 5000, 6000, 8000,
	10000, 12000, 15000, 20000, 25000, 30000, 40000, 50000 };
static const int maxPercentPresets[] = { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };

#define ARRAY_COUNT(a)	((int)(sizeof(a) / sizeof((a)[0])))

static CyclesState ReadCycles()
{
	CyclesState state = { CYCLES_AUTO, 0 };
	Section_prop * sec = GetSection("cpu");
	Prop_multival * cycles = sec ? sec->Get_multival("cycles") : NULL;

	if(!cycles)
		return state;

	const std::string type = cycles->GetSection()->Get_string("type");
	const std::string params = cycles->GetSection()->Get_string("parameters");

	if(type == "max")
	{
		state.mode = CYCLES_MAX;

		// the percentage is the parameter that ends in '%'
		size_t pos = 0;
		while(pos < params.size())
		{
			size_t end = params.find(' ', pos);
			if(end == std::string::npos)
				end = params.size();

			if(end > pos && params[end - 1] == '%')
				state.amount = atoi(params.substr(pos, end - pos).c_str());

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

//! The fixed cycle count the core is running, which the home screen's +/-
//! buttons change without touching the config.
static int EffectiveFixed(const CyclesState & state)
{
	if(!CPU_CycleAutoAdjust && MENU_CyclesDisplay > 0)
		return MENU_CyclesDisplay;

	return state.amount;
}

//! The same for max, which the +/- buttons run in steps of 5 percent.
static int EffectiveMaxPercent(const CyclesState & state)
{
	if(CPU_CycleAutoAdjust && MENU_CyclesDisplay > 0)
		return MENU_CyclesDisplay;

	return state.amount > 0 ? state.amount : 100;
}

static void CyclesModeValue(char * buf, size_t size)
{
	static const char * const names[] = { "Auto", "Fixed", "Max" };

	snprintf(buf, size, "%s", names[ReadCycles().mode]);
}

static void CyclesAmountValue(char * buf, size_t size)
{
	const CyclesState state = ReadCycles();

	if(state.mode == CYCLES_FIXED)
		snprintf(buf, size, "%d cycles", EffectiveFixed(state));
	else if(state.mode == CYCLES_MAX)
		snprintf(buf, size, "%d%%", EffectiveMaxPercent(state));
	else
		snprintf(buf, size, "-");
}

static bool StepCyclesMode(int direction)
{
	const CyclesState state = ReadCycles();
	const int count = 3;
	const CyclesMode next = (CyclesMode)((state.mode + (direction >= 0 ? 1 : count - 1)) % count);
	char value[32];

	if(next == CYCLES_FIXED)
	{
		// carry the current speed over when it is a cycle count; not when
		// it is a percentage of the host
		int start = (!CPU_CycleAutoAdjust && MENU_CyclesDisplay > 0) ? MENU_CyclesDisplay : CYCLES_FIXED_START;
		snprintf(value, sizeof(value), "fixed %d", start);
	}
	else if(next == CYCLES_MAX)
		snprintf(value, sizeof(value), "max");
	else
		snprintf(value, sizeof(value), "auto");

	return Apply("cpu", "cycles", value);
}

static bool StepCyclesAmount(int direction)
{
	const CyclesState state = ReadCycles();
	char value[32];

	if(state.mode == CYCLES_FIXED)
	{
		int next = StepPreset(fixedCyclesPresets, ARRAY_COUNT(fixedCyclesPresets),
			EffectiveFixed(state), direction);
		snprintf(value, sizeof(value), "fixed %d", next);
	}
	else if(state.mode == CYCLES_MAX)
	{
		int next = StepPreset(maxPercentPresets, ARRAY_COUNT(maxPercentPresets),
			EffectiveMaxPercent(state), direction);
		snprintf(value, sizeof(value), "max %d%%", next);
	}
	else
		return false; // auto has no amount

	return Apply("cpu", "cycles", value);
}

/****************************************************************************
 * cpu.core and cpu.cputype
 *
 * CPU::Change_Config calls E_Exit, which ends the program, when cputype is
 * 386_prefetch and the core is anything but normal or auto. Neither row may
 * ever produce that pair, so each refuses the values that would.
 ***************************************************************************/
static bool CoreAllowed(const char * candidate)
{
	if(GetConfig("cpu", "cputype") == "386_prefetch")
		return strcmp(candidate, "auto") == 0 || strcmp(candidate, "normal") == 0;

	return true;
}

static bool CpuTypeAllowed(const char * candidate)
{
	if(strcmp(candidate, "386_prefetch") == 0)
	{
		const std::string core = GetConfig("cpu", "core");
		return core == "auto" || core == "normal";
	}
	return true;
}

/****************************************************************************
 * speaker.tandy
 *
 * "on" on a machine that is not a Tandy or PCjr makes TANDYSOUND close the
 * second DMA controller (tandy_sound.cpp, CloseSecondDMAController), and
 * nothing opens it again until DOSBox restarts. Without it GetDMAChannel()
 * returns NULL for channels 4-7 and DMA_Write_Port dereferences it for a
 * write to page register 0x89, 0x8a or 0x8b (dma.cpp), and an SB16 can no
 * longer be set up. On a Tandy or PCjr "auto" already turns the sound on,
 * so "on" is only ever offered where it does no harm.
 ***************************************************************************/
static bool TandyAllowed(const char * candidate)
{
	if(strcmp(candidate, "on") != 0)
		return true;

	const std::string machine = GetConfig("dosbox", "machine");

	return machine == "tandy" || machine == "pcjr";
}

/****************************************************************************
 * render.frameskip
 *
 * The home screen's +/- buttons change the running frameskip without
 * touching the config, so that is the value to show. Writing the config
 * from here is safe while the display is suspended: RENDER_Init only resets
 * the screen when render.aspect changes, which this never does.
 ***************************************************************************/
static int EffectiveFrameskip()
{
	return MENU_FrameskipDisplay;
}

/****************************************************************************
 * Performance page
 ***************************************************************************/
static const int frameskipPresets[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };	// the property's range
static const int cycleStepPresets[] = { 5, 10, 20, 25, 50, 100, 250, 500, 1000, 2000, 5000 };

// DOSBox's own help for cpu.cycles runs to a paragraph and cycleup/cycledown
// talk about keyboard shortcuts, none of which fit or mean anything here
static const char * const helpCyclesMode =
	"Auto lets DOSBox guess what a game needs. Fixed runs a set number of cycles per "
	"millisecond. Max uses as much of the console as it can. Too high and sound drops out.";
static const char * const helpCyclesAmount =
	"Fixed: cycles per millisecond. Max: the share of the console DOSBox may use. "
	"Auto has no amount.";
static const char * const helpCycleStepUp =
	"How much the + button on the home screen adds in Fixed mode. "
	"Below 100 is a percentage, otherwise a number of cycles.";
static const char * const helpCycleStepDown =
	"How much the - button on the home screen takes off in Fixed mode. "
	"Below 100 is a percentage, otherwise a number of cycles.";

static const SettingRow performanceRows[] =
{
	{ "CPU core",			"cpu",		"core",			ROW_CHOICE,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, CoreAllowed, NULL },
	{ "CPU type",			"cpu",		"cputype",		ROW_CHOICE,			TIER_LIVE,		NULL, 0, FMT_NUMBER, NULL, CpuTypeAllowed, NULL },
	{ "Cycles mode",		"cpu",		"cycles",		ROW_CYCLES_MODE,	TIER_LIVE,		NULL, 0, FMT_NUMBER, NULL, NULL, helpCyclesMode },
	{ "Cycles amount",		"cpu",		"cycles",		ROW_CYCLES_AMOUNT,	TIER_LIVE,		NULL, 0, FMT_NUMBER, NULL, NULL, helpCyclesAmount },
	{ "Cycle step up",		"cpu",		"cycleup",		ROW_INT,			TIER_LIVE,		cycleStepPresets, ARRAY_COUNT(cycleStepPresets), FMT_CYCLE_STEP, NULL, NULL, helpCycleStepUp },
	{ "Cycle step down",	"cpu",		"cycledown",	ROW_INT,			TIER_LIVE,		cycleStepPresets, ARRAY_COUNT(cycleStepPresets), FMT_CYCLE_STEP, NULL, NULL, helpCycleStepDown },
	{ "Frameskip",			"render",	"frameskip",	ROW_INT,			TIER_LIVE,		frameskipPresets, ARRAY_COUNT(frameskipPresets), FMT_NUMBER, EffectiveFrameskip, NULL, NULL },
};

/****************************************************************************
 * Audio page
 *
 * Everything here re-initialises a whole section (sblaster, gus, speaker,
 * midi), which resets the hardware it emulates, so each row is for the DOS
 * prompt. The ports, IRQs and DMA channels are not on this page: they are
 * raw hex and numbers with no safe preset, and a change that collides with
 * another device is silent, so they are left to the all-settings page.
 ***************************************************************************/
static const char * const helpAudioOutput =
	"Fixed by the console's audio hardware, and DOSBox's mixer follows it. A device set to the same rate "
	"is not resampled.";
static const char * const helpSbType =
	"Which Sound Blaster DOSBox pretends to be. An SB16 needs a VGA machine type, otherwise DOSBox uses an SB Pro 2.";
static const char * const helpOplRate =
	"Matched skips resampling. Fast (22050 Hz) is cheaper to compute. Native (49716 Hz) is the OPL chip's own "
	"rate: the most accurate and the most work.";
static const char * const helpDeviceRate =
	"Matched skips resampling. Fast (22050 Hz) is cheaper to compute and is resampled by the mixer.";
static const char * const helpTandy =
	"auto turns Tandy sound on only for the tandy and pcjr machines. on is not offered on other machines: it "
	"removes the second DMA controller until DOSBox restarts.";
static const char * const helpDisney =
	"Disney Sound Source (and Covox) compatible sound.";
static const char * const helpMpu =
	"MIDI port for games. This build has no MIDI synthesizer, so a game that picks MIDI music plays nothing. "
	"none lets it fall back to the OPL.";

#define RATE_NATIVE_OPL	49716

static const SettingRow audioRows[] =
{
	{ "Output",			"mixer",	"rate",			ROW_AUDIO_OUTPUT,	TIER_LIVE,		NULL, 0, FMT_NUMBER, NULL, NULL, helpAudioOutput, 0 },
	{ "Sound Blaster",	"sblaster",	"sbtype",		ROW_CHOICE,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, NULL, helpSbType, 0 },
	{ "OPL mode",		"sblaster",	"oplmode",		ROW_CHOICE,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, NULL, NULL, 0 },
	{ "OPL emulator",	"sblaster",	"oplemu",		ROW_CHOICE,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, NULL, NULL, 0 },
	{ "OPL rate",		"sblaster",	"oplrate",		ROW_RATE,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, NULL, helpOplRate, RATE_NATIVE_OPL },
	{ "PC speaker",		"speaker",	"pcspeaker",	ROW_BOOL,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, NULL, NULL, 0 },
	{ "PC speaker rate","speaker",	"pcrate",		ROW_RATE,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, NULL, helpDeviceRate, 0 },
	{ "Tandy sound",	"speaker",	"tandy",		ROW_CHOICE,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, TandyAllowed, helpTandy, 0 },
	{ "Tandy rate",		"speaker",	"tandyrate",	ROW_RATE,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, NULL, helpDeviceRate, 0 },
	{ "Disney",			"speaker",	"disney",		ROW_BOOL,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, NULL, helpDisney, 0 },
	{ "Gravis Ultrasound","gus",	"gus",			ROW_BOOL,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, NULL, NULL, 0 },
	{ "GUS rate",		"gus",		"gusrate",		ROW_RATE,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, NULL, helpDeviceRate, 0 },
	{ "MPU-401",		"midi",		"mpu401",		ROW_CHOICE,			TIER_AT_PROMPT,	NULL, 0, FMT_NUMBER, NULL, NULL, helpMpu, 0 },
};

static const PageDef pages[SETTINGS_PAGE_COUNT] =
{
	{ "Performance", performanceRows, ARRAY_COUNT(performanceRows) },
	{ "Audio", audioRows, ARRAY_COUNT(audioRows) },
};

/****************************************************************************
 * Public interface
 ***************************************************************************/
static const SettingRow * GetRow(SettingsPage page, int row)
{
	if((int)page < 0 || (int)page >= SETTINGS_PAGE_COUNT)
		return NULL;

	if(row < 0 || row >= pages[page].count)
		return NULL;

	return &pages[page].rows[row];
}

const char * Settings_PageTitle(SettingsPage page)
{
	if((int)page < 0 || (int)page >= SETTINGS_PAGE_COUNT)
		return "";

	return pages[page].title;
}

int Settings_RowCount(SettingsPage page)
{
	if((int)page < 0 || (int)page >= SETTINGS_PAGE_COUNT)
		return 0;

	return pages[page].count;
}

const char * Settings_RowLabel(SettingsPage page, int row)
{
	const SettingRow * r = GetRow(page, row);

	return r ? r->label : "";
}

static void FormatInt(const SettingRow & row, int value, char * buf, size_t size)
{
	if(row.format == FMT_CYCLE_STEP)
	{
		if(value < 100)
			snprintf(buf, size, "%d%%", value);
		else
			snprintf(buf, size, "%d cycles", value);
	}
	else
		snprintf(buf, size, "%d", value);
}

static int CurrentInt(const SettingRow & row)
{
	if(row.effective)
		return row.effective();

	Section_prop * sec = GetSection(row.section);

	return sec ? sec->Get_int(row.prop) : 0;
}

void Settings_RowValue(SettingsPage page, int row, char * buf, size_t size)
{
	const SettingRow * r = GetRow(page, row);

	if(size == 0)
		return;

	buf[0] = 0;

	if(!r)
		return;

	switch(r->kind)
	{
		case ROW_CHOICE:
			snprintf(buf, size, "%s", GetConfig(r->section, r->prop).c_str());
			break;

		case ROW_INT:
			FormatInt(*r, CurrentInt(*r), buf, size);
			break;

		case ROW_CYCLES_MODE:
			CyclesModeValue(buf, size);
			break;

		case ROW_CYCLES_AMOUNT:
			CyclesAmountValue(buf, size);
			break;

		case ROW_BOOL:
		{
			Section_prop * sec = GetSection(r->section);

			snprintf(buf, size, "%s", (sec && sec->Get_bool(r->prop)) ? "On" : "Off");
			break;
		}

		case ROW_RATE:
			FormatRate(*r, CurrentInt(*r), buf, size);
			break;

		case ROW_AUDIO_OUTPUT:
			AudioOutputValue(buf, size);
			break;
	}

	if(IsLocked(*r))
	{
		const size_t used = strlen(buf);
		snprintf(buf + used, size - used, " (DOS prompt only)");
	}
}

void Settings_RowHelp(SettingsPage page, int row, char * buf, size_t size)
{
	const SettingRow * r = GetRow(page, row);
	char help[256];

	if(size == 0)
		return;

	buf[0] = 0;

	if(!r)
		return;

	help[0] = 0;

	if(r->help)
		CollapseSpaces(r->help, help, sizeof(help));
	else
	{
		Section_prop * sec = GetSection(r->section);
		Property * p = sec ? FindProp(sec, r->prop) : NULL;

		if(p)
			CollapseSpaces(p->Get_help(), help, sizeof(help));
	}

	snprintf(buf, size, "%s%s", IsLocked(*r) ? "Only available at the DOS prompt. " : "", help);
}

bool Settings_RowStep(SettingsPage page, int row, int direction)
{
	const SettingRow * r = GetRow(page, row);

	if(!r || IsLocked(*r))
		return false;

	switch(r->kind)
	{
		case ROW_CYCLES_MODE:
			return StepCyclesMode(direction);

		case ROW_CYCLES_AMOUNT:
			return StepCyclesAmount(direction);

		case ROW_AUDIO_OUTPUT:
			return false; // read-only

		case ROW_BOOL:
		{
			Section_prop * sec = GetSection(r->section);

			if(!sec)
				return false;

			return Apply(r->section, r->prop, sec->Get_bool(r->prop) ? "false" : "true");
		}

		case ROW_RATE:
		{
			int presets[3];
			const int count = RatePresets(*r, presets);
			const int current = CurrentInt(*r);

			if(count == 0)
				return false;

			const int next = StepPreset(presets, count, current, direction);
			char value[16];

			if(next == current)
				return false;

			snprintf(value, sizeof(value), "%d", next);
			return Apply(r->section, r->prop, value);
		}

		case ROW_INT:
		{
			const int current = CurrentInt(*r);
			const int next = StepPreset(r->presets, r->presetCount, current, direction);
			char value[16];

			if(next == current)
				return false;

			snprintf(value, sizeof(value), "%d", next);
			return Apply(r->section, r->prop, value);
		}

		case ROW_CHOICE:
		{
			Section_prop * sec = GetSection(r->section);
			Property * p = sec ? FindProp(sec, r->prop) : NULL;

			if(!p)
				return false;

			const std::vector<Value> & values = p->GetValues();
			const std::string current = GetConfig(r->section, r->prop);
			const int count = (int)values.size();
			const int step = (direction >= 0) ? 1 : -1;
			int index = -1;

			for(int i = 0; i < count; i++)
			{
				if(values[i].ToString() == current)
					index = i;
			}

			// walk the list from the current value, taking the first one the
			// core can accept; a full lap with nothing means there is no other
			for(int k = 1; k <= count; k++)
			{
				const int i = (index < 0) ? (step > 0 ? k - 1 : count - k)
					: (((index + step * k) % count) + count) % count;
				const std::string candidate = values[i].ToString();

				if(candidate == current)
					continue;

				if(r->allowed && !r->allowed(candidate.c_str()))
					continue;

				return Apply(r->section, r->prop, candidate);
			}
			return false;
		}
	}

	return false;
}
