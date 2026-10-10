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
#include "configsave.h"
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
	ROW_AUDIO_OUTPUT,	//!< read-only: the audio driver's fixed output format
	ROW_RANGE			//!< an integer property stepped through its own SetMinMax range, by a fixed amount
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
	FMT_CYCLE_STEP,		//!< below 100 is a percentage, otherwise a number of cycles
	FMT_PERCENT,		//!< 80 shows as 80%
	FMT_SIGNED			//!< a position: +12, 0, -12
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
	int step;						//!< ROW_RANGE: how far one press moves the value
	bool (*shown)();				//!< NULL = always; false leaves the row off the page (the platform cannot do it)
	const char * (*inactive)();		//!< NULL = never; why the setting has no effect right now, if it has none
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
 * sblaster and gus: clashes
 *
 * DOSBox does not check that two devices ask for the same resource. Handlers
 * installed on the same I/O port overwrite each other (and the loser's port
 * goes dead when either is removed), and two devices on one IRQ or DMA
 * channel confuse each other. So a step to a value that would start a clash
 * is skipped. A clash that is already there (from dosbox.conf) does not stop
 * the rows that have nothing to do with it.
 *
 * What each device claims, from the constructors:
 *   SB / OPL (sblaster.cpp, adlib.cpp): sbbase .. sbbase+0xf, and 0x388.
 *   GUS (gus.cpp): gusbase+0x0, 0x6, 0x8-0xb and gusbase+0x102-0x107.
 *     With gusbase 2c0 that is 3c2-3c7, which VGA owns (vga_misc.cpp,
 *     vga_seq.cpp, vga_dac.cpp). The bases in the lists are 0x20 apart, so
 *     two devices only meet when their bases are equal.
 * The SB16's 8 bit and 16 bit DMA channels must differ as well.
 *
 * Not checked, because nothing here was read for it: IRQs against the PS/2
 * mouse (12) or the dummy serial ports (3, 4).
 ***************************************************************************/
enum { CLASH_PORT = 1, CLASH_IRQ = 2, CLASH_DMA = 4, CLASH_VGA = 8, CLASH_SBDMA = 16 };

struct Resources
{
	bool sbOn, sb16, gusOn;
	int sbBase, sbIrq, sbDma, sbHdma;
	int gusBase, gusIrq, gusDma;
};

//! The value of section.prop with the candidate (if any) in place of the config's.
static std::string ValueWith(const char * section, const char * prop,
	const char * candSection, const char * candProp, const std::string * cand)
{
	if(cand && !strcasecmp(section, candSection) && !strcasecmp(prop, candProp))
		return *cand;

	return GetConfig(section, prop);
}

static Resources ReadResources(const char * candSection, const char * candProp, const std::string * cand)
{
	Resources r;
	const std::string type = ValueWith("sblaster", "sbtype", candSection, candProp, cand);

	r.sbOn = (type != "none" && type != "gb" && type != "");
	r.sb16 = (type == "sb16");
	r.gusOn = (ValueWith("gus", "gus", candSection, candProp, cand) == "true");
	r.sbBase = (int)strtol(ValueWith("sblaster", "sbbase", candSection, candProp, cand).c_str(), NULL, 16);
	r.sbIrq = atoi(ValueWith("sblaster", "irq", candSection, candProp, cand).c_str());
	r.sbDma = atoi(ValueWith("sblaster", "dma", candSection, candProp, cand).c_str());
	r.sbHdma = atoi(ValueWith("sblaster", "hdma", candSection, candProp, cand).c_str());
	r.gusBase = (int)strtol(ValueWith("gus", "gusbase", candSection, candProp, cand).c_str(), NULL, 16);
	r.gusIrq = atoi(ValueWith("gus", "gusirq", candSection, candProp, cand).c_str());
	r.gusDma = atoi(ValueWith("gus", "gusdma", candSection, candProp, cand).c_str());
	return r;
}

static int Clashes(const Resources & r)
{
	int bits = 0;

	if(r.gusOn)
	{
		if(r.gusBase == 0x2c0)
			bits |= CLASH_VGA;

		if(r.sbOn)
		{
			if(r.sbBase == r.gusBase)
				bits |= CLASH_PORT;
			if(r.sbIrq == r.gusIrq)
				bits |= CLASH_IRQ;
			if(r.gusDma == r.sbDma || (r.sb16 && r.gusDma == r.sbHdma))
				bits |= CLASH_DMA;
		}
	}

	if(r.sbOn && r.sb16 && r.sbDma == r.sbHdma)
		bits |= CLASH_SBDMA;

	return bits;
}

static const char * ClashReason(int bits)
{
	if(bits & CLASH_VGA)
		return "A Gravis Ultrasound at 2c0 would take over the VGA registers.";
	if(bits & CLASH_PORT)
		return "The Sound Blaster and the Gravis Ultrasound would share a port address.";
	if(bits & CLASH_IRQ)
		return "The Sound Blaster and the Gravis Ultrasound would share an IRQ.";
	if(bits & CLASH_DMA)
		return "The Sound Blaster and the Gravis Ultrasound would share a DMA channel.";
	if(bits & CLASH_SBDMA)
		return "The Sound Blaster's 8 bit and 16 bit DMA would be the same channel.";
	return NULL;
}

static bool IsResourceProp(const char * section, const char * prop)
{
	static const char * const sbProps[] = { "sbtype", "sbbase", "irq", "dma", "hdma", NULL };
	static const char * const gusProps[] = { "gus", "gusbase", "gusirq", "gusdma", NULL };
	const char * const * list = !strcasecmp(section, "sblaster") ? sbProps
		: (!strcasecmp(section, "gus") ? gusProps : NULL);

	for(int i = 0; list && list[i]; i++)
	{
		if(!strcasecmp(list[i], prop))
			return true;
	}
	return false;
}

/****************************************************************************
 * Refusal
 *
 * The one place that says whether a value may be written. Every row, on the
 * curated pages and in the all-settings editor, asks it before Apply(), so
 * the two cannot disagree about which combinations are safe. Returns why the
 * value is refused, or NULL.
 ***************************************************************************/
static const char * Refusal(const char * section, const char * prop, const std::string & candidate)
{
	if(!strcasecmp(section, "cpu"))
	{
		if(!strcasecmp(prop, "core") && !CoreAllowed(candidate.c_str()))
			return "The 386_prefetch CPU type only works with the normal core.";
		if(!strcasecmp(prop, "cputype") && !CpuTypeAllowed(candidate.c_str()))
			return "386_prefetch needs the normal or auto core.";
	}
	else if(!strcasecmp(section, "speaker") && !strcasecmp(prop, "tandy"))
	{
		if(!TandyAllowed(candidate.c_str()))
			return "Tandy sound can only be forced on for the tandy and pcjr machines.";
	}
	else if(IsResourceProp(section, prop))
	{
		const int now = Clashes(ReadResources(NULL, NULL, NULL));
		const int then = Clashes(ReadResources(section, prop, &candidate));

		return ClashReason(then & ~now);
	}
	else if(!strcasecmp(section, "dos"))
	{
		const char * why = NULL;

		if(!CanReinitDosSection(&why))
			return why;
	}
	return NULL;
}

/****************************************************************************
 * render.frameskip
 *
 * The home screen's +/- buttons change the running frameskip without
 * touching the config, so that is the value to show. Writing the config
 * from here is safe while the display is suspended: RENDER_Init only reads
 * frameskip and does not reset the screen.
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
 * Video page
 *
 * The [display] section (displayconfig.cpp), which the video driver reads on
 * its next frame. Nothing here touches the screen, so every row is safe at
 * any time the menu is open. The driver says what it can do, and a row it
 * cannot honour is left off: scanlines need a 480 line mode, and the
 * widescreen row is for consoles that cannot tell what shape the TV is.
 ***************************************************************************/
static bool ScanlinesShown()
{
	bool scanlines = false;

	GetEmulatorVideoCapabilities(&scanlines, NULL, NULL);
	return scanlines;
}

static bool WidescreenShown()
{
	bool widescreen = false;

	GetEmulatorVideoCapabilities(NULL, NULL, &widescreen);
	return widescreen;
}

//! "sharp" is only offered where the driver has it
static bool FilterAllowed(const char * candidate)
{
	bool sharp = false;

	if(strcmp(candidate, "sharp") != 0)
		return true;

	GetEmulatorVideoCapabilities(NULL, &sharp, NULL);
	return sharp;
}

//! The whole-number fit picks its own size, so the zoom has nothing to do
static const char * ZoomInactive()
{
	return (GetConfig("display", "fit") == "integer") ? "Zoom is not used with the integer fit." : NULL;
}

#define SHIFT_STEP	5	//!< of 1/640 of the width, 1/480 of the height
#define ZOOM_STEP	5	//!< percent

static const SettingRow videoRows[] =
{
	{ "Aspect",			"display",	"aspect",		ROW_CHOICE,			TIER_LIVE,		NULL, 0, FMT_NUMBER, NULL, NULL, NULL, 0, 0, NULL, NULL },
	{ "Fit",			"display",	"fit",			ROW_CHOICE,			TIER_LIVE,		NULL, 0, FMT_NUMBER, NULL, NULL, NULL, 0, 0, NULL, NULL },
	{ "Filter",			"display",	"filter",		ROW_CHOICE,			TIER_LIVE,		NULL, 0, FMT_NUMBER, NULL, FilterAllowed, NULL, 0, 0, NULL, NULL },
	{ "Scanlines",		"display",	"scanlines",	ROW_RANGE,			TIER_LIVE,		NULL, 0, FMT_PERCENT, NULL, NULL, NULL, 0, 10, ScanlinesShown, NULL },
	{ "Zoom width",		"display",	"zoomx",		ROW_RANGE,			TIER_LIVE,		NULL, 0, FMT_PERCENT, NULL, NULL, NULL, 0, ZOOM_STEP, NULL, ZoomInactive },
	{ "Zoom height",	"display",	"zoomy",		ROW_RANGE,			TIER_LIVE,		NULL, 0, FMT_PERCENT, NULL, NULL, NULL, 0, ZOOM_STEP, NULL, ZoomInactive },
	{ "Shift X",		"display",	"shiftx",		ROW_RANGE,			TIER_LIVE,		NULL, 0, FMT_SIGNED, NULL, NULL, NULL, 0, SHIFT_STEP, NULL, NULL },
	{ "Shift Y",		"display",	"shifty",		ROW_RANGE,			TIER_LIVE,		NULL, 0, FMT_SIGNED, NULL, NULL, NULL, 0, SHIFT_STEP, NULL, NULL },
	{ "Widescreen",		"display",	"widescreen",	ROW_CHOICE,			TIER_LIVE,		NULL, 0, FMT_NUMBER, NULL, NULL, NULL, 0, 0, WidescreenShown, NULL },
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
	{ "Video", videoRows, ARRAY_COUNT(videoRows) },
	{ "Audio", audioRows, ARRAY_COUNT(audioRows) },
};

/****************************************************************************
 * Public interface
 ***************************************************************************/
static bool RowShown(const SettingRow & row)
{
	return !row.shown || row.shown();
}

//! The row at a position among the rows that are shown
static const SettingRow * GetRow(int page, int row)
{
	if((int)page < 0 || (int)page >= SETTINGS_PAGE_COUNT || row < 0)
		return NULL;

	for(int i = 0; i < pages[page].count; i++)
	{
		if(RowShown(pages[page].rows[i]) && row-- == 0)
			return &pages[page].rows[i];
	}
	return NULL;
}

static void CuratedValue(const SettingRow * r, char * buf, size_t size);
static void CuratedHelp(const SettingRow * r, char * buf, size_t size);
static bool CuratedStep(const SettingRow * r, int direction);

/****************************************************************************
 * All settings: a reflective editor over Section_prop
 *
 * One page per config section, one row per property, built from the property
 * itself every time it is asked for. A property that has a curated row is
 * handed to that row, so there is one definition of how it is shown, stepped
 * and guarded. The rest are shown by what DOSBox says they are:
 *
 *   bool                        toggles
 *   string / int / hex with a list of values (Set_values)   cycles the list
 *   int with a range of at most 100 (SetMinMax)             steps by 1 or 5
 *   everything else (free text, paths, multival)            read-only
 *
 * There is no free-text entry. Each change goes through Apply() and
 * Refusal() like every other row.
 *
 * Whether a change takes effect now is decided by the section, not the
 * property: Section::ExecuteInit(false) only runs the init functions that
 * DOSBOX_Init() registered with canchange=true (render, cpu, midi, sblaster,
 * gus, speaker, serial, display, and the JOYSTICK_Init, XMS_Init, EMS_Init and
 * DOS_KeyboardLayout_Init of joystick and dos), and every property of those
 * sections is read by one of them. A property of any other section (dosbox,
 * mixer, sdl) is read once at start. Those rows are read-only here and say
 * "next launch": the in-memory config would change but nothing would read it,
 * and nothing saves it to dosbox.conf yet. The list of live sections below
 * has to follow dosbox.cpp; the host test checks it against the real
 * registrations.
 ***************************************************************************/
struct PropRef
{
	const char * section;
	const char * prop;
};

//! Not shown. Each one is dead or not ours to set on this build:
static const PropRef hiddenProps[] =
{
	{ "mixer", "rate" },			// the audio driver fixes it (MIXER_Init)
	{ "mixer", "blocksize" },		// the same
	{ "midi", "mididevice" },		// no handler that opens: nothing to choose
	{ "midi", "midiconfig" },		// the options of one
	{ "sdl", "fullscreen" },		// nothing reads these six
	{ "sdl", "fulldouble" },
	{ "sdl", "fullresolution" },
	{ "sdl", "windowresolution" },
	{ "sdl", "output" },
	{ "sdl", "waitonerror" },
	{ "sdl", "priority" },			// only read when an SDL focus event arrives, and nothing sends one
};

//! Not shown: only dummy and disabled exist on this build (C_MODEM and
//! C_DIRECTSERIAL are 0), and a console has no serial port to connect.
static const char * const hiddenSections[] = { "serial" };

//! Sections whose changes reach the running core (see above).
static const char * const liveSections[] =
{
	"render", "display", "cpu", "midi", "sblaster", "gus", "speaker", "joystick", "dos"
};

//! Of those, the ones that are safe at any time. The rest wait for the DOS prompt.
static const char * const anytimeSections[] = { "render", "display", "cpu" };

struct SectionTitle
{
	const char * name;
	const char * title;
};

static const SectionTitle sectionTitles[] =
{
	{ "dosbox", "Machine and memory" },
	{ "render", "Rendering" },
	{ "display", "Video output" },
	{ "cpu", "CPU" },
	{ "mixer", "Mixer" },
	{ "midi", "MIDI" },
	{ "sblaster", "Sound Blaster" },
	{ "gus", "Gravis Ultrasound" },
	{ "speaker", "PC speaker, Tandy, Disney" },
	{ "joystick", "Joystick" },
	{ "dos", "DOS memory and keyboard" },
	{ "sdl", "Mouse and mapper" },
};

enum AllKind { ALL_DELEGATE, ALL_BOOL, ALL_CHOICE, ALL_RANGE, ALL_READONLY };

struct AllRow
{
	Section_prop * sec;
	Property * prop;
	const SettingRow * curated;	//!< ALL_DELEGATE: the row that owns this property
	AllKind kind;
	bool live;
	bool atPrompt;
	const char * note;			//!< ALL_READONLY: where to change it
};

static bool InList(const char * const * list, int count, const char * name)
{
	for(int i = 0; i < count; i++)
	{
		if(!strcasecmp(list[i], name))
			return true;
	}
	return false;
}

static bool IsHidden(const char * section, const char * prop)
{
	for(int i = 0; i < ARRAY_COUNT(hiddenProps); i++)
	{
		if(!strcasecmp(hiddenProps[i].section, section) && !strcasecmp(hiddenProps[i].prop, prop))
			return true;
	}
	return false;
}

static const SettingRow * FindCuratedRow(const char * section, const char * prop)
{
	// cpu.cycles has two rows (mode and amount); neither is the property
	if(!strcasecmp(section, "cpu") && !strcasecmp(prop, "cycles"))
		return NULL;

	for(int p = 0; p < SETTINGS_PAGE_COUNT; p++)
	{
		for(int i = 0; i < pages[p].count; i++)
		{
			const SettingRow & r = pages[p].rows[i];

			if(!strcasecmp(r.section, section) && !strcasecmp(r.prop, prop))
				return &r;
		}
	}
	return NULL;
}

static AllKind KindOf(Property * p, const char ** note)
{
	*note = "edit dosbox.conf";

	if(dynamic_cast<Prop_multival *>(p))
		return ALL_READONLY;

	if(p->Get_type() == Value::V_BOOL)
		return ALL_BOOL;

	if(!p->GetValues().empty())
		return ALL_CHOICE;

	Prop_int * pi = dynamic_cast<Prop_int *>(p);

	if(pi)
	{
		const int lo = pi->getMin(), hi = pi->getMax();

		if(!(lo == -1 && hi == -1) && hi > lo && hi - lo <= 100)
			return ALL_RANGE;
	}
	return ALL_READONLY;
}

//! The rows of the section at index in control's list, in DOSBox's order. Empty
//! if it is not a Section_prop or everything in it is hidden.
static std::vector<AllRow> SectionRows(int controlIndex)
{
	std::vector<AllRow> rows;
	Section_prop * sec = control ? dynamic_cast<Section_prop *>(control->GetSection(controlIndex)) : NULL;

	if(!sec || InList(hiddenSections, ARRAY_COUNT(hiddenSections), sec->GetName()))
		return rows;

	const bool live = InList(liveSections, ARRAY_COUNT(liveSections), sec->GetName());
	const bool anytime = InList(anytimeSections, ARRAY_COUNT(anytimeSections), sec->GetName());
	Property * p;

	for(int i = 0; (p = sec->Get_prop(i)) != NULL; i++)
	{
		if(IsHidden(sec->GetName(), p->propname.c_str()))
			continue;

		AllRow row;

		row.sec = sec;
		row.prop = p;
		row.curated = live ? FindCuratedRow(sec->GetName(), p->propname.c_str()) : NULL;
		row.live = live;
		row.atPrompt = live && !anytime;
		row.kind = row.curated ? ALL_DELEGATE : KindOf(p, &row.note);

		if(row.curated && !RowShown(*row.curated))
		{
			// the Video page leaves this row off because the platform cannot do it
			row.curated = NULL;
			row.kind = ALL_READONLY;
			row.note = "not available here";
		}
		else if(row.curated)
			row.note = "";
		else if(!strcasecmp(sec->GetName(), "cpu") && p->propname == "cycles")
			row.note = "Performance page";

		rows.push_back(row);
	}
	return rows;
}

//! Control's index of the n'th section that has anything to show, or -1.
static int VisibleSection(int n)
{
	for(int i = 0; control && control->GetSection(i) != NULL; i++)
	{
		if(!SectionRows(i).empty() && n-- == 0)
			return i;
	}
	return -1;
}

static int AllSectionCount()
{
	int count = 0;

	for(int i = 0; control && control->GetSection(i) != NULL; i++)
	{
		if(!SectionRows(i).empty())
			count++;
	}
	return count;
}

static bool AllRows(int page, std::vector<AllRow> & rows)
{
	const int index = VisibleSection(page - SETTINGS_PAGE_COUNT);

	if(page < SETTINGS_PAGE_COUNT || index < 0)
		return false;

	rows = SectionRows(index);
	return true;
}

static const AllRow * AllRowAt(const std::vector<AllRow> & rows, int row)
{
	return (row >= 0 && row < (int)rows.size()) ? &rows[row] : NULL;
}

static const char * AllSectionTitle(int page)
{
	const int index = VisibleSection(page - SETTINGS_PAGE_COUNT);
	Section * sec = (page >= SETTINGS_PAGE_COUNT && index >= 0 && control) ? control->GetSection(index) : NULL;

	if(!sec)
		return "";

	for(int i = 0; i < ARRAY_COUNT(sectionTitles); i++)
	{
		if(!strcasecmp(sectionTitles[i].name, sec->GetName()))
			return sectionTitles[i].title;
	}
	return sec->GetName();
}

//! Set when the row cannot be changed right now, and says why.
static const char * AllLockReason(const AllRow & r)
{
	if(!r.live || r.kind == ALL_READONLY)
		return NULL;

	if(r.atPrompt && !IsShellIdle())
		return "Only available at the DOS prompt.";

	if(!strcasecmp(r.sec->GetName(), "dos"))
	{
		const char * why = NULL;

		if(!CanReinitDosSection(&why))
			return why;
	}
	return NULL;
}

static std::string AllValueText(const AllRow & r)
{
	std::string v = r.sec->GetPropValue(r.prop->propname);

	if(r.kind == ALL_BOOL)
		v = r.sec->Get_bool(r.prop->propname) ? "On" : "Off";
	else if(r.prop->Get_type() == Value::V_HEX)
		v = "0x" + v;

	return v.empty() ? std::string("(none)") : v;
}

static void AllValue(const AllRow & r, char * buf, size_t size)
{
	if(r.kind == ALL_DELEGATE)
	{
		CuratedValue(r.curated, buf, size);
		return;
	}

	std::string v = AllValueText(r);
	const char * lock = AllLockReason(r);

	if(!r.live)
		v += " (next launch)";
	else if(r.kind == ALL_READONLY)
		v += std::string(" (") + r.note + ")";
	else if(lock)
		v += (r.atPrompt && !IsShellIdle()) ? " (DOS prompt only)" : " (unavailable)";

	snprintf(buf, size, "%s", v.c_str());
}

static const char * AllExtraNote(const AllRow & r)
{
	const char * section = r.sec->GetName();

	if(IsResourceProp(section, r.prop->propname.c_str()))
		return " A value that would clash with the other sound card (port, IRQ or DMA) is skipped.";

	if(!strcasecmp(section, "joystick"))
		return " Nothing drives the emulated joystick on this build yet.";

	return "";
}

static void AllHelp(const AllRow & r, char * buf, size_t size)
{
	char help[256];
	const char * lock = AllLockReason(r);

	if(r.kind == ALL_DELEGATE)
	{
		CuratedHelp(r.curated, buf, size);
		return;
	}

	CollapseSpaces(r.prop->Get_help(), help, sizeof(help));

	if(!r.live)
		snprintf(buf, size, "Takes effect the next time DOSBox starts: set it in dosbox.conf. %s", help);
	else if(r.kind == ALL_READONLY)
		snprintf(buf, size, "Cannot be changed from the menu (%s). %s", r.note, help);
	else
		snprintf(buf, size, "%s%s%s", lock ? lock : "", lock ? " " : "", help);

	const size_t used = strlen(buf);

	if(r.live && r.kind != ALL_READONLY)
		snprintf(buf + used, size - used, "%s", AllExtraNote(r));
}

static bool AllStep(const AllRow & r, int direction)
{
	if(r.kind == ALL_DELEGATE)
		return CuratedStep(r.curated, direction);

	if(!r.live || r.kind == ALL_READONLY || AllLockReason(r))
		return false;

	const char * section = r.sec->GetName();
	const char * name = r.prop->propname.c_str();

	if(r.kind == ALL_BOOL)
	{
		const std::string next = r.sec->Get_bool(name) ? "false" : "true";

		return !Refusal(section, name, next) && Apply(section, name, next);
	}

	if(r.kind == ALL_RANGE)
	{
		Prop_int * pi = dynamic_cast<Prop_int *>(r.prop);

		if(!pi)
			return false;

		const int lo = pi->getMin(), hi = pi->getMax();
		const int amount = (hi - lo <= 20) ? 1 : 5;
		int next = r.sec->Get_int(name) + ((direction >= 0) ? amount : -amount);
		char value[16];

		if(next > hi)
			next = lo;
		else if(next < lo)
			next = hi;

		snprintf(value, sizeof(value), "%d", next);
		return !Refusal(section, name, value) && Apply(section, name, value);
	}

	// ALL_CHOICE: DOSBox's own list, starting from where the value is now.
	// The list starts with the default and is otherwise in DOSBox's order, which
	// is right for words but not for numbers (hex bases come out 240, 220, 260
	// ...), so numbers are stepped in ascending order.
	const std::vector<Value> & values = r.prop->GetValues();
	std::vector<std::string> list;
	const std::string current = GetConfig(section, name);
	const int step = (direction >= 0) ? 1 : -1;
	const Value::Etype type = r.prop->Get_type();
	int index = -1;

	for(size_t i = 0; i < values.size(); i++)
		list.push_back(values[i].ToString());

	if(type == Value::V_INT || type == Value::V_HEX)
	{
		const int base = (type == Value::V_HEX) ? 16 : 10;

		for(size_t i = 1; i < list.size(); i++)
		{
			for(size_t j = i; j > 0 && strtol(list[j - 1].c_str(), NULL, base) > strtol(list[j].c_str(), NULL, base); j--)
				list[j].swap(list[j - 1]);
		}
	}

	const int count = (int)list.size();

	for(int i = 0; i < count; i++)
	{
		if(!strcasecmp(list[i].c_str(), current.c_str()))
			index = i;
	}

	for(int k = 1; k <= count; k++)
	{
		const int i = (index < 0) ? (step > 0 ? k - 1 : count - k)
			: (((index + step * k) % count) + count) % count;
		const std::string & candidate = list[i];

		if(!strcasecmp(candidate.c_str(), current.c_str()))
			continue;

		if(Refusal(section, name, candidate))
			continue;

		return Apply(section, name, candidate);
	}
	return false;
}

/****************************************************************************
 * Public interface
 ***************************************************************************/
int Settings_AllSectionCount()
{
	return AllSectionCount();
}

const char * Settings_PageTitle(int page)
{
	if(page >= SETTINGS_PAGE_COUNT)
		return AllSectionTitle(page);

	if(page < 0)
		return "";

	return pages[page].title;
}

int Settings_RowCount(int page)
{
	std::vector<AllRow> rows;

	if(page >= SETTINGS_PAGE_COUNT)
		return AllRows(page, rows) ? (int)rows.size() : 0;

	if(page < 0)
		return 0;

	int count = 0;

	for(int i = 0; i < pages[page].count; i++)
		count += RowShown(pages[page].rows[i]) ? 1 : 0;

	return count;
}

const char * Settings_RowLabel(int page, int row)
{
	std::vector<AllRow> rows;

	if(page >= SETTINGS_PAGE_COUNT)
	{
		// the property's own name, which is what dosbox.conf calls it; the
		// pointer is into the property, which outlives the page
		const AllRow * a = AllRows(page, rows) ? AllRowAt(rows, row) : NULL;

		return a ? a->prop->propname.c_str() : "";
	}

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
	else if(row.format == FMT_PERCENT)
		snprintf(buf, size, "%d%%", value);
	else if(row.format == FMT_SIGNED)
		snprintf(buf, size, value == 0 ? "%d" : "%+d", value);
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

static void CuratedValue(const SettingRow * r, char * buf, size_t size)
{
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
		case ROW_RANGE:
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
	else if(r->inactive && r->inactive())
	{
		const size_t used = strlen(buf);
		snprintf(buf + used, size - used, " (not used)");
	}
}

static void CuratedHelp(const SettingRow * r, char * buf, size_t size)
{
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

	const char * unused = (!IsLocked(*r) && r->inactive) ? r->inactive() : NULL;

	snprintf(buf, size, "%s%s%s%s", IsLocked(*r) ? "Only available at the DOS prompt. " : "",
		unused ? unused : "", unused ? " " : "", help);
}

static bool CuratedStep(const SettingRow * r, int direction)
{
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

			const std::string next = sec->Get_bool(r->prop) ? "false" : "true";

			if(Refusal(r->section, r->prop, next))
				return false;

			return Apply(r->section, r->prop, next);
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

			if(Refusal(r->section, r->prop, value))
				return false;

			return Apply(r->section, r->prop, value);
		}

		case ROW_RANGE:
		{
			// the property's own range, in fixed steps. A value off the grid (from
			// dosbox.conf) steps from where it is; the last step before an end lands
			// on the end, and one more wraps to the other.
			Section_prop * sec = GetSection(r->section);
			Prop_int * p = sec ? dynamic_cast<Prop_int *>(FindProp(sec, r->prop)) : NULL;

			if(!p)
				return false;

			const int lo = p->getMin(), hi = p->getMax();
			const int amount = (r->step > 0) ? r->step : 1;
			const int current = CurrentInt(*r);
			int next = current + ((direction >= 0) ? amount : -amount);

			if(direction >= 0 && next > hi)
				next = (current >= hi) ? lo : hi;
			else if(direction < 0 && next < lo)
				next = (current <= lo) ? hi : lo;

			if(next == current)
				return false;

			char value[16];

			snprintf(value, sizeof(value), "%d", next);

			if(Refusal(r->section, r->prop, value))
				return false;

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

			if(Refusal(r->section, r->prop, value))
				return false;

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

				if(Refusal(r->section, r->prop, candidate))
					continue;

				return Apply(r->section, r->prop, candidate);
			}
			return false;
		}
	}

	return false;
}

void Settings_RowValue(int page, int row, char * buf, size_t size)
{
	std::vector<AllRow> rows;

	if(size == 0)
		return;

	buf[0] = 0;

	if(page >= SETTINGS_PAGE_COUNT)
	{
		const AllRow * a = AllRows(page, rows) ? AllRowAt(rows, row) : NULL;

		if(a)
			AllValue(*a, buf, size);
		return;
	}

	CuratedValue(GetRow(page, row), buf, size);
}

void Settings_RowHelp(int page, int row, char * buf, size_t size)
{
	std::vector<AllRow> rows;

	if(size == 0)
		return;

	buf[0] = 0;

	if(page >= SETTINGS_PAGE_COUNT)
	{
		const AllRow * a = AllRows(page, rows) ? AllRowAt(rows, row) : NULL;

		if(a)
			AllHelp(*a, buf, size);
		return;
	}

	CuratedHelp(GetRow(page, row), buf, size);
}

bool Settings_RowStep(int page, int row, int direction)
{
	std::vector<AllRow> rows;

	if(page >= SETTINGS_PAGE_COUNT)
	{
		const AllRow * a = AllRows(page, rows) ? AllRowAt(rows, row) : NULL;

		return a && AllStep(*a, direction);
	}

	return CuratedStep(GetRow(page, row), direction);
}

/****************************************************************************
 * Save
 *
 * The config is the single copy of every setting, and the pages write to it,
 * with two exceptions: the home screen's +/- buttons change the running
 * cycles and frameskip without touching it (the pages show the running
 * values). Those two are put into the config first, with no re-initialisation
 * (nothing is meant to run differently), so the file says what the player was
 * looking at. Cycles in auto mode are not: they are what DOSBox guessed, not
 * a choice.
 ***************************************************************************/
static void SyncRunningValues()
{
	Section_prop * render = GetSection("render");
	Section_prop * cpu = GetSection("cpu");
	char line[48];

	if(render && render->Get_int("frameskip") != MENU_FrameskipDisplay)
	{
		snprintf(line, sizeof(line), "frameskip=%d", MENU_FrameskipDisplay);
		render->HandleInputline(line);
	}

	if(!cpu)
		return;

	const CyclesState state = ReadCycles();

	if(state.mode == CYCLES_FIXED)
	{
		const int running = EffectiveFixed(state);

		if(running > 0 && running != state.amount)
		{
			snprintf(line, sizeof(line), "cycles=fixed %d", running);
			cpu->HandleInputline(line);
		}
	}
	else if(state.mode == CYCLES_MAX)
	{
		// "max 80% 20000" has a cycle limit after the percentage; rewriting it
		// from the percentage alone would lose it, so that form is left as it is
		Prop_multival * cycles = cpu->Get_multival("cycles");
		const std::string params = cycles ? cycles->GetSection()->Get_string("parameters") : "";
		size_t tokens = 0, percents = 0, pos = 0;

		while(pos < params.size())
		{
			size_t end = params.find(' ', pos);

			if(end == std::string::npos)
				end = params.size();

			if(end > pos)
			{
				tokens++;
				percents += (params[end - 1] == '%') ? 1 : 0;
			}
			pos = end + 1;
		}

		const int running = EffectiveMaxPercent(state);

		if(tokens == percents && running != (state.amount > 0 ? state.amount : 100))
		{
			snprintf(line, sizeof(line), "cycles=max %d%%", running);
			cpu->HandleInputline(line);
		}
	}
}

static const char * BaseName(const std::string & path)
{
	const size_t slash = path.find_last_of("/\\");

	return path.c_str() + ((slash == std::string::npos) ? 0 : slash + 1);
}

bool Settings_Save(char * message, size_t size)
{
	if(size == 0)
		return false;

	SyncRunningValues();

	const std::string path = ConfigSavePath();
	std::string error;

	if(!ConfigSave(path.c_str(), &error))
	{
		snprintf(message, size, "Not saved. %s", error.c_str());
		return false;
	}

	snprintf(message, size, "Saved %s. The file it replaced is kept as %s.bak.", path.c_str(), BaseName(path));
	return true;
}
