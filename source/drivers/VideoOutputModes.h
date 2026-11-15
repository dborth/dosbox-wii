/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * VideoOutputModes.h
 *
 * The video output modes the user can pick (Wii): every stock GXRModeObj
 * libogc2 declares in ogc/video_types.h, and nothing custom. This is only the
 * list of names, so the config, the menu and the host tests can use it without
 * libogc; the Ogc driver expands the same list into a table of the modes
 * themselves, so the two cannot drift apart.
 *
 * The identifier is the libogc2 one without the TV prefix. The config value is
 * that identifier in lower case, because DOSBox lower-cases anything it is
 * given for a property that has a list of values; the value is read without
 * regard to case, so "Ntsc480IntDf" in a hand-written conf works too.
 ***************************************************************************/
#pragma once

#include <string>
#include <vector>
#include <strings.h>

//! X(identifier, menu label). A menu label means the entry is on the Video
//! page; "" means it can only be set in the config file (its value is the label).
//! The order is the order the menu steps through.
#define VIDEO_OUTPUT_MODES(X) \
\
	/* NTSC */ \
	X(Ntsc240Ds,               "NTSC 240p") \
	X(Ntsc240DsVf,             "") \
	X(Ntsc240DsAa,             "") \
	X(Ntsc240Int,              "") \
	X(Ntsc240IntDf,            "") \
	X(Ntsc240IntAa,            "") \
	X(Ntsc480Int,              "") \
	X(Ntsc480IntDf,            "NTSC 480i") \
	X(Ntsc480IntAa,            "") \
	X(Ntsc480Prog,             "NTSC 480p") \
	X(Ntsc480ProgSoft,         "") \
	X(Ntsc480ProgAa,           "") \
\
	/* MPAL (Brazil) */ \
	X(Mpal240Ds,               "") \
	X(Mpal240DsVf,             "") \
	X(Mpal240DsAa,             "") \
	X(Mpal240Int,              "") \
	X(Mpal240IntDf,            "") \
	X(Mpal240IntAa,            "") \
	X(Mpal480Int,              "") \
	X(Mpal480IntDf,            "") \
	X(Mpal480IntAa,            "") \
	X(Mpal480Prog,             "") \
	X(Mpal480ProgSoft,         "") \
	X(Mpal480ProgAa,           "") \
\
	/* PAL, 50 Hz */ \
	X(Pal264Ds,                "") \
	X(Pal264DsVf,              "") \
	X(Pal264DsAa,              "") \
	X(Pal288DsScale,           "PAL 288p") \
	X(Pal288DsVfScale,         "") \
	X(Pal288DsAaScale,         "") \
	X(Pal264Int,               "") \
	X(Pal264IntDf,             "") \
	X(Pal264IntAa,             "") \
	X(Pal288IntScale,          "") \
	X(Pal288IntDfScale,        "") \
	X(Pal528Int,               "") \
	X(Pal528IntDf,             "") \
	X(Pal524IntAa,             "") \
	X(Pal576IntScale,          "") \
	X(Pal576IntDfScale,        "PAL 576i") \
	X(Pal528Prog,              "") \
	X(Pal528ProgSoft,          "") \
	X(Pal524ProgAa,            "") \
	X(Pal576ProgScale,         "PAL 576p") \
	X(Pal576ProgSoftScale,     "") \
\
	/* EuRGB60 (PAL colour, 60 Hz) */ \
	X(Eurgb60Hz240Ds,          "PAL60 240p") \
	X(Eurgb60Hz240DsVf,        "") \
	X(Eurgb60Hz240DsAa,        "") \
	X(Eurgb60Hz240Int,         "") \
	X(Eurgb60Hz240IntDf,       "") \
	X(Eurgb60Hz240IntAa,       "") \
	X(Eurgb60Hz480Int,         "") \
	X(Eurgb60Hz480IntDf,       "PAL60 480i") \
	X(Eurgb60Hz480IntAa,       "") \
	X(Eurgb60Hz480Prog,        "PAL60 480p") \
	X(Eurgb60Hz480ProgSoft,    "") \
	X(Eurgb60Hz480ProgAa,      "") \

//! "No choice made": the mode the console's own settings call for
#define VIDEO_OUTPUT_AUTO (-1)

inline int VideoOutput_Count()
{
	static const int count = 0
#define X(name, label) + 1
		VIDEO_OUTPUT_MODES(X)
#undef X
		;
	return count;
}

//! The libogc2 identifier without the TV prefix, eg. "Ntsc480IntDf"
inline const char * VideoOutput_Name(int index)
{
	static const char * const names[] = {
#define X(name, label) #name,
		VIDEO_OUTPUT_MODES(X)
#undef X
	};
	return (index >= 0 && index < VideoOutput_Count()) ? names[index] : "auto";
}

//! True for an entry the Video page offers
inline bool VideoOutput_InMenu(int index)
{
	static const char * const labels[] = {
#define X(name, label) label,
		VIDEO_OUTPUT_MODES(X)
#undef X
	};
	return index >= 0 && index < VideoOutput_Count() && labels[index][0] != 0;
}

//! What the Video page calls an entry; the identifier for one it does not list
inline const char * VideoOutput_Label(int index)
{
	static const char * const labels[] = {
#define X(name, label) label,
		VIDEO_OUTPUT_MODES(X)
#undef X
	};
	if(index < 0 || index >= VideoOutput_Count())
		return "auto";
	return labels[index][0] ? labels[index] : VideoOutput_Name(index);
}

//! The config value: the identifier in lower case
inline const char * VideoOutput_Value(int index)
{
	static std::vector<std::string> values;

	if(index < 0 || index >= VideoOutput_Count())
		return "auto";

	if(values.empty())
	{
		for(int i = 0; i < VideoOutput_Count(); i++)
		{
			std::string v = VideoOutput_Name(i);
			for(size_t c = 0; c < v.size(); c++)
				if(v[c] >= 'A' && v[c] <= 'Z')
					v[c] = (char)(v[c] - 'A' + 'a');
			values.push_back(v);
		}
	}
	return values[index].c_str();
}

//! Every value the config accepts, "auto" first, ending in a null pointer
//! (for Property::Set_values)
inline const char * const * VideoOutput_Values()
{
	static std::vector<const char *> list;

	if(list.empty())
	{
		list.push_back("auto");
		for(int i = 0; i < VideoOutput_Count(); i++)
			list.push_back(VideoOutput_Value(i));
		list.push_back(0);
	}
	return &list[0];
}

//! The table index for a config value, ignoring case. VIDEO_OUTPUT_AUTO for
//! "auto" and for anything that is not a mode, so a bad value is the same as none.
inline int VideoOutput_Find(const char * text)
{
	if(!text)
		return VIDEO_OUTPUT_AUTO;

	for(int i = 0; i < VideoOutput_Count(); i++)
	{
		if(strcasecmp(text, VideoOutput_Name(i)) == 0)
			return i;
	}
	return VIDEO_OUTPUT_AUTO;
}

/****************************************************************************
 * Where a mode puts the picture on the screen
 *
 * The emulator and the menu are in different modes (the menu stays in the
 * console's own 480-line mode), and the menu draws the last game frame behind
 * itself on its 640x480 canvas. A canvas row is a share of the lines the mode
 * shows, and not every mode shows the same lines (PAL 264p covers 528 of the
 * 576), so a frame placed on the emulator's canvas has to be moved onto the
 * menu's to land on the same part of the screen.
 ***************************************************************************/
//! The part of the full picture height a mode covers, as fractions of it
struct VideoExtent
{
	float top;		//!< where the first line is
	float height;	//!< how much it covers
};

//! A rect's vertical position and height on the canvas of mode `from`, moved
//! to the canvas of mode `to` so it covers the same part of the screen. The
//! result can run outside the canvas (the menu's mode shows less): what is
//! there is off the screen.
inline void VideoOutput_RemapCanvasY(const VideoExtent & from, const VideoExtent & to, float canvasHeight, float * y, float * h)
{
	if(!y || !h || to.height <= 0.0f || from.height <= 0.0f)
		return;

	const float scale = from.height / to.height;
	*y = ((from.top - to.top) / to.height) * canvasHeight + *y * scale;
	*h = *h * scale;
}
