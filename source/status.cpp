/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * status.cpp
 *
 * The Status page: one line per thing worth knowing about, in six groups
 * (CPU, Video, Audio, Memory, Input, Drives and config). Each line is the
 * value in the config, then in brackets the value in effect when that is
 * something else, then " *" if the config value is not the property's
 * default. See status.h.
 *
 * "In effect" comes from EmulationStatus (dosboxsupport.cpp), which reads
 * the running core. Nothing here changes any state.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <string>
#include <vector>

#include "dosbox.h"
#include "setup.h"
#include "control.h"
#include "status.h"
#include "dosboxsupport.h"
#include "fileop.h"
#include "input.h"

//! An option list holds 49 characters of value
#define MAX_VALUE	49

struct Line
{
	std::string label;
	std::string value;
	std::string help;
};

static std::vector<Line> lines;

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

	for(int i = 0; sec && (p = sec->Get_prop(i)) != NULL; i++)
	{
		if(strcasecmp(p->propname.c_str(), name) == 0)
			return p;
	}
	return NULL;
}

//! The property's value as it stands in the config, or "" if there isn't one.
static std::string Cfg(const char * section, const char * prop)
{
	Section_prop * sec = GetSection(section);

	if(!sec)
		return "";

	const std::string value = sec->GetPropValue(prop);

	return (value == NO_SUCH_PROPERTY) ? std::string("") : value;
}

static bool SameText(const std::string & a, const std::string & b)
{
	return strcasecmp(a.c_str(), b.c_str()) == 0;
}

/**
 * True if the property is not at its default. A multival property (cycles)
 * is changed when any of its parts is.
 */
static bool Changed(const char * section, const char * prop)
{
	Property * p = FindProp(GetSection(section), prop);

	if(!p)
		return false;

	Prop_multival * multi = dynamic_cast<Prop_multival *>(p);

	if(multi)
	{
		Section_prop * parts = multi->GetSection();
		Property * q;

		for(int i = 0; parts && (q = parts->Get_prop(i)) != NULL; i++)
		{
			if(!SameText(q->GetValue().ToString(), q->Get_Default_Value().ToString()))
				return true;
		}
		return false;
	}

	return !SameText(p->GetValue().ToString(), p->Get_Default_Value().ToString());
}

static std::string OnOff(const std::string & configValue)
{
	return SameText(configValue, "true") ? "On" : "Off";
}

static std::string Number(int n)
{
	char text[16];

	snprintf(text, sizeof(text), "%d", n);
	return text;
}

/****************************************************************************
 * Lines
 ***************************************************************************/
static void Heading(const char * title)
{
	Line l;

	l.label = title;
	lines.push_back(l);
}

/**
 * Adds a line. The value is `configured`, then `effective` in brackets when
 * it is there and says something else, then the not-the-default marker.
 */
static void Add(const char * label, const std::string & configured, const std::string & effective,
	bool changed, const char * help)
{
	Line l;
	const std::string marker = changed ? " *" : "";

	l.label = label;
	l.value = configured;

	if(!effective.empty() && !SameText(effective, configured))
		l.value += " (" + effective + ")";

	if(l.value.size() + marker.size() > MAX_VALUE)
		l.value.resize(MAX_VALUE - marker.size());

	l.value += marker;
	l.help = help;
	lines.push_back(l);
}

//! A line with nothing to compare: just a value.
static void AddPlain(const char * label, const std::string & value, const char * help)
{
	Add(label, value, "", false, help);
}

//! A line for a config property, shown as it is in the config.
static void AddProp(const char * label, const char * section, const char * prop,
	const std::string & effective, const char * help)
{
	Add(label, Cfg(section, prop), effective, Changed(section, prop), help);
}

//! A line for a bool property, shown as On or Off.
static void AddBool(const char * label, const char * section, const char * prop,
	const std::string & effective, const char * help)
{
	Add(label, OnOff(Cfg(section, prop)), effective, Changed(section, prop), help);
}

//! The end of a long text, which is the part that tells paths apart.
static std::string Tail(const std::string & text, size_t max)
{
	if(text.size() <= max)
		return text;

	return "..." + text.substr(text.size() - (max - 3));
}

/****************************************************************************
 * CPU
 ***************************************************************************/
static std::string Cycles(int n)
{
	return Number(n) + " cycles";
}

static void CpuLines(const EmulationStatus & st)
{
	std::string core = st.core;

	Heading("CPU");

	AddPlain("DOS state", st.atPrompt ? "At the DOS prompt" : "A program is running",
		"Settings marked DOS prompt only can be changed while this says At the DOS prompt.");

	AddProp("CPU core", "cpu", "core", core,
		"The core DOSBox is running now. With auto, the dynamic recompiler is used from the start "
		"if it could get a code cache.");

	// The one question the page exists to answer for some people
	AddPlain("Dynarec", core.empty() ? "Unknown" : (core == "dynamic" ? "Active" : "Not active"),
		"Whether the dynamic recompiler is the core running now. If it is not active with core auto "
		"or dynamic, there was no memory for its code cache.");

	AddProp("CPU type", "cpu", "cputype", "",
		"The CPU timing DOSBox emulates. auto picks a mix that suits most games.");

	// cycles: the config says auto, fixed N or max P%; the core is running
	// some number of cycles or a share of the host, and the home screen's
	// +/- buttons move either without touching the config
	Section_prop * cpu = GetSection("cpu");
	Prop_multival * multi = cpu ? cpu->Get_multival("cycles") : NULL;
	std::string type, params;

	if(multi)
	{
		type = multi->GetSection()->Get_string("type");
		params = multi->GetSection()->Get_string("parameters");
	}

	std::string configured = "auto";
	std::string effective;
	int amount = 0;

	if(type == "fixed" || atoi(type.c_str()) > 0)
	{
		amount = (type == "fixed") ? atoi(params.c_str()) : atoi(type.c_str());
		configured = "fixed " + Number(amount);
	}
	else if(type == "max")
	{
		amount = 100;

		// the percentage is the parameter that ends in '%'
		size_t pos = 0;
		while(pos < params.size())
		{
			size_t end = params.find(' ', pos);
			if(end == std::string::npos)
				end = params.size();

			if(end > pos && params[end - 1] == '%')
				amount = atoi(params.substr(pos, end - pos).c_str());

			pos = end + 1;
		}
		configured = "max " + Number(amount) + "%";
	}

	const std::string running = "max " + Number(st.cyclePercent) + "%";
	const std::string measured = "~" + Cycles(st.cycleMax);

	if(configured.compare(0, 5, "fixed") == 0)
	{
		if(st.autoAdjust)
			effective = running;
		else if(st.cycleMax != amount)
			effective = Cycles(st.cycleMax);
	}
	else if(configured.compare(0, 3, "max") == 0)
	{
		if(st.autoAdjust)
			effective = (st.cyclePercent == amount) ? measured : running + ", " + measured;
		else
			effective = Cycles(st.cycleMax);
	}
	else
		effective = st.autoAdjust ? running + ", " + measured : Cycles(st.cycleMax);

	Add("Cycles", configured, effective, Changed("cpu", "cycles"),
		"How much the emulated CPU does per millisecond. Fixed is a set number of cycles. Max uses a share "
		"of the console and the number is what it manages. Auto starts fixed and switches to max when a "
		"game enters protected mode. The home screen's + and - change the running value, which goes in the config when you press Save.");
}

/****************************************************************************
 * Video
 ***************************************************************************/
//! 4:3 and the other common shapes by name, anything else as a ratio.
static std::string ShapeText(double ratio)
{
	static const struct { double ratio; const char * name; } known[] =
		{ { 4.0 / 3.0, "4:3" }, { 16.0 / 9.0, "16:9" }, { 16.0 / 10.0, "16:10" }, { 5.0 / 4.0, "5:4" } };
	char text[24];

	for(size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++)
	{
		if(fabs(ratio - known[i].ratio) < 0.01)
			return known[i].name;
	}

	snprintf(text, sizeof(text), "%.2f:1", ratio);
	return text;
}

static void VideoLines(const EmulationStatus & st)
{
	bool scanlines = false, widescreen = false;

	GetEmulatorVideoCapabilities(&scanlines, NULL, &widescreen);

	Heading("VIDEO");

	if(!st.haveMode)
	{
		AddPlain("Screen mode", "None yet", "DOSBox has not set a video mode.");
		AddPlain("Frame to display", "-", "The size of the picture DOSBox hands to the display.");
		AddPlain("Picture shape", "-", "The shape of the picture before the display fits it to the screen.");
	}
	else
	{
		AddPlain("Screen mode", std::string(st.textMode ? "Text " : "Graphics ")
			+ Number(st.srcWidth) + "x" + Number(st.srcHeight),
			"What the emulated video card is drawing now.");

		AddPlain("Frame to display", Number(st.frameWidth) + "x" + Number(st.frameHeight),
			"The size of the picture DOSBox hands to the display. Modes that a real monitor would stretch "
			"in one direction are doubled here.");

		// corrected: each source pixel has the shape the card's monitor gave it
		double ratio = (st.srcHeight > 0) ? (double)st.srcWidth / (double)st.srcHeight : 1.0;

		if(st.aspectOn && st.pixelRatio > 0.0)
			ratio /= st.pixelRatio;

		AddPlain("Picture shape", ShapeText(ratio),
			"The shape of the picture before the display fits it to the screen.");
	}

	AddProp("Fit", "display", "fit", "",
		"How big the picture is. fit keeps its shape, integer uses the largest whole-number multiple, "
		"fill stretches it over the screen.");

	AddProp("Aspect", "display", "aspect", st.aspectOn ? "corrected" : "square",
		"corrected shows the picture the shape a monitor did, such as 320x200 as 4:3. square makes every pixel square.");

	AddProp("Filter", "display", "filter", "",
		"How the picture is smoothed when it is enlarged: sharp, bilinear or nearest.");

	if(scanlines)
	{
		const std::string percent = Cfg("display", "scanlines");

		Add("Scanlines", percent == "0" ? "off" : percent + "%", "", Changed("display", "scanlines"),
			"How dark the gaps between lines are.");
	}

	Add("Zoom", Cfg("display", "zoomx") + "% x " + Cfg("display", "zoomy") + "%", "",
		Changed("display", "zoomx") || Changed("display", "zoomy"),
		"Width and height of the picture as a percentage of its normal size. Used by the fit and fill sizes; integer ignores it.");

	Add("Shift", Cfg("display", "shiftx") + ", " + Cfg("display", "shifty"), "",
		Changed("display", "shiftx") || Changed("display", "shifty"),
		"How far the picture is moved right and down from the middle of the screen. Negative moves it left and up.");

	if(widescreen)
		AddProp("Widescreen", "display", "widescreen", "",
			"Which shape the TV is, so the picture is not stretched on a widescreen TV. auto follows the "
			"console's own setting.");

	// the +/- on the home screen change the running value; Save puts it in the config
	Add("Frameskip", Cfg("render", "frameskip"), Number(st.frameskip), Changed("render", "frameskip"),
		"Frames not drawn for each frame that is. The home screen's + and - change the running value, "
		"which goes in the config when you press Save.");
}

/****************************************************************************
 * Audio
 ***************************************************************************/
static void AudioLines(const EmulationStatus & st)
{
	int rate = 0, frames = 0;
	std::string output;
	char text[64];

	Heading("AUDIO");

	if(GetEmulatorAudioInfo(&rate, &frames) && rate > 0)
	{
		if(frames > 0)
			snprintf(text, sizeof(text), "%d Hz, %d frames (%d ms)", rate, frames, (frames * 1000) / rate);
		else
			snprintf(text, sizeof(text), "%d Hz", rate);

		output = text;
	}
	else
		output = "No audio output";

	AddPlain("Audio output", output,
		"Fixed by the console's audio hardware, and DOSBox's mixer follows it. A device set to the same "
		"rate is not resampled.");

	AddProp("Sound Blaster", "sblaster", "sbtype", st.sbType,
		"The card DOSBox pretends to have. An SB16 needs a VGA machine, otherwise DOSBox uses an SB Pro 2.");

	AddProp("OPL mode", "sblaster", "oplmode", st.oplMode,
		"The FM music chip. auto follows the Sound Blaster type.");

	// a device whose rate is not the output's is resampled by the mixer
	const int oplRate = atoi(Cfg("sblaster", "oplrate").c_str());

	Add("OPL rate", Number(oplRate) + " Hz", (rate > 0 && oplRate != rate) ? "resampled" : "",
		Changed("sblaster", "oplrate"),
		"The rate the FM chip is generated at. The same as the output rate is not resampled.");

	AddBool("PC speaker", "speaker", "pcspeaker", "", "The PC's internal speaker.");

	AddProp("Tandy sound", "speaker", "tandy", st.tandyOn ? "on" : "off",
		"Tandy 3-voice sound. auto turns it on only for the tandy and pcjr machines.");

	AddBool("Disney", "speaker", "disney", "", "Disney Sound Source (and Covox) compatible sound.");

	AddBool("Gravis Ultrasound", "gus", "gus", "", "Gravis Ultrasound emulation.");
}

/****************************************************************************
 * Memory
 ***************************************************************************/
static void MemoryLines(const EmulationStatus & st)
{
	static const char * const emsNames[] = { "false", "true", "emsboard", "emm386" };

	Heading("MEMORY");

	AddProp("Machine", "dosbox", "machine", "",
		"The kind of PC DOSBox emulates. Takes effect the next time DOSBox starts.");

	Add("Memory", Cfg("dosbox", "memsize") + " MB", Number(st.memoryMB) + " MB", Changed("dosbox", "memsize"),
		"How much memory the machine has. Takes effect the next time DOSBox starts.");

	AddBool("XMS", "dos", "xms", "", "Extended memory services.");

	Add("EMS", Cfg("dos", "ems"),
		(st.emsType >= 0 && st.emsType < 4) ? emsNames[st.emsType] : "",
		Changed("dos", "ems"), "Expanded memory. DOSBox turns it off for the pcjr machine.");

	AddBool("UMB", "dos", "umb", st.umbActive ? "On" : "Off",
		"Upper memory blocks. They need XMS, and DOSBox does not offer them on the tandy and pcjr machines.");
}

/****************************************************************************
 * Input
 ***************************************************************************/
static void InputLines()
{
	char text[64];
	int found = 0;

	Heading("INPUT");

	for(int channel = 0; channel < 4; channel++)
	{
		if(!GetControllerSummary(channel, text, sizeof(text)))
			continue;

		char label[16];

		snprintf(label, sizeof(label), "Player %d", channel + 1);
		AddPlain(label, text, "The controller connected for this player.");
		found++;
	}

	if(!found)
		AddPlain("Controllers", "None connected", "No controller is connected.");

	AddProp("Joystick", "joystick", "joysticktype", "",
		"The kind of joystick DOSBox emulates. Auto picks 4axis: one joystick with four buttons, driven by the first controller.");
}

/****************************************************************************
 * Drives and config
 ***************************************************************************/
static void DriveLines()
{
	char info[256];
	int mounted = 0;

	Heading("DRIVES AND CONFIG");

	if(!control || control->configfiles.empty())
		AddPlain("Config file", "None (defaults)",
			"No dosbox.conf was found, so DOSBox is using its default settings. Save creates one.");
	else
	{
		const size_t extra = control->configfiles.size() - 1;
		const std::string more = extra ? " +" + Number((int)extra) : "";

		AddPlain("Config file", Tail(control->configfiles[0], MAX_VALUE - more.size()) + more,
			"The config file DOSBox loaded, which Save writes back to. Further files given with -conf follow it.");
	}

	for(char letter = 'A'; letter <= 'Z'; letter++)
	{
		if(!IsDOSDriveMounted(letter) || !GetDOSDriveInfo(letter, info, sizeof(info)))
			continue;

		char label[16];

		snprintf(label, sizeof(label), "Drive %c:", letter);
		AddPlain(label, Tail(info, MAX_VALUE), "Where the DOS drive comes from.");
		mounted++;
	}

	if(!mounted)
		AddPlain("Drives", "None mounted", "No DOS drive is mounted.");
}

/****************************************************************************
 * Public interface
 ***************************************************************************/
void Status_Refresh()
{
	EmulationStatus st;

	lines.clear();

	if(!GetEmulationStatus(&st))
	{
		Heading("STATUS");
		AddPlain("Not available", "DOSBox is not running", "The state of DOSBox cannot be read yet.");
		return;
	}

	CpuLines(st);
	VideoLines(st);
	AudioLines(st);
	MemoryLines(st);
	InputLines();
	DriveLines();
}

int Status_RowCount()
{
	return (int)lines.size();
}

const char * Status_RowLabel(int row)
{
	return (row >= 0 && row < (int)lines.size()) ? lines[row].label.c_str() : "";
}

void Status_RowValue(int row, char * buf, size_t size)
{
	if(size == 0)
		return;

	snprintf(buf, size, "%s", (row >= 0 && row < (int)lines.size()) ? lines[row].value.c_str() : "");
}

void Status_RowHelp(int row, char * buf, size_t size)
{
	if(size == 0)
		return;

	snprintf(buf, size, "%s", (row >= 0 && row < (int)lines.size()) ? lines[row].help.c_str() : "");
}
