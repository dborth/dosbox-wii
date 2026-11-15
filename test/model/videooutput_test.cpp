// Host tests of the video output mode list (drivers/VideoOutputModes.h) and how it reaches the settings.
// The config parsing itself (preferences.cpp) includes core headers and cannot be built here: it is
// VideoOutput_Find() on the property's string, which is what these check.
//
// Set LIBOGC2_HEADER to libogc2's include/ogc/video_types.h to also check the list against it.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <set>
#include <fstream>
#include "drivers/EmulatorVideoSettings.h"

static int checks = 0, failures = 0;

#define CHECK(cond) do { checks++; if(!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while(0)

static bool Near(float a, float b) { return fabsf(a - b) < 0.01f; }

static std::string Upper(std::string s) { for(size_t i = 0; i < s.size(); i++) s[i] = (char) toupper((unsigned char) s[i]); return s; }

int main()
{
	const int count = VideoOutput_Count();

	// libogc2 declares 57 stock modes: 12 each of NTSC, MPAL and EuRGB60, 21 of PAL (it has no 240 / 480 line ones)
	CHECK(count == 57);

	std::set<std::string> names, values, labels;
	int menu = 0, ntsc = 0, mpal = 0, pal = 0, eurgb60 = 0;

	for(int i = 0; i < count; i++)
	{
		const std::string name = VideoOutput_Name(i);
		const std::string value = VideoOutput_Value(i);

		CHECK(names.insert(name).second);
		CHECK(values.insert(value).second);
		CHECK(value != "auto");

		// the value is the identifier in lower case, and only that
		CHECK(value.size() == name.size());
		for(size_t c = 0; c < value.size(); c++)
			CHECK(value[c] == tolower((unsigned char) name[c]));

		if(name.compare(0, 4, "Ntsc") == 0) ntsc++;
		else if(name.compare(0, 4, "Mpal") == 0) mpal++;
		else if(name.compare(0, 9, "Eurgb60Hz") == 0) eurgb60++;
		else if(name.compare(0, 3, "Pal") == 0) pal++;
		else CHECK(!"unknown family");

		// every entry can be told apart in the menu, and a config-only one is shown by its name
		CHECK(labels.insert(VideoOutput_Label(i)).second);
		if(VideoOutput_InMenu(i)) menu++;
		else CHECK(name == VideoOutput_Label(i));

		// every config value maps back to its mode, in any case
		CHECK(VideoOutput_Find(value.c_str()) == i);
		CHECK(VideoOutput_Find(name.c_str()) == i);
		CHECK(VideoOutput_Find(Upper(name).c_str()) == i);
	}

	CHECK(ntsc == 12 && mpal == 12 && eurgb60 == 12 && pal == 21);
	CHECK(menu == 9);

	// the 240 line modes the user asked for are on the page
	CHECK(VideoOutput_InMenu(VideoOutput_Find("ntsc240ds")));
	CHECK(VideoOutput_InMenu(VideoOutput_Find("eurgb60hz240ds")));
	CHECK(VideoOutput_InMenu(VideoOutput_Find("pal288dsscale")));
	CHECK(!VideoOutput_InMenu(VideoOutput_Find("ntsc480ints")));

	// the list the property is declared with: "auto" first, every mode once, then the end marker
	const char * const * list = VideoOutput_Values();
	CHECK(strcmp(list[0], "auto") == 0);
	int n = 0;
	while(list[n]) n++;
	CHECK(n == count + 1);
	for(int i = 0; i < count; i++)
		CHECK(strcmp(list[i + 1], VideoOutput_Value(i)) == 0);

	// auto, a value that is not a mode, and no value are all "no choice"
	CHECK(VideoOutput_Find("auto") == VIDEO_OUTPUT_AUTO);
	CHECK(VideoOutput_Find("AUTO") == VIDEO_OUTPUT_AUTO);
	CHECK(VideoOutput_Find("") == VIDEO_OUTPUT_AUTO);
	CHECK(VideoOutput_Find("bogus") == VIDEO_OUTPUT_AUTO);
	CHECK(VideoOutput_Find("TVNtsc480IntDf") == VIDEO_OUTPUT_AUTO);	// the prefix is not part of the name
	CHECK(VideoOutput_Find("ntsc480intdf ") == VIDEO_OUTPUT_AUTO);
	CHECK(VideoOutput_Find(NULL) == VIDEO_OUTPUT_AUTO);
	CHECK(strcmp(VideoOutput_Value(VIDEO_OUTPUT_AUTO), "auto") == 0);
	CHECK(strcmp(VideoOutput_Value(count), "auto") == 0);

	// Settings: default is auto, a bad index is forced back, and the field counts as a change
	EmulatorVideoSettings s;
	CHECK(s.output == VIDEO_OUTPUT_AUTO);
	s.output = count;
	s.clamp();
	CHECK(s.output == VIDEO_OUTPUT_AUTO);
	s.output = -7;
	s.clamp();
	CHECK(s.output == VIDEO_OUTPUT_AUTO);
	s.output = count - 1;
	s.clamp();
	CHECK(s.output == count - 1);

	EmulatorVideoSettings a, b;
	CHECK(a == b);
	b.output = VideoOutput_Find("ntsc240ds");
	CHECK(a != b);

	// What the output mode does not touch
	EmulatorVideoCapabilities caps;
	CHECK(!caps.outputModes);		// off unless a driver says so: Wii U stays as it was
	CHECK(VideoOutputInfo().requested == VIDEO_OUTPUT_AUTO && !VideoOutputInfo().fellBack);

	// Moving the frame onto the menu's canvas
	const VideoExtent full = { 0.0f, 1.0f };
	const VideoExtent pal264 = { 24.0f / 576.0f, 528.0f / 576.0f };	// TVPal264Ds: 528 of 576 lines, origin 12 field lines
	float y, h;

	// the same mode: untouched, exactly
	y = 37.0f; h = 410.0f;
	VideoOutput_RemapCanvasY(full, full, 480.0f, &y, &h);
	CHECK(y == 37.0f && h == 410.0f);
	y = 37.0f; h = 410.0f;
	VideoOutput_RemapCanvasY(pal264, pal264, 480.0f, &y, &h);
	CHECK(y == 37.0f && h == 410.0f);

	// the whole 264p canvas is 528 lines starting 24 lines down: on the menu's 576 that is 20..460 of 480
	y = 0.0f; h = 480.0f;
	VideoOutput_RemapCanvasY(pal264, full, 480.0f, &y, &h);
	CHECK(Near(y, 20.0f) && Near(h, 440.0f));

	// the other way, the menu's canvas is the bigger one: the frame grows past it
	y = 0.0f; h = 480.0f;
	VideoOutput_RemapCanvasY(full, pal264, 480.0f, &y, &h);
	CHECK(Near(h, 480.0f * 576.0f / 528.0f) && Near(y, -(24.0f / 528.0f) * 480.0f));

	// and back again lands where it started
	VideoOutput_RemapCanvasY(pal264, full, 480.0f, &y, &h);
	CHECK(Near(y, 0.0f) && Near(h, 480.0f));

	// the middle of the screen stays the middle when both are centred
	const VideoExtent centred = { 24.0f / 576.0f, 528.0f / 576.0f };
	y = 235.0f; h = 10.0f;
	VideoOutput_RemapCanvasY(centred, full, 480.0f, &y, &h);
	CHECK(Near(y + h / 2.0f, 240.0f));

	// a mode that shows nothing leaves the rect alone
	const VideoExtent none = { 0.0f, 0.0f };
	y = 5.0f; h = 6.0f;
	VideoOutput_RemapCanvasY(full, none, 480.0f, &y, &h);
	CHECK(y == 5.0f && h == 6.0f);
	VideoOutput_RemapCanvasY(none, full, 480.0f, &y, &h);
	CHECK(y == 5.0f && h == 6.0f);

	// NTSC single field 240p keeps its whole 480 lines (origin 0): the same screen as the menu's 480
	const VideoExtent ntsc240 = { 0.0f, 480.0f / 480.0f };
	y = 60.0f; h = 360.0f;
	VideoOutput_RemapCanvasY(ntsc240, full, 480.0f, &y, &h);
	CHECK(y == 60.0f && h == 360.0f);

	// the list against libogc2's own header, when it is given
	const char * path = getenv("LIBOGC2_HEADER");
	if(path)
	{
		std::ifstream in(path);
		std::string line;
		int found = 0, listed = 0;

		CHECK(in.good());
		while(std::getline(in, line))
		{
			const std::string prefix = "extern GXRModeObj TV";
			if(line.compare(0, prefix.size(), prefix) != 0)
				continue;
			const size_t end = line.find(';', prefix.size());
			if(end == std::string::npos)
				continue;
			const std::string name = line.substr(prefix.size(), end - prefix.size());
			found++;
			// in the same order, as the table is expanded into the modes
			CHECK(found <= count && name == VideoOutput_Name(found - 1));
			listed++;
		}
		CHECK(listed == count);
		printf("compared with %s: %d modes\n", path, found);
	}
	else
		printf("LIBOGC2_HEADER not set: list not compared with libogc2\n");

	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
