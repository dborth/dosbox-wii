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
#include <sys/param.h>
#include <unistd.h>

#include "libgui/Gui.h"
#include "drivers/Platform.h"
#include "drivers/AudioDriver.h"
#include "drivers/VideoDriver.h"
#include "drivers/EmulatorVideoDriver.h"
#include "drivers/InputDriver.h"
#include "drivers/InputController.h"
#include "filelist.h"
#include "input.h"
#include "menu.h"
#include "settings.h"
#include "memmanager.h"

// Declared here rather than including cpu.h, to keep DOSBox headers out of
// this libgui/HAL file.
extern bool CPU_CycleAutoAdjust;

#define APPVERSION		"2.0"

int MENU_CyclesDisplay = 0;
int MENU_FrameskipDisplay = 0;

static GuiImageData * pointer[4] = { NULL, NULL, NULL, NULL };
static GuiImage cursorImg[4];
static GuiWindow * mainWindow = NULL;

/****************************************************************************
 * ExitApp
 *
 * Fades out and leaves the application. Does not return, so nothing above it
 * on the stack is unwound; that is fine, the platform is shut down.
 ***************************************************************************/
static void ExitApp()
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
static void UpdateGui()
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
		ExitApp();
}

/****************************************************************************
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable.
 ***************************************************************************/
static void OnScreenKeyboard(char * var, uint32_t maxlen)
{
	int save = -1;

	GuiKeyboard kb(var, maxlen);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.setPrimaryTrigger();

	GuiText okBtnTxt("OK", 24, (PixelColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.getWidth(), btnOutline.getHeight());
	okBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	okBtn.setPosition(25, -25);
	okBtn.setLabel(&okBtnTxt);
	okBtn.setImage(&okBtnImg);
	okBtn.setImageOver(&okBtnImgOver);
	okBtn.setSoundOver(&btnSoundOver);
	okBtn.setSoundClick(&btnSoundClick);
	okBtn.setTrigger(&trigA);
	okBtn.setEffectGrow();

	GuiText cancelBtnTxt("Cancel", 24, (PixelColor){0, 0, 0, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.getWidth(), btnOutline.getHeight());
	cancelBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	cancelBtn.setPosition(-25, -25);
	cancelBtn.setLabel(&cancelBtnTxt);
	cancelBtn.setImage(&cancelBtnImg);
	cancelBtn.setImageOver(&cancelBtnImgOver);
	cancelBtn.setSoundOver(&btnSoundOver);
	cancelBtn.setSoundClick(&btnSoundClick);
	cancelBtn.setTrigger(&trigA);
	cancelBtn.setEffectGrow();

	// the buttons are destroyed before kb: unregister them on destruction
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
	int y = 20;

	GuiWindow creditsWindow(528, 408);
	creditsWindow.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	GuiImageData creditsBox(credits_box_png);
	GuiImage creditsBoxImg(&creditsBox);
	creditsBoxImg.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	creditsWindow.appendWithAutoRemove(&creditsBoxImg); // destroyed before the window

	const int numEntries = 10;
	GuiText * txt[numEntries];

	txt[i] = new GuiText("Credits", 30, (PixelColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP); txt[i]->setPosition(0,y); i++; y+=32;

	txt[i] = new GuiText("Official Site: https://github.com/dborth/dosbox-wii/", 20, (PixelColor){0, 0, 0, 255});
	txt[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP); txt[i]->setPosition(0,y); i++; y+=40;

	// presets apply to every GuiText constructed after this call
	GuiText::setPresets(20, (PixelColor){0, 0, 0, 255}, 0,
		GUI_TEXT_JUSTIFY_CENTER | GUI_TEXT_ALIGN_TOP, ALIGN_H::CENTRE, ALIGN_V::TOP);

	txt[i] = new GuiText("Porting & Menu Coding:");
	txt[i]->setPosition(0,y); i++; y+=36;

	txt[i] = new GuiText("Tantric");
	txt[i]->setPosition(0,y); i++; y+=60;

	txt[i] = new GuiText("Thanks to:");
	txt[i]->setPosition(0,y); i++; y+=36;

	txt[i] = new GuiText("DOSBox Team");
	txt[i]->setPosition(0,y); i++; y+=22;

	txt[i] = new GuiText("shagkur & wintermute (libogc / devkitPPC)");
	txt[i]->setPosition(0,y); i++; y+=22;

	y+=60;

	GuiText::setPresets(18, (PixelColor){0, 0, 0, 255}, 0,
		GUI_TEXT_JUSTIFY_CENTER | GUI_TEXT_ALIGN_TOP, ALIGN_H::CENTRE, ALIGN_V::TOP);

	txt[i] = new GuiText("This software is open source and may be copied,");
	txt[i]->setPosition(0,y); i++; y+=20;

	txt[i] = new GuiText("distributed, or modified under the terms of the");
	txt[i]->setPosition(0,y); i++; y+=20;

	txt[i] = new GuiText("GNU General Public License (GPL) Version 2.");
	txt[i]->setPosition(0,y); i++; y+=20;

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

static void updateCyclesText(GuiText * cycleText)
{
	char tmpCyclesTxt[15];

	if (CPU_CycleAutoAdjust)
		sprintf(tmpCyclesTxt, "%d%%", MENU_CyclesDisplay);
	else
		sprintf(tmpCyclesTxt, "%d", MENU_CyclesDisplay);

	cycleText->setText(tmpCyclesTxt);
}

static void updateFskipText(GuiText * fskipText)
{
	char tmpFskipTxt[15];
	sprintf(tmpFskipTxt, "%d", MENU_FrameskipDisplay);
	fskipText->setText(tmpFskipTxt);
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
 * SettingsBrowser
 *
 * A GuiOptionBrowser that says which option is highlighted, which it keeps
 * to itself. The page needs it to show help for that option and to step it
 * with left and right.
 ***************************************************************************/
class SettingsBrowser : public GuiOptionBrowser
{
	public:
		SettingsBrowser(int w, int h, OptionList * l) : GuiOptionBrowser(w, h, l)
		{
			for(int i = 0; i < OPTION_PAGESIZE; i++)
				optionIndex[i] = -1; // not set until the first update()
		}

		//! The highlighted option's index, or -1 until the list has been drawn.
		int getSelectedOption() const { return optionIndex[selectedItem]; }
};

static void RefreshSettingsValues(int page, OptionList * options, GuiOptionBrowser * browser)
{
	for(int i = 0; i < options->length; i++)
		Settings_RowValue(page, i, options->value[i], sizeof(options->value[i]));

	browser->triggerUpdate();
}

static void RefreshSettingsHelp(int page, int row, GuiText * helpTxt)
{
	char help[320];

	Settings_RowHelp(page, row, help, sizeof(help));
	helpTxt->setText(help);
}

/****************************************************************************
 * SettingsPageScreen
 *
 * One page of the settings registry: a row per setting. A (or clicking)
 * moves the row to its next value, left and right step it back and forth.
 * Every change is applied to DOSBox as it is made. Returns when the page
 * is closed.
 ***************************************************************************/
static void SettingsPageScreen(int page)
{
	VideoDriver * video = platform->getVideo();
	const PixelColor white = {255, 255, 255, 255};
	char title[64];

	// about 15 KB: kept off the stack, which this runs deep in the core's call
	// chain on a size nothing here sets. Reset on every entry.
	static OptionList options;

	memset(&options, 0, sizeof(options));

	options.length = Settings_RowCount(page);
	if(options.length > MAX_OPTIONS)
		options.length = MAX_OPTIONS;

	for(int i = 0; i < options.length; i++)
		snprintf(options.name[i], sizeof(options.name[i]), "%s", Settings_RowLabel(page, i));

	snprintf(title, sizeof(title), "Settings - %s", Settings_PageTitle(page));
	GuiText titleTxt(title, 28, white);
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50, 50);

	GuiText helpTxt("", 18, white);
	helpTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	helpTxt.setPosition(0, 366);
	helpTxt.setMaxWidth(540);
	helpTxt.setScroll(SCROLL::HORIZONTAL);

	GuiText hintTxt("A or Left/Right: change", 16, (PixelColor){200, 200, 200, 255});
	hintTxt.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	hintTxt.setPosition(-50, -50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA, trigB;
	trigA.setPrimaryTrigger();
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(100, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(&trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	SettingsBrowser browser(552, 248, &options);
	browser.setPosition(0, 108);
	browser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);

	GuiWindow w(video->getScreenWidth(), video->getScreenHeight());
	w.append(&backBtn);

	RefreshSettingsValues(page, &options, &browser);
	RefreshSettingsHelp(page, 0, &helpTxt);

	// the browser first: it is what takes focus
	mainWindow->append(&browser);
	mainWindow->append(&w);
	mainWindow->append(&titleTxt);
	mainWindow->append(&helpTxt);
	mainWindow->append(&hintTxt);

	int helpRow = 0;
	bool done = false;

	while(!done)
	{
		UpdateGui();

		bool changed = false;
		const int clicked = browser.getClickedOption();

		if(clicked >= 0)
			changed = Settings_RowStep(page, clicked, 1);

		// the browser does not use left and right itself
		if(!changed && browser.isFocused() == 1 && browser.getSelectedOption() >= 0)
		{
			for(int i = 0; i < 4 && !changed; i++)
			{
				if(controller[i]->left())
					changed = Settings_RowStep(page, browser.getSelectedOption(), -1);
				else if(controller[i]->right())
					changed = Settings_RowStep(page, browser.getSelectedOption(), 1);
			}
		}

		// one change can alter other rows (the cycles amount follows the
		// mode, a CPU core may rule out a CPU type), so redo them all
		if(changed)
		{
			RefreshSettingsValues(page, &options, &browser);
			helpRow = -1;
		}

		const int selected = browser.getSelectedOption();

		if(selected >= 0 && selected != helpRow)
		{
			RefreshSettingsHelp(page, selected, &helpTxt);
			helpRow = selected;
		}

		if(backBtn.getState() == STATE::CLICKED)
			done = true;
	}

	mainWindow->remove(&browser);
	mainWindow->remove(&w);
	mainWindow->remove(&titleTxt);
	mainWindow->remove(&helpTxt);
	mainWindow->remove(&hintTxt);
}

/****************************************************************************
 * SettingsList
 *
 * A list of settings pages, one row per page. A (or clicking) opens the page.
 * Returns when the list is closed.
 *
 * Two lists use it. The first (sections = false) is the curated pages followed
 * by "All settings", which opens the second: one row per config section, each
 * opening its all-settings page. The second one is opened from inside the
 * first, so each level has its own OptionList.
 ***************************************************************************/
static void SettingsList(bool sections)
{
	VideoDriver * video = platform->getVideo();
	const PixelColor white = {255, 255, 255, 255};

	// kept off the stack for the same reason as the page's list, and not
	// static because a second list is opened while this one is up
	OptionList * listOptions = new OptionList;
	OptionList & options = *listOptions;

	memset(&options, 0, sizeof(options));

	const int pageCount = sections ? Settings_AllSectionCount() : SETTINGS_PAGE_COUNT;

	options.length = pageCount + (sections ? 0 : 1);	// the curated list ends with "All settings"
	if(options.length > MAX_OPTIONS)
		options.length = MAX_OPTIONS;

	for(int i = 0; i < options.length; i++)
	{
		if(i < pageCount)
			snprintf(options.name[i], sizeof(options.name[i]), "%s",
				Settings_PageTitle(sections ? Settings_AllPage(i) : i));
		else
			snprintf(options.name[i], sizeof(options.name[i]), "All settings");
	}

	GuiText titleTxt(sections ? "Settings - All settings" : "Settings", 28, white);
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(50, 50);

	GuiText hintTxt("A: open", 16, (PixelColor){200, 200, 200, 255});
	hintTxt.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	hintTxt.setPosition(-50, -50);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA, trigB;
	trigA.setPrimaryTrigger();
	trigB.setSecondaryTrigger();

	GuiText backBtnTxt("Go Back", 22, (PixelColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.getWidth(), btnOutline.getHeight());
	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(100, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setImageOver(&backBtnImgOver);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(&trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	GuiOptionBrowser browser(552, 248, &options);
	browser.setPosition(0, 108);
	browser.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);

	GuiWindow w(video->getScreenWidth(), video->getScreenHeight());
	w.append(&backBtn);

	// the browser first: it is what takes focus
	mainWindow->append(&browser);
	mainWindow->append(&w);
	mainWindow->append(&titleTxt);
	mainWindow->append(&hintTxt);

	bool done = false;

	while(!done)
	{
		UpdateGui();

		const int clicked = browser.getClickedOption();

		if(clicked >= 0 && clicked < options.length)
		{
			// the list stays up behind the page; take it off so only one has focus
			mainWindow->remove(&browser);
			mainWindow->remove(&w);
			mainWindow->remove(&titleTxt);
			mainWindow->remove(&hintTxt);

			if(clicked >= pageCount)
				SettingsList(true);
			else
				SettingsPageScreen(sections ? Settings_AllPage(clicked) : clicked);

			mainWindow->append(&browser);
			mainWindow->append(&w);
			mainWindow->append(&titleTxt);
			mainWindow->append(&hintTxt);
		}

		if(backBtn.getState() == STATE::CLICKED)
			done = true;
	}

	mainWindow->remove(&browser);
	mainWindow->remove(&w);
	mainWindow->remove(&titleTxt);
	mainWindow->remove(&hintTxt);

	delete listOptions;
}

/****************************************************************************
 * SettingsMenu
 *
 * Opens the settings in place of the home screen, which is taken off the
 * main window while they are up: the list of pages, then the chosen page.
 ***************************************************************************/
static void SettingsMenu(GuiWindow * home)
{
	mainWindow->remove(home);
	SettingsList(false);
	mainWindow->append(home);
}

/****************************************************************************
 * HomeMenu
 ***************************************************************************/
void HomeMenu ()
{
	VideoDriver * video = platform->getVideo();
	const int screenwidth = video->getScreenWidth();
	const int screenheight = video->getScreenHeight();

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

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiSound enterSound(enter_ogg, enter_ogg_size, SOUND::OGG);
	GuiSound exitSound(exit_ogg, exit_ogg_size, SOUND::OGG);

	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);
	GuiImageData btnCloseOutline(button_small_png);
	GuiImageData btnCloseOutlineOver(button_small_over_png);

	GuiImageData battery(battery_png);
	GuiImageData batteryRed(battery_red_png);
	GuiImageData batteryBar(battery_bar_png);

	GuiImageData bgTop(bg_top_png);
	GuiImage bgTopImg(&bgTop);
	GuiImageData bgBottom(bg_bottom_png);
	GuiImage bgBottomImg(&bgBottom);
	bgBottomImg.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);

	GuiImageData logo(logo_png);
	GuiImage logoImg(&logo);
	GuiImageData logoOver(logo_over_png);
	GuiImage logoImgOver(&logoOver);
	GuiText logoTxt(APPVERSION, 18, (PixelColor){255, 255, 255, 255});
	logoTxt.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	logoTxt.setPosition(30, 31);
	GuiButton logoBtn(logoImg.getWidth(), logoImg.getHeight());
	logoBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	logoBtn.setPosition(-85, -40);
	logoBtn.setImage(&logoImg);
	logoBtn.setImageOver(&logoImgOver);
	logoBtn.setLabel(&logoTxt);
	logoBtn.setSoundOver(&btnSoundOver);
	logoBtn.setSoundClick(&btnSoundClick);
	logoBtn.setTrigger(&trigA);

	GuiText cycleText("", 20, (PixelColor){255, 255, 255, 255});
	cycleText.setPosition(-215, -180);
	updateCyclesText(&cycleText);

	GuiText fskipText("", 20, (PixelColor){255, 255, 255, 255});
	fskipText.setPosition(-45, -180);
	updateFskipText(&fskipText);

	// the four +/- buttons share their image data
	GuiImageData keyData(keyboard_key_png);
	GuiImageData keyDataOver(keyboard_key_over_png);

	GuiText cycleDecBtnTxt("-", 24, (PixelColor){0, 0, 0, 255});
	GuiImage cycleDecImg(&keyData);
	GuiImage cycleDecOverImg(&keyDataOver);
	GuiButton cycleDecBtn(keyData.getWidth(), keyData.getHeight());
	cycleDecBtn.setImage(&cycleDecImg);
	cycleDecBtn.setImageOver(&cycleDecOverImg);
	cycleDecBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	cycleDecBtn.setPosition(-270, -180);
	cycleDecBtn.setLabel(&cycleDecBtnTxt);
	cycleDecBtn.setTrigger(&trigA);
	cycleDecBtn.setEffectGrow();

	GuiText cycleIncBtnTxt("+", 24, (PixelColor){0, 0, 0, 255});
	GuiImage cycleIncImg(&keyData);
	GuiImage cycleIncOverImg(&keyDataOver);
	GuiButton cycleIncBtn(keyData.getWidth(), keyData.getHeight());
	cycleIncBtn.setImage(&cycleIncImg);
	cycleIncBtn.setImageOver(&cycleIncOverImg);
	cycleIncBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	cycleIncBtn.setPosition(-160, -180);
	cycleIncBtn.setLabel(&cycleIncBtnTxt);
	cycleIncBtn.setTrigger(&trigA);
	cycleIncBtn.setEffectGrow();

	GuiText fskipDecBtnTxt("-", 24, (PixelColor){0, 0, 0, 255});
	GuiImage fskipDecImg(&keyData);
	GuiImage fskipDecOverImg(&keyDataOver);
	GuiButton fskipDecBtn(keyData.getWidth(), keyData.getHeight());
	fskipDecBtn.setImage(&fskipDecImg);
	fskipDecBtn.setImageOver(&fskipDecOverImg);
	fskipDecBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	fskipDecBtn.setPosition(-80, -180);
	fskipDecBtn.setLabel(&fskipDecBtnTxt);
	fskipDecBtn.setTrigger(&trigA);
	fskipDecBtn.setEffectGrow();

	GuiText fskipIncBtnTxt("+", 24, (PixelColor){0, 0, 0, 255});
	GuiImage fskipIncImg(&keyData);
	GuiImage fskipIncOverImg(&keyDataOver);
	GuiButton fskipIncBtn(keyData.getWidth(), keyData.getHeight());
	fskipIncBtn.setImage(&fskipIncImg);
	fskipIncBtn.setImageOver(&fskipIncOverImg);
	fskipIncBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	fskipIncBtn.setPosition(0, -180);
	fskipIncBtn.setLabel(&fskipIncBtnTxt);
	fskipIncBtn.setTrigger(&trigA);
	fskipIncBtn.setEffectGrow();

	GuiText exitBtnTxt("Exit", 24, (PixelColor){0, 0, 0, 255});
	GuiImage exitBtnImg(&btnLargeOutline);
	GuiImage exitBtnImgOver(&btnLargeOutlineOver);
	GuiButton exitBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	exitBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	exitBtn.setPosition(-125, 120);
	exitBtn.setLabel(&exitBtnTxt);
	exitBtn.setImage(&exitBtnImg);
	exitBtn.setImageOver(&exitBtnImgOver);
	exitBtn.setSoundOver(&btnSoundOver);
	exitBtn.setSoundClick(&btnSoundClick);
	exitBtn.setTrigger(&trigA);
	exitBtn.setEffectGrow();

	GuiText keyboardBtnTxt("Keyboard", 24, (PixelColor){0, 0, 0, 255});
	GuiImage keyboardBtnImg(&btnLargeOutline);
	GuiImage keyboardBtnImgOver(&btnLargeOutlineOver);
	GuiButton keyboardBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	keyboardBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	keyboardBtn.setPosition(125, 120);
	keyboardBtn.setLabel(&keyboardBtnTxt);
	keyboardBtn.setImage(&keyboardBtnImg);
	keyboardBtn.setImageOver(&keyboardBtnImgOver);
	keyboardBtn.setSoundOver(&btnSoundOver);
	keyboardBtn.setSoundClick(&btnSoundClick);
	keyboardBtn.setTrigger(&trigA);
	keyboardBtn.setEffectGrow();

	GuiText settingsBtnTxt("Settings", 24, (PixelColor){0, 0, 0, 255});
	GuiImage settingsBtnImg(&btnLargeOutline);
	GuiImage settingsBtnImgOver(&btnLargeOutlineOver);
	GuiButton settingsBtn(btnLargeOutline.getWidth(), btnLargeOutline.getHeight());
	settingsBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	settingsBtn.setPosition(0, 235);
	settingsBtn.setLabel(&settingsBtnTxt);
	settingsBtn.setImage(&settingsBtnImg);
	settingsBtn.setImageOver(&settingsBtnImgOver);
	settingsBtn.setSoundOver(&btnSoundOver);
	settingsBtn.setSoundClick(&btnSoundClick);
	settingsBtn.setTrigger(&trigA);
	settingsBtn.setEffectGrow();

	GuiText closeBtnTxt("Close", 22, (PixelColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.getWidth(), btnCloseOutline.getHeight());
	closeBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	closeBtn.setPosition(-50, 35);
	closeBtn.setLabel(&closeBtnTxt);
	closeBtn.setImage(&closeBtnImg);
	closeBtn.setImageOver(&closeBtnImgOver);
	closeBtn.setSoundOver(&btnSoundOver);
	closeBtn.setSoundClick(&btnSoundClick);
	closeBtn.setTrigger(0, &trigA);
	closeBtn.setTrigger(1, &trigHome);
	closeBtn.setEffectGrow();

	int i;
	char txt[3];
	bool status[4] = { false, false, false, false };
	int level[4] = { 0, 0, 0, 0 };
	bool newStatus;
	int newLevel;

	GuiText * batteryTxt[4];
	GuiImage * batteryImg[4];
	GuiImage * batteryBarImg[4];
	GuiButton * batteryBtn[4];

	for(i=0; i < 4; i++)
	{
		if(i == 0)
			sprintf(txt, "P %d", i+1);
		else
			sprintf(txt, "P%d", i+1);

		batteryTxt[i] = new GuiText(txt, 22, (PixelColor){255, 255, 255, 255});
		batteryTxt[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		batteryImg[i] = new GuiImage(&battery);
		batteryImg[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		batteryImg[i]->setPosition(30, 0);
		batteryBarImg[i] = new GuiImage(&batteryBar);
		batteryBarImg[i]->setTile(0);
		batteryBarImg[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		batteryBarImg[i]->setPosition(34, 0);

		batteryBtn[i] = new GuiButton(70, 20);
		batteryBtn[i]->setLabel(batteryTxt[i]);
		batteryBtn[i]->setImage(batteryImg[i]);
		batteryBtn[i]->setIcon(batteryBarImg[i]);
		batteryBtn[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
		batteryBtn[i]->setRumble(false);
		batteryBtn[i]->setSelectable(false);
		batteryBtn[i]->setAlpha(150);
	}

	batteryBtn[0]->setPosition(45, -65);
	batteryBtn[1]->setPosition(135, -65);
	batteryBtn[2]->setPosition(45, -40);
	batteryBtn[3]->setPosition(135, -40);

	GuiWindow w(screenwidth, screenheight);
	w.append(&bgTopImg);
	w.append(&bgBottomImg);
	w.append(batteryBtn[0]);
	w.append(batteryBtn[1]);
	w.append(batteryBtn[2]);
	w.append(batteryBtn[3]);
	w.append(&logoBtn);
	w.append(&closeBtn);
	w.append(&exitBtn);
	w.append(&cycleText);
	w.append(&fskipText);
	w.append(&cycleDecBtn);
	w.append(&cycleIncBtn);
	w.append(&fskipDecBtn);
	w.append(&fskipIncBtn);
	w.append(&keyboardBtn);
	w.append(&settingsBtn);

	mainWindow->append(screenImg);
	mainWindow->append(&w);

	enterSound.play();

	bgTopImg.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
	closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
	bgBottomImg.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
	logoBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
	for(i=0; i < 4; i++)
		batteryBtn[i]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
	w.setEffect(EFFECT::FADE, 15);

	while(1)
	{
		UpdateGui();

		for(i=0; i < 4; i++)
		{
			const InputPadData & pad = controller[i]->getPadData();

			if(pad.hw_connected[INPUT_HW_WIIMOTE])
			{
				newStatus = true;
				newLevel = (pad.battery_level / 100.0) * 4;
				if(newLevel > 4) newLevel = 4;
			}
			else
			{
				newStatus = false;
				newLevel = 0;
			}

			if(status[i] != newStatus || level[i] != newLevel)
			{
				if(newStatus == true) // controller connected
				{
					batteryBtn[i]->setAlpha(255);
					batteryBarImg[i]->setTile(newLevel);

					if(newLevel == 0)
						batteryImg[i]->setImage(&batteryRed);
					else
						batteryImg[i]->setImage(&battery);
				}
				else // controller not connected
				{
					batteryBtn[i]->setAlpha(150);
					batteryBarImg[i]->setTile(0);
					batteryImg[i]->setImage(&battery);
				}
				status[i] = newStatus;
				level[i] = newLevel;
			}
		}

		if(closeBtn.getState() == STATE::CLICKED)
		{
			exitSound.play();
			bgTopImg.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			bgBottomImg.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			logoBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			for(i=0; i < 4; i++)
				batteryBtn[i]->setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			w.setEffect(EFFECT::FADE, -15);

			// step the GUI until the effects have finished
			while(w.getEffect() > 0)
				UpdateGui();
			break;
		}
		else if(exitBtn.getState() == STATE::CLICKED)
		{
			ExitApp(); // does not return
		}
		else if(keyboardBtn.getState() == STATE::CLICKED)
		{
			keyboardBtn.resetState();
			OnScreenKeyboard(dosboxCommand, MAXPATHLEN);

			if(dosboxCommand[0] != 0)
				break;
		}
		else if (cycleDecBtn.getState() == STATE::CLICKED)
		{
			cycleDecBtn.resetState();
			MENU_CycleIncreaseOrDecrease(false);
			updateCyclesText(&cycleText);
		}
		else if (cycleIncBtn.getState() == STATE::CLICKED)
		{
			cycleIncBtn.resetState();
			MENU_CycleIncreaseOrDecrease(true);
			updateCyclesText(&cycleText);
		}
		else if (fskipDecBtn.getState() == STATE::CLICKED)
		{
			fskipDecBtn.resetState();
			MENU_IncreaseOrDecreaseFrameSkip(false);
			updateFskipText(&fskipText);
		}
		else if (fskipIncBtn.getState() == STATE::CLICKED)
		{
			fskipIncBtn.resetState();
			MENU_IncreaseOrDecreaseFrameSkip(true);
			updateFskipText(&fskipText);
		}
		else if (settingsBtn.getState() == STATE::CLICKED)
		{
			settingsBtn.resetState();
			SettingsMenu(&w);

			// the settings may have changed what these show
			updateCyclesText(&cycleText);
			updateFskipText(&fskipText);
		}
		else if (logoBtn.getState() == STATE::CLICKED)
		{
			logoBtn.resetState();
			WindowCredits();
		}
	}

	// wait for keys to be depressed
	while(isMenuRequested())
		usleep(10000);

	exitSound.stop();

	// elements are stack objects that the window only points at: take them
	// out before the window (and the stack) goes away
	mainWindow->remove(screenImg);
	mainWindow->remove(&w);
	delete mainWindow;
	mainWindow = NULL;

	// the image only borrows the texture
	delete screenImg;
	if(backgroundTexture)
		platform->getVideo()->getImageRenderer()->destroyTexture(backgroundTexture);
	w.removeAll(); // before the heap-allocated battery elements are deleted

	for(i=0; i < 4; i++)
	{
		delete batteryBtn[i];
		delete batteryImg[i];
		delete batteryBarImg[i];
		delete batteryTxt[i];
	}

	for(int i = 0; i < 4; i++)
	{
		cursorImg[i].setImage(nullptr);
		delete pointer[i];
		pointer[i] = nullptr;
	}
}
