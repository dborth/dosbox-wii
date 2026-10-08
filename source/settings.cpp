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
	ROW_CYCLES_AMOUNT	//!< cpu.cycles: the number that goes with fixed or max
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

static const PageDef pages[SETTINGS_PAGE_COUNT] =
{
	{ "Performance", performanceRows, ARRAY_COUNT(performanceRows) },
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
