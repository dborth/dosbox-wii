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
#include "osk.h"
#include "menu.h"
#include "controlsui.h"
#include "settings.h"
#include "dosboxsupport.h"
#include "menuart.h"
#include "controllerstatus.h"

#define APPVERSION		"2.0"

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

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);

	GuiTrigger trigA;
	trigA.setPrimaryTrigger();

	GuiText okBtnTxt("OK", 24, (PixelColor){0, 0, 0, 255});
	MenuButtonShape okBtnImg(MenuButtonShape::NORMAL);
	GuiButton okBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	okBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	okBtn.setPosition(25, -25);
	okBtn.setLabel(&okBtnTxt);
	okBtn.setImage(&okBtnImg);
	okBtn.setSoundOver(&btnSoundOver);
	okBtn.setSoundClick(&btnSoundClick);
	okBtn.setTrigger(&trigA);
	okBtn.setEffectGrow();

	GuiText cancelBtnTxt("Cancel", 24, (PixelColor){0, 0, 0, 255});
	MenuButtonShape cancelBtnImg(MenuButtonShape::NORMAL);
	GuiButton cancelBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H);
	cancelBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	cancelBtn.setPosition(-25, -25);
	cancelBtn.setLabel(&cancelBtnTxt);
	cancelBtn.setImage(&cancelBtnImg);
	cancelBtn.setSoundOver(&btnSoundOver);
	cancelBtn.setSoundClick(&btnSoundClick);
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
 * WindowCredits
 * Display credits, legal copyright and licence
 *
 * THIS MUST NOT BE REMOVED OR DISABLED IN ANY DERIVATIVE WORK
 ***************************************************************************/
static void WindowCredits()
{
	bool exit = false;
	int i = 0;
	int y = 14;

	GuiWindow creditsWindow(MENU_CREDITS_W, MENU_CREDITS_H);
	creditsWindow.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	MenuDialogShape creditsBoxImg(MENU_CREDITS_W, MENU_CREDITS_H, MENU_CREDITS_HEADER_H);
	creditsWindow.appendWithAutoRemove(&creditsBoxImg); // destroyed before the window

	const int numEntries = 12;
	GuiText * txt[numEntries];

	// the title sits on the dialog's chrome band, so it is white; the credits start below the band
	txt[i] = new GuiText("Credits", 30, (PixelColor){255, 255, 255, 255});
	txt[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP); txt[i]->setPosition(0, MENU_CREDITS_TITLE_Y); i++; y+=56;

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

	y+=20;

	GuiText::setPresets(12, (PixelColor){0, 0, 0, 255}, 0,
		GUI_TEXT_JUSTIFY_CENTER | GUI_TEXT_ALIGN_TOP, ALIGN_H::CENTRE, ALIGN_V::TOP);

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

static void updateCyclesText(GuiText * cycleText)
{
	char tmpCyclesTxt[15];
	bool autoAdjust;
	const int cycles = GetRunningCycles(&autoAdjust);

	if (autoAdjust)
		sprintf(tmpCyclesTxt, "%d%%", cycles);
	else
		sprintf(tmpCyclesTxt, "%d", cycles);

	cycleText->setText(tmpCyclesTxt);
}

static void updateFskipText(GuiText * fskipText)
{
	char tmpFskipTxt[15];
	sprintf(tmpFskipTxt, "%d", GetRunningFrameskip());
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

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM);
	GuiSound enterSound(enter_ogg, enter_ogg_size, SOUND::OGG);
	GuiSound exitSound(exit_ogg, exit_ogg_size, SOUND::OGG);

	MenuBarShape bgTopImg(MenuBarShape::TOP);
	MenuBarShape bgBottomImg(MenuBarShape::BOTTOM);

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

	GuiText cycleDecBtnTxt("-", 24, (PixelColor){0, 0, 0, 255});
	MenuKeyShape cycleDecImg;
	GuiButton cycleDecBtn(40, 40);
	cycleDecBtn.setImage(&cycleDecImg);
	cycleDecBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	cycleDecBtn.setPosition(-270, -180);
	cycleDecBtn.setLabel(&cycleDecBtnTxt);
	cycleDecBtn.setTrigger(&trigA);
	cycleDecBtn.setEffectGrow();

	GuiText cycleIncBtnTxt("+", 24, (PixelColor){0, 0, 0, 255});
	MenuKeyShape cycleIncImg;
	GuiButton cycleIncBtn(40, 40);
	cycleIncBtn.setImage(&cycleIncImg);
	cycleIncBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	cycleIncBtn.setPosition(-160, -180);
	cycleIncBtn.setLabel(&cycleIncBtnTxt);
	cycleIncBtn.setTrigger(&trigA);
	cycleIncBtn.setEffectGrow();

	GuiText fskipDecBtnTxt("-", 24, (PixelColor){0, 0, 0, 255});
	MenuKeyShape fskipDecImg;
	GuiButton fskipDecBtn(40, 40);
	fskipDecBtn.setImage(&fskipDecImg);
	fskipDecBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	fskipDecBtn.setPosition(-80, -180);
	fskipDecBtn.setLabel(&fskipDecBtnTxt);
	fskipDecBtn.setTrigger(&trigA);
	fskipDecBtn.setEffectGrow();

	GuiText fskipIncBtnTxt("+", 24, (PixelColor){0, 0, 0, 255});
	MenuKeyShape fskipIncImg;
	GuiButton fskipIncBtn(40, 40);
	fskipIncBtn.setImage(&fskipIncImg);
	fskipIncBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	fskipIncBtn.setPosition(0, -180);
	fskipIncBtn.setLabel(&fskipIncBtnTxt);
	fskipIncBtn.setTrigger(&trigA);
	fskipIncBtn.setEffectGrow();

	GuiText exitBtnTxt("Exit", 24, (PixelColor){0, 0, 0, 255});
	MenuButtonShape exitBtnImg(MenuButtonShape::LARGE);
	GuiButton exitBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	exitBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	exitBtn.setPosition(-125, 120);
	exitBtn.setLabel(&exitBtnTxt);
	exitBtn.setImage(&exitBtnImg);
	exitBtn.setSoundOver(&btnSoundOver);
	exitBtn.setSoundClick(&btnSoundClick);
	exitBtn.setTrigger(&trigA);
	exitBtn.setEffectGrow();

	GuiText keyboardBtnTxt("Keyboard", 24, (PixelColor){0, 0, 0, 255});
	MenuButtonShape keyboardBtnImg(MenuButtonShape::LARGE);
	GuiButton keyboardBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	keyboardBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	keyboardBtn.setPosition(125, 120);
	keyboardBtn.setLabel(&keyboardBtnTxt);
	keyboardBtn.setImage(&keyboardBtnImg);
	keyboardBtn.setSoundOver(&btnSoundOver);
	keyboardBtn.setSoundClick(&btnSoundClick);
	keyboardBtn.setTrigger(&trigA);
	keyboardBtn.setEffectGrow();

	GuiText controllersBtnTxt("Controllers", 24, (PixelColor){0, 0, 0, 255});
	MenuButtonShape controllersBtnImg(MenuButtonShape::LARGE);
	GuiButton controllersBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	controllersBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	controllersBtn.setPosition(-125, 235);
	controllersBtn.setLabel(&controllersBtnTxt);
	controllersBtn.setImage(&controllersBtnImg);
	controllersBtn.setSoundOver(&btnSoundOver);
	controllersBtn.setSoundClick(&btnSoundClick);
	controllersBtn.setTrigger(&trigA);
	controllersBtn.setEffectGrow();

	GuiText settingsBtnTxt("Settings", 24, (PixelColor){0, 0, 0, 255});
	MenuButtonShape settingsBtnImg(MenuButtonShape::LARGE);
	GuiButton settingsBtn(MENU_BTN_LARGE_W, MENU_BTN_LARGE_H);
	settingsBtn.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	settingsBtn.setPosition(125, 235);
	settingsBtn.setLabel(&settingsBtnTxt);
	settingsBtn.setImage(&settingsBtnImg);
	settingsBtn.setSoundOver(&btnSoundOver);
	settingsBtn.setSoundClick(&btnSoundClick);
	settingsBtn.setTrigger(&trigA);
	settingsBtn.setEffectGrow();

	GuiText closeBtnTxt("Close", 22, (PixelColor){0, 0, 0, 255});
	MenuButtonShape closeBtnImg(MenuButtonShape::SMALL);
	GuiButton closeBtn(MENU_BTN_SMALL_W, MENU_BTN_SMALL_H);
	closeBtn.setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	closeBtn.setPosition(-50, 29);
	closeBtn.setLabel(&closeBtnTxt);
	closeBtn.setImage(&closeBtnImg);
	closeBtn.setSoundOver(&btnSoundOver);
	closeBtn.setSoundClick(&btnSoundClick);
	closeBtn.setTrigger(0, &trigA);
	closeBtn.setTrigger(1, &trigHome);
	closeBtn.setEffectGrow();

	ControllerStatus controllerStatus(&trigA, &btnSoundOver, &btnSoundClick);
	int controllerClicked;

	GuiWindow w(screenwidth, screenheight);
	controllerStatus.appendTo(w);
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
	w.append(&controllersBtn);
	w.append(&settingsBtn);

	// the bars belong to the main window, not to the home screen, so they stay up while the pages (settings,
	// controllers) come and go on top of the game picture
	mainWindow->append(screenImg);
	mainWindow->append(&bgTopImg);
	mainWindow->append(&bgBottomImg);
	mainWindow->append(&w);

	enterSound.play();

	bgTopImg.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
	closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_IN, 35);
	bgBottomImg.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
	logoBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
	controllerStatus.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_IN, 35);
	w.setEffect(EFFECT::FADE, 15);

	while(1)
	{
		UpdateGui();

		keyboard->refresh();	// nothing polls it here, so hot-plug would otherwise go unseen
		controllerStatus.update();

		if(closeBtn.getState() == STATE::CLICKED)
		{
			exitSound.play();
			bgTopImg.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			closeBtn.setEffect(EFFECT::SLIDE_TOP | EFFECT::SLIDE_OUT, 15);
			bgBottomImg.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			logoBtn.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			controllerStatus.setEffect(EFFECT::SLIDE_BOTTOM | EFFECT::SLIDE_OUT, 15);
			w.setEffect(EFFECT::FADE, -15);

			// step the GUI until the effects have finished
			while(w.getEffect() > 0)
				UpdateGui();
			break;
		}
		else if(exitBtn.getState() == STATE::CLICKED)
		{
			FadeOutAndExit(); // does not return
		}
		else if(keyboardBtn.getState() == STATE::CLICKED)
		{
			keyboardBtn.resetState();

			// typed into DOS once the menu is closed and the emulator polls again
			char command[1024] = { 0 };
			OnScreenKeyboard(command, sizeof(command));

			if(command[0] != 0)
			{
				OSK_TypeText(command);
				break;
			}
		}
		else if (cycleDecBtn.getState() == STATE::CLICKED)
		{
			cycleDecBtn.resetState();
			MENU_CycleIncreaseOrDecrease(false);
			Settings_SyncRunning();
			updateCyclesText(&cycleText);
		}
		else if (cycleIncBtn.getState() == STATE::CLICKED)
		{
			cycleIncBtn.resetState();
			MENU_CycleIncreaseOrDecrease(true);
			Settings_SyncRunning();
			updateCyclesText(&cycleText);
		}
		else if (fskipDecBtn.getState() == STATE::CLICKED)
		{
			fskipDecBtn.resetState();
			MENU_IncreaseOrDecreaseFrameSkip(false);
			Settings_SyncRunning();
			updateFskipText(&fskipText);
		}
		else if (fskipIncBtn.getState() == STATE::CLICKED)
		{
			fskipIncBtn.resetState();
			MENU_IncreaseOrDecreaseFrameSkip(true);
			Settings_SyncRunning();
			updateFskipText(&fskipText);
		}
		else if((controllerClicked = controllerStatus.clicked()) >= 0)
		{
			Controls_Menu(&w, controllerClicked);
		}
		else if (controllersBtn.getState() == STATE::CLICKED)
		{
			controllersBtn.resetState();
			Controls_Menu(&w);
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

	// wait for keys to be depressed (rescanning: the pad data is only as
	// fresh as the last update, and a combo is held longer than a HOME click)
	while(isMenuRequested())
	{
		platform->getInput()->update();
		usleep(10000);
	}

	exitSound.stop();

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
	w.removeAll(); // before the controller strip (which the window only points at) is destroyed

	for(int i = 0; i < 4; i++)
	{
		cursorImg[i].setImage(nullptr);
		delete pointer[i];
		pointer[i] = nullptr;
	}

	GuiMeshCache::flush();
}
