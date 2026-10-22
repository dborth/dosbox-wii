/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * menu.cpp
 *
 * Menu flow routines - handles all menu logic. Runs on libgui and the
 * platform HAL: single-threaded, the menu loop steps the GUI itself with
 * UpdateGui().
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <unistd.h>

#include "libgui/Gui.h"
#include "drivers/Platform.h"
#include "drivers/AudioDriver.h"
#include "drivers/VideoDriver.h"
#include "drivers/EmulatorVideoDriver.h"
#include "drivers/InputDriver.h"
#include "drivers/InputController.h"
#include "drivers/KeyboardDriver.h"
#include "drivers/MouseDriver.h"
#include "filelist.h"
#include "input.h"
#include "osk.h"
#include "menu.h"
#include "controlsui.h"
#include "controlslive.h"
#include "controlsmapper.h"
#include "settings.h"
#include "dosboxwii.h"
#include "dosboxsupport.h"
#include "menuart.h"
#include "homeview.h"
#include "gamesave.h"
#include "gameslive.h"
#include "menupopover.h"

static GuiImageData * pointer[4] = { NULL, NULL, NULL, NULL };
static GuiImage cursorImg[4];
GuiWindow * mainWindow = NULL;

/****************************************************************************
 * FadeOutAndExit
 *
 * Fades out and leaves the application. The leaving is ExitApp()'s (main.cpp);
 * this only adds the fade. Does not return, so nothing above it on the stack
 * is unwound; that is fine, the platform is shut down.
 ***************************************************************************/
static void FadeOutAndExit()
{
	VideoDriver * video = platform->getVideo();

	for(int a = 0; a <= 255; a += 15)
	{
		mainWindow->draw();
		video->getImageRenderer()->drawRectangle(0, 0, video->getScreenWidth(),
			video->getScreenHeight(), (PixelColor){0, 0, 0, (uint8_t)a});
		video->renderMenu();
	}

	platform->requestExit();
}

/****************************************************************************
 * UpdateGui
 *
 * One GUI frame: input, draw, cursors, present. Called in a loop by
 * whatever is waiting on the menu. A power button / shutdown request (see
 * Platform::shouldExit) leaves the app from here, so the nested loops
 * (keyboard, credits) never need to unwind.
 ***************************************************************************/
void UpdateGui()
{
	platform->getInput()->update();

	for(int i = 3; i >= 0; i--)
		mainWindow->update(controller[i]);

	mainWindow->draw();

	for(int i = 3; i >= 0; i--) // so that player 1's cursor appears on top!
	{
		const InputPadData & pad = controller[i]->getPadData();

		if(pad.validPointer)
		{
			cursorImg[i].setPosition(pad.cursor_x - cursorImg[i].getWidth() / 2,
				pad.cursor_y - cursorImg[i].getHeight() / 2);
			cursorImg[i].setAngle(pad.cursor_angle);
			cursorImg[i].draw();
		}
	}

	platform->getVideo()->renderMenu();

	if(platform->shouldExit())
		FadeOutAndExit();
}

/****************************************************************************
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable. The text in the variable is what the keyboard
 * starts with. Returns false when the user cancelled.
 ***************************************************************************/
bool OnScreenKeyboard(char * var, uint32_t maxlen)
{
	int save = -1;

	// GuiKeyboard edits its own 256 byte buffer, so it must not be told it has more
	const uint32_t kbLimit = (uint32_t)sizeof(GuiKeyboard::kbtextstr);
	// declared before kb, so it is still there when kb lets go of its children
	MenuKeyboardBox keyboardBox;
	GuiKeyboard kb(var, maxlen < kbLimit ? maxlen : kbLimit);

	GuiTrigger trigA;
	trigA.setPrimaryTrigger();

	// OK and Cancel go black under the cursor (white words on a dark face), as the bar buttons do
	const PixelColor white = MenuChromeText();
	GuiText okBtnTxt("OK", 24, (PixelColor){0, 0, 0, 255});
	GuiText okBtnTxtOver("OK", 24, white);
	MenuButtonShape okBtnImg(MenuButtonShape::NORMAL);
	okBtnImg.setStyle(MenuBarButtonStyle());
	GuiButton okBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	okBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	okBtn.setPosition(25, -25);
	okBtn.setLabel(&okBtnTxt);
	okBtn.setLabelOver(&okBtnTxtOver);
	okBtn.setImage(&okBtnImg);
	okBtn.setTrigger(&trigA);
	okBtn.setEffectGrow();

	GuiText cancelBtnTxt("Cancel", 24, (PixelColor){0, 0, 0, 255});
	GuiText cancelBtnTxtOver("Cancel", 24, white);
	MenuButtonShape cancelBtnImg(MenuButtonShape::NORMAL);
	cancelBtnImg.setStyle(MenuBarButtonStyle());
	GuiButton cancelBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	cancelBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	cancelBtn.setPosition(-25, -25);
	cancelBtn.setLabel(&cancelBtnTxt);
	cancelBtn.setLabelOver(&cancelBtnTxtOver);
	cancelBtn.setImage(&cancelBtnImg);
	cancelBtn.setTrigger(&trigA);
	cancelBtn.setEffectGrow();

	// the box goes behind the keys; the buttons are destroyed before kb: unregister them on destruction
	kb.insert(&keyboardBox, 0);
	kb.appendWithAutoRemove(&okBtn);
	kb.appendWithAutoRemove(&cancelBtn);

	mainWindow->setState(STATE::DISABLED);
	mainWindow->append(&kb);
	mainWindow->changeFocus(&kb);

	while(save == -1)
	{
		UpdateGui();

		if(okBtn.getState() == STATE::CLICKED)
			save = 1;
		else if(cancelBtn.getState() == STATE::CLICKED)
			save = 0;
	}

	if(save)
		snprintf(var, maxlen, "%s", kb.kbtextstr);

	mainWindow->remove(&kb);
	mainWindow->setState(STATE::DEFAULT);
	return save != 0;
}


/****************************************************************************
 * SaveGame
 *
 * The Game bar's Save. With a game running, the running config and buttons are written back to that game (its
 * startup and title kept). At the plain prompt it asks for a name, with the program that is running as the
 * suggestion, and saves a new game: DOSBox is then running that game, so a restart keeps it. A name that is taken
 * asks before replacing it. Returns what to say on the hint line; empty when the name was cancelled.
 ***************************************************************************/
static std::string SaveGame()
{
	GameSaveHost& host = GameSave_Live();
	const char * given = GetGameConf();

	if(given && given[0])
		return Game_SaveActive(host, given).message;

	char typed[64];
	char program[64];
	std::string suggestion = "My game";

	if(GetRunningProgram(program, sizeof(program)))
	{
		std::string stem = program;
		const size_t dot = stem.rfind('.');
		if(dot != std::string::npos)
			stem.resize(dot);
		stem = Game_FileName(stem);
		if(!stem.empty())
			suggestion = stem;
	}
	snprintf(typed, sizeof(typed), "%s", suggestion.c_str());

	if(!OnScreenKeyboard(typed, sizeof(typed)))
		return std::string();

	GameSaveResult r = Game_SaveNew(host, typed);

	if(r.exists)
	{
		std::vector<std::string> answers;
		answers.push_back("Replace it");
		answers.push_back("Cancel");
		if(MenuPopover_Ask(r.message.c_str(), answers,
			"Replacing it overwrites its saved controls, settings and startup with what is running.", GuiPopover::Anchor()) != 0)
			return "Not saved.";
		r = Game_SaveReplace(host, r.path, false);
	}

	if(r.ok)
		SetGameConf(r.path.c_str());
	return r.message;
}

/****************************************************************************
 * WindowCredits
 * Display credits, legal copyright and licence
 *
 * THIS MUST NOT BE REMOVED OR DISABLED IN ANY DERIVATIVE WORK
 ***************************************************************************/
static void WindowCredits()
{
	bool exit = false;
	int i = 0;
	int y = 0;

	GuiWindow creditsWindow(MENU_CREDITS_W, MENU_CREDITS_H);
	creditsWindow.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	MenuDialogShape creditsBoxImg(MENU_CREDITS_W, MENU_CREDITS_H, MENU_CREDITS_HEADER_H);
	creditsWindow.appendWithAutoRemove(&creditsBoxImg); // destroyed before the window

	const int numEntries = 17;
	GuiText * txt[numEntries];

	const PixelColor black = {0, 0, 0, 255};
	const PixelColor white = {255, 255, 255, 255};
	const PixelColor gray = {96, 96, 96, 255};

	auto addLine = [&](const char * text, int size, PixelColor color, int lineY)
	{
		txt[i] = new GuiText(text, size, color);
		txt[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
		txt[i]->setPosition(0, lineY);
		i++;
	};

	addLine("Credits", 30, white, MENU_CREDITS_TITLE_Y);

	addLine("https://github.com/dborth/dosbox-wii/", 16, gray, 74);

	struct Credit { const char * role; const char * name; };
	static const Credit credits[] = {
		{"Porting & Menu Coding",	"Tantric"},
		{"PowerPC Dynarec",			"jmarsh"},
		{"DOSBox/dynarec Updates",	"retro100"},
		{"DOSBox",					"The DOSBox Team"},
		{"devkitPPC & libogc2",		"shagkur, wintermute & Extrems"},
	};

	y = 108;
	for(const Credit & c : credits)
	{
		addLine(c.role, 15, gray, y);
		addLine(c.name, 22, black, y + 18);
		y += 46;
	}

	y += 12;

	GuiText::setPresets(12, black, 0, GUI_TEXT_JUSTIFY_CENTER | GUI_TEXT_ALIGN_TOP, ALIGN_H::CENTRE, ALIGN_V::TOP);

	txt[i] = new GuiText("This software is open source and may be copied, distributed, or modified");
	txt[i]->setPosition(0,y); i++; y+=20;

	txt[i] = new GuiText("under the terms of the GNU General Public License (GPL) Version 2.");
	txt[i]->setPosition(0,y); i++; y+=20;

	char consoleDetails[64];
	char memoryFreeInfo[50];
	char jitInfo[24];

	snprintf(consoleDetails, sizeof(consoleDetails), "%s", platform->getConsoleDetails());
	snprintf(memoryFreeInfo, sizeof(memoryFreeInfo), "%s", platform->getMemoryFreeInfo());

	EmulationStatus status;
	const char * jit = "Unknown";
	if(GetEmulationStatus(&status) && status.core[0])
		jit = strcmp(status.core, "dynamic") == 0 ? "Enabled" : "Disabled";
	snprintf(jitInfo, sizeof(jitInfo), "JIT: %s", jit);

	txt[i] = new GuiText(consoleDetails, 14, (PixelColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	txt[i]->setPosition(-20,-52); i++;

	txt[i] = new GuiText(memoryFreeInfo, 14, (PixelColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	txt[i]->setPosition(-20,-36); i++;

	txt[i] = new GuiText(jitInfo, 14, (PixelColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	txt[i]->setPosition(-20,-20); i++;

	for(i=0; i < numEntries; i++)
		creditsWindow.append(txt[i]);

	mainWindow->setState(STATE::DISABLED);
	mainWindow->append(&creditsWindow);
	mainWindow->changeFocus(&creditsWindow);

	while(!exit)
	{
		UpdateGui();

		for(i=0; i < 4; i++)
		{
			if(controller[i]->getPadData().buttons_d)
				exit = true;
		}
	}

	mainWindow->remove(&creditsWindow);
	mainWindow->setState(STATE::DEFAULT);

	// the window orphans its children when it is destroyed: it must not be
	// holding the heap-allocated text objects by then
	creditsWindow.removeAll();

	for(i=0; i < numEntries; i++)
		delete txt[i];
}

/****************************************************************************
 * Game background
 *
 * The last emulated frame, placed where it was on screen, dimmed, behind
 * the menu. GFX_Suspend() took the snapshot before the display was handed
 * over; this turns it into a screen-size texture. fillTexture() writes the
 * result straight into the texture, and the snapshot is read a row at a
 * time, so the only memory this needs is the texture and two rows: no
 * screen-size RGBA buffer, no converted copy of the frame.
 ***************************************************************************/
#define BACKGROUND_BRIGHTNESS	0.4f

//! The widest frame the background is built from
#define BACKGROUND_MAX_WIDTH	640

struct BackgroundSource
{
	EmulatorVideoDriver * emu;
	int width, height;		// of the snapshot
	float x, y, w, h;		// where the frame goes on the canvas

	// The two most recently read rows. The texture is filled top to bottom
	// and each pixel needs the source row above and below its position, so
	// every source row is read about twice.
	uint8_t rows[2][BACKGROUND_MAX_WIDTH * 3];
	int rowIndex[2];
	int victim;
};

//! Row y of the snapshot, as packed RGB24. `keep` is a row that must stay
//! valid (the other one the caller is using), or -1.
static const uint8_t * BackgroundRow(BackgroundSource * s, int y, int keep)
{
	for(int i = 0; i < 2; i++)
		if(s->rowIndex[i] == y)
			return s->rows[i];

	const int slot = (s->rowIndex[0] == keep) ? 1 : (s->rowIndex[1] == keep) ? 0 : s->victim;
	s->victim = slot ^ 1;

	if(!s->emu->readFrameRowRGB24(y, s->rows[slot]))
		memset(s->rows[slot], 0, (size_t)s->width * 3);

	s->rowIndex[slot] = y;
	return s->rows[slot];
}

static void BackgroundPixel(int x, int y, PixelColor * out, void * userdata)
{
	BackgroundSource * s = (BackgroundSource *) userdata;
	const float fx = (x + 0.5f - s->x) / s->w;
	const float fy = (y + 0.5f - s->y) / s->h;

	out->a = 255;

	if(fx < 0.0f || fx >= 1.0f || fy < 0.0f || fy >= 1.0f) // bars around the frame
	{
		out->r = out->g = out->b = 0;
		return;
	}

	// bilinear, as the emulator view does
	float sx = fx * s->width - 0.5f;
	float sy = fy * s->height - 0.5f;
	if(sx < 0.0f) sx = 0.0f;
	if(sy < 0.0f) sy = 0.0f;

	const int x0 = (int) sx;
	const int y0 = (int) sy;
	const int x1 = (x0 + 1 < s->width) ? x0 + 1 : x0;
	const int y1 = (y0 + 1 < s->height) ? y0 + 1 : y0;
	const float tx = sx - x0;
	const float ty = sy - y0;

	const uint8_t * row0 = BackgroundRow(s, y0, -1);
	const uint8_t * row1 = BackgroundRow(s, y1, y0);

	const uint8_t * p00 = row0 + x0 * 3;
	const uint8_t * p10 = row0 + x1 * 3;
	const uint8_t * p01 = row1 + x0 * 3;
	const uint8_t * p11 = row1 + x1 * 3;

	uint8_t rgb[3];
	for(int i = 0; i < 3; i++)
	{
		const float top = p00[i] + (p10[i] - p00[i]) * tx;
		const float bottom = p01[i] + (p11[i] - p01[i]) * tx;
		rgb[i] = (uint8_t)((top + (bottom - top) * ty) * BACKGROUND_BRIGHTNESS);
	}

	out->r = rgb[0];
	out->g = rgb[1];
	out->b = rgb[2];
}

//! Returns a texture the caller destroys with ImageRenderer::destroyTexture(),
//! or NULL if there was no frame to show.
static void * CreateGameBackground(int screenwidth, int screenheight)
{
	EmulatorVideoDriver * emu = platform->getVideo()->getEmulatorVideo();
	FrameSnapshotInfo info;

	if(!emu->getSnapshotInfo(&info))
		return NULL;

	if(info.width <= 0 || info.width > BACKGROUND_MAX_WIDTH)
	{
		emu->releaseSnapshot();
		return NULL;
	}

	ImageRenderer * images = platform->getVideo()->getImageRenderer();
	void * texture = images->createTexture(screenwidth, screenheight);

	if(texture)
	{
		BackgroundSource src;
		src.emu = emu;
		src.width = info.width; src.height = info.height;
		src.x = info.x; src.y = info.y; src.w = info.w; src.h = info.h;
		src.rowIndex[0] = src.rowIndex[1] = -1;
		src.victim = 0;
		images->fillTexture(texture, screenwidth, screenheight, BackgroundPixel, &src);
	}

	emu->releaseSnapshot();
	return texture;
}

/****************************************************************************
 * LiveHomeSettings
 *
 * The home screen's quick settings: the running cycles and frameskip (the
 * hotkeys and the old +/- buttons changed these, not the config), and the
 * display choices, which are the Video page's rows.
 ***************************************************************************/
class LiveHomeSettings : public HomeSettings
{
	public:
		// the file name of the game's conf, without its folder and ".conf" (the title is in the file: gameTitle() reads it)
		std::string gameName() const override
		{
			std::string name = GetGameConf();
			const size_t slash = name.find_last_of("/\\");
			if(slash != std::string::npos)
				name = name.substr(slash + 1);
			if(name.size() > 5 && strcasecmp(name.c_str() + name.size() - 5, ".conf") == 0)
				name.resize(name.size() - 5);
			return name;
		}

		// the game's title and program, from its conf: read when the game changes, not every frame
		std::string gameTitle() const override { loadGameInfo(); return infoTitle.empty() ? gameName() : infoTitle; }
		std::string gameProgram() const override { loadGameInfo(); return infoProgram; }

		// The game's conf was changed (renamed from the Games screen): read its title again when it is next asked for
		void forgetGame() { infoStale = true; }

		std::string speed() const override
		{
			bool autoAdjust;
			const int cycles = GetRunningCycles(&autoAdjust);
			return GuiFormat(autoAdjust ? "%d%%" : "%d", cycles);
		}

		std::string speedNote() const override
		{
			EmulationStatus status;
			if(GetEmulationStatus(&status) && status.core[0])
			{
				if(strcmp(status.core, "dynamic") == 0) return "Dynamic core";
				if(strcmp(status.core, "normal") == 0) return "Normal core";
				return "Full core";
			}
			return "CPU speed";
		}

		void stepSpeed(bool up) override
		{
			MENU_CycleIncreaseOrDecrease(up);
			Settings_SyncRunning();
		}

		std::string frameskip() const override { return GuiFormat("%d", GetRunningFrameskip()); }

		void stepFrameskip(bool up) override
		{
			MENU_IncreaseOrDecreaseFrameSkip(up);
			Settings_SyncRunning();
		}

		bool aspectOriginal() const override { return !strcasecmp(value("Aspect").c_str(), "original"); }
		void setAspectOriginal(bool original) override { choose("Aspect", original ? "original" : "uncorrected", nullptr); }

		bool filterSharp() const override
		{
			const std::string v = value("Filter");
			return !strcasecmp(v.c_str(), "sharp") || !strcasecmp(v.c_str(), "nearest");
		}

		void setFilterSharp(bool sharp) override { choose("Filter", sharp ? "sharp" : "bilinear", sharp ? "nearest" : nullptr); }

	private:
		void loadGameInfo() const
		{
			const std::string conf = GetGameConf() ? GetGameConf() : "";
			if(conf == infoConf && !infoStale)
				return;
			infoStale = false;
			infoConf = conf;
			infoTitle.clear();
			infoProgram.clear();
			if(conf.empty())
				return;
			// the path as given, or in the DOSBox folder: the same two places LoadPrefs() looks
			GamesHost& host = Games_Live();
			std::string text;
			if(!host.read(conf, text, 64 * 1024) && !host.read(host.directory() + "/" + conf, text, 64 * 1024))
				return;
			GameRecord game;
			game.name = gameName();
			Game_ParseConf(text, game);
			infoTitle = game.title;
			infoProgram = game.program;
		}

		mutable std::string infoConf, infoTitle, infoProgram;
		mutable bool infoStale = false;
		//! The Video page's row with this label, if it is there
		static bool find(const char * label, int& page, int& row)
		{
			for(int p = 0; p < SETTINGS_PAGE_COUNT; p++)
			{
				if(strcmp(Settings_PageTitle(p), "Video") != 0)
					continue;
				for(int r = 0; r < Settings_RowCount(p); r++)
				{
					if(strcmp(Settings_RowLabel(p, r), label) == 0)
					{
						page = p;
						row = r;
						return true;
					}
				}
			}
			return false;
		}

		static std::string value(const char * label)
		{
			int page, row;
			char buf[64] = { 0 };
			if(find(label, page, row))
				Settings_RowValue(page, row, buf, sizeof(buf));
			return buf;
		}

		//! Steps the row until it shows `wanted` (or `alternative`, where one is as good): a row only steps through its values
		static void choose(const char * label, const char * wanted, const char * alternative)
		{
			int page, row;
			if(!find(label, page, row))
				return;
			for(int i = 0; i < 4; i++)
			{
				char buf[64] = { 0 };
				Settings_RowValue(page, row, buf, sizeof(buf));
				if(!strcasecmp(buf, wanted) || (alternative && !strcasecmp(buf, alternative)))
					return;
				if(!Settings_RowStep(page, row, 1))
					return;
			}
		}
};

/****************************************************************************
 * HomeMenu
 ***************************************************************************/
void HomeMenu ()
{
	// Declared first so it runs last, after every menu element below has let go of its mesh:
	// gives the cached shape meshes (and the video backend's copies) back before the game runs
	struct MeshCacheFlush { ~MeshCacheFlush() { GuiMeshCache::flush(); } } meshCacheFlush;

	// the hotkeys change the cycles and frameskip without the config; the
	// settings pages and Save work from the config
	Settings_SyncRunning();

	VideoDriver * video = platform->getVideo();
	const int screenwidth = video->getScreenWidth();
	const int screenheight = video->getScreenHeight();

	MenuApplyTheme(); // the shared widgets follow the palette

	pointer[0] = new GuiImageData(player1_point_png);
	pointer[1] = new GuiImageData(player2_point_png);
	pointer[2] = new GuiImageData(player3_point_png);
	pointer[3] = new GuiImageData(player4_point_png);

	for(int i = 0; i < 4; i++)
		cursorImg[i].setImage(pointer[i]);

	mainWindow = new GuiWindow(screenwidth, screenheight);

	// The last emulated frame, if there was one; otherwise a plain backdrop
	// (renderMenu() clears the EFB every frame, so there is nothing on screen
	// to show through)
	void * backgroundTexture = CreateGameBackground(screenwidth, screenheight);
	GuiImage * screenImg;

	if(backgroundTexture)
		screenImg = new GuiImage((uint8_t *) backgroundTexture, screenwidth, screenheight);
	else
		screenImg = new GuiImage(screenwidth, screenheight, (PixelColor){50, 50, 50, 255});

	screenImg->setStripe(30);

	GuiTrigger trigA;
	trigA.setPrimaryTrigger();

	GuiTrigger trigHome;
	trigHome.setButtonOnlyTrigger(-1, INPUT_BTN_HOME);

	MenuBarShape bgTopImg(MenuBarShape::TOP);
	MenuBarShape bgBottomImg(MenuBarShape::BOTTOM);

	// the controller screens read the real controllers, keyboard and mouse, and what a button does is what the
	// mapper has for it (all four players); the model keeps its state between visits
	static MapperControlsModel mapperModel(Controls_Hal(), Mapper_Live(), Controls_RunningProgram);
	static int modelRun = 0;
	if(modelRun != RunNumber())
	{
		// DOSBox has restarted since: its mapper is new, so what the model knew of the last one (the preset each
		// player was taken to start from) is not true of this one
		mapperModel.forget();
		modelRun = RunNumber();
	}
	Controls_SetModel(&mapperModel);

	LiveHomeSettings homeSettings;
	HomeView home(homeSettings, APPVERSION);

	GuiWindow w(screenwidth, screenheight);
	w.append(&home);

	// the bars belong to the main window, not to the home screen, so they stay up while the pages (settings,
	// controllers) come and go on top of the game picture
	mainWindow->append(screenImg);
	mainWindow->append(&bgTopImg);
	mainWindow->append(&bgBottomImg);
	mainWindow->append(&w);

	bgTopImg.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
	bgBottomImg.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
	w.setEffect(EFFECT::FADE, 15);

	while(1)
	{
		UpdateGui();

		keyboard->refresh();	// nothing polls it here, so hot-plug would otherwise go unseen

		HomeView::Action action = home.takeAction();

		// HOME closes the menu, as Resume does
		for(int i = 0; i < 4; i++)
			if(trigHome.isClicked(controller[i]))
				action = HomeView::Action::RESUME;

		if(action == HomeView::Action::NONE)
			continue;
		home.setStatus(std::string());

		// Restart closes the menu like Resume; GFX_Events() sees the request once the machine has been handed
		// back (memory, audio and video restored), and leaves the core from there
		if(action == HomeView::Action::RESTART)
		{
			RequestRestart();
			action = HomeView::Action::RESUME;
		}

		// A game chosen from the list restarts DOSBox: the menu closes like Resume, and GFX_Events() does the rest
		if(action == HomeView::Action::GAMES)
		{
			GamesMenu(&w);
			homeSettings.forgetGame();		// it may have been renamed there
			action = RestartRequested() ? HomeView::Action::RESUME : HomeView::Action::NONE;
			if(action == HomeView::Action::NONE)
				continue;
		}

		// Save writes the running DOSBox to its game, or to a new one; the hint line says how it went
		if(action == HomeView::Action::SAVE)
		{
			home.setStatus(SaveGame());
			home.select(HomeView::Action::SAVE);
			continue;
		}

		bool leave = false;

		switch(action)
		{
			case HomeView::Action::RESUME:
				bgTopImg.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
				bgBottomImg.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
				w.setEffect(EFFECT::FADE, -15);

				// step the GUI until the effects have finished
				while(w.getEffect() > 0)
					UpdateGui();
				leave = true;
				break;

			case HomeView::Action::EXIT:
				FadeOutAndExit(); // does not return
				break;

			case HomeView::Action::KEYBOARD:
			{
				// typed into DOS once the menu is closed and the emulator polls again
				char command[1024] = { 0 };
				OnScreenKeyboard(command, sizeof(command));

				if(command[0] != 0)
				{
					OSK_TypeText(command);
					leave = true;
				}
				break;
			}

			case HomeView::Action::CONTROLLERS:
				Controls_Menu(&w, home.takePlayer());
				break;

			case HomeView::Action::SETTINGS:
				SettingsMenu(&w);
				break;

			case HomeView::Action::STATUS:
				StatusMenu(&w);
				break;

			case HomeView::Action::CREDITS:
				WindowCredits();
				break;

			default:
				break;
		}

		if(leave)
			break;
	}

	// wait for keys to be depressed (rescanning: the pad data is only as
	// fresh as the last update, and a combo is held longer than a HOME click)
	while(isMenuRequested())
	{
		platform->getInput()->update();
		usleep(10000);
	}

	// elements are stack objects that the window only points at: take them
	// out before the window (and the stack) goes away
	mainWindow->remove(screenImg);
	mainWindow->remove(&bgTopImg);
	mainWindow->remove(&bgBottomImg);
	mainWindow->remove(&w);
	delete mainWindow;
	mainWindow = NULL;

	// the image only borrows the texture
	delete screenImg;
	if(backgroundTexture)
		platform->getVideo()->getImageRenderer()->destroyTexture(backgroundTexture);
	w.removeAll(); // before the home view (which the window only points at) is destroyed

	for(int i = 0; i < 4; i++)
	{
		cursorImg[i].setImage(nullptr);
		delete pointer[i];
		pointer[i] = nullptr;
	}

	GuiMeshCache::flush();

	// flush() hands the meshes to the video driver, which frees them once the GPU has finished a frame. They live in
	// the menu heap, which goes when EnterMenu() switches the memory mode back, so draw a frame now rather than
	// leave the driver holding pointers into it (it freed them on the next menu entry, into whatever was there)
	platform->getVideo()->renderMenu();
}
