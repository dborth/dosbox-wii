/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * displayconfig.cpp
 ***************************************************************************/
#include <string>

#include "dosbox.h"
#include "setup.h"
#include "control.h"
#include "displayconfig.h"
#include "videosupport.h"

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
