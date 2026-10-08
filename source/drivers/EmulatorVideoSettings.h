/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * EmulatorVideoSettings.h
 *
 * User-facing display options of the emulator video output. Plain data: the
 * app owns persistence and the menu, the drivers only consume it through
 * EmulatorVideoDriver::setSettings(). Nothing here knows about DOSBox.
 ***************************************************************************/
#pragma once

//! How the picture is sized inside the screen
enum class VideoFit
{
	Fit,      //!< as large as fits, keeping the picture's shape (bars where needed)
	Integer,  //!< largest whole-number multiple that fits; zoom is ignored
	Fill      //!< stretched over the whole screen, shape ignored
};

//! Whether the source's non-square pixels are honoured
enum class VideoAspect
{
	Corrected, //!< use the pixel aspect the emulator reports (eg. 320x200 shown as 4:3)
	Square     //!< treat every source pixel as square
};

enum class VideoFilter
{
	Sharp,    //!< sharp bilinear: crisp pixel centres, blended edges only
	Bilinear, //!< plain bilinear
	Nearest   //!< no blending
};

//! Only meaningful where the driver can't tell what shape the screen is
//! (Wii: a 16:9 TV stretches the 4:3-shaped output). See
//! EmulatorVideoCapabilities::widescreenSetting.
enum class VideoWidescreen
{
	Auto,        //!< follow the console's own aspect ratio setting
	Display4x3,  //!< the screen is 4:3: no compensation
	Display16x9  //!< the screen is 16:9: squeeze the picture so it isn't stretched
};

struct EmulatorVideoSettings
{
	VideoFit fit = VideoFit::Fit;
	VideoAspect aspect = VideoAspect::Corrected;
	VideoFilter filter = VideoFilter::Bilinear;
	VideoWidescreen widescreen = VideoWidescreen::Auto;

	//! Darkness of the gaps between lines, 0 (off) to 1
	float scanlines = 0.0f;

	//! Multipliers on the picture's width and height (Fit and Fill only)
	float zoomX = 1.0f;
	float zoomY = 1.0f;

	//! Picture offset; positive moves right / down. Units are 1/640 of the
	//! screen width and 1/480 of its height on every platform, so a saved
	//! value looks the same everywhere.
	int shiftX = 0;
	int shiftY = 0;

	static constexpr float ZOOM_MIN = 0.50f;
	static constexpr float ZOOM_MAX = 2.00f;
	static constexpr int SHIFT_MIN = -100;
	static constexpr int SHIFT_MAX = 100;
	static constexpr float SHIFT_UNITS_X = 640.0f;
	static constexpr float SHIFT_UNITS_Y = 480.0f;

	//! Forces every field into its valid range (also NaN -> default)
	void clamp()
	{
		if (!(zoomX >= ZOOM_MIN)) zoomX = (zoomX != zoomX) ? 1.0f : ZOOM_MIN;
		if (zoomX > ZOOM_MAX) zoomX = ZOOM_MAX;
		if (!(zoomY >= ZOOM_MIN)) zoomY = (zoomY != zoomY) ? 1.0f : ZOOM_MIN;
		if (zoomY > ZOOM_MAX) zoomY = ZOOM_MAX;
		if (shiftX < SHIFT_MIN) shiftX = SHIFT_MIN;
		if (shiftX > SHIFT_MAX) shiftX = SHIFT_MAX;
		if (shiftY < SHIFT_MIN) shiftY = SHIFT_MIN;
		if (shiftY > SHIFT_MAX) shiftY = SHIFT_MAX;
		if (!(scanlines >= 0.0f)) scanlines = 0.0f;
		if (scanlines > 1.0f) scanlines = 1.0f;
	}

	bool operator==(const EmulatorVideoSettings& o) const
	{
		return fit == o.fit && aspect == o.aspect && filter == o.filter
			&& widescreen == o.widescreen && scanlines == o.scanlines
			&& zoomX == o.zoomX && zoomY == o.zoomY
			&& shiftX == o.shiftX && shiftY == o.shiftY;
	}
	bool operator!=(const EmulatorVideoSettings& o) const { return !(*this == o); }
};

//! What this driver can actually do, so a menu can hide or disable rows
//! instead of assuming
struct EmulatorVideoCapabilities
{
	bool scanlines = false;
	bool sharpFilter = false;
	//! True where VideoWidescreen has any effect (Wii). False where each
	//! output's real shape is known (Wii U), so the setting is ignored.
	bool widescreenSetting = false;
};
