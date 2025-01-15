/****************************************************************************
 * DOSBox Wii Home Menu
 * Tantric 2009-2010
 *
 * menu.cpp
 *
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/param.h>

#include "libwiigui/gui.h"
#include "wiihardware.h"
#include "menu.h"
#include "cpu.h"
#include "vconsole.h"
#include "wiiio.h"

#define THREAD_SLEEP 100
#define APPVERSION 		"1.7"


int MENU_CyclesDisplay = 0;
int MENU_FrameskipDisplay = 0;


static GuiImageData * pointer[4];
static GuiWindow * mainWindow = NULL;
static GuiButton * logoBtn = NULL;

static lwp_t guithread = LWP_THREAD_NULL;
static bool guiHalt = true;
static bool ExitRequested = false;

/****************************************************************************
 * UpdateGUI
 *
 * Primary thread to allow GUI to respond to state changes, and draws GUI
 ***************************************************************************/
static void * UpdateGUI (void *arg)
{
	int i;

	while(1)
	{
		if(guiHalt)
			break;

		UpdatePads();
		mainWindow->Draw();

		for(i=3; i >= 0; i--) // so that player 1's cursor appears on top!
		{
			if(userInput[i].wpad->ir.valid)
				Menu_DrawImg(userInput[i].wpad->ir.x-48, userInput[i].wpad->ir.y-48,
					96, 96, pointer[i]->GetImage(), userInput[i].wpad->ir.angle, 1, 1, 255);
			DoRumble(i);
		}

		Menu_Render();

		for(i=3; i >= 0; i--)
			mainWindow->Update(&userInput[i]);
		
		if(ExitRequested)
		{
			for(i = 0; i <= 255; i += 15)
			{
				mainWindow->Draw();
				Menu_DrawRectangle(0,0,screenwidth,screenheight,(GXColor){0, 0, 0, i},1);
				Menu_Render();
			}
			ShutoffRumble();
			VIDEO_SetBlack(TRUE);
			VIDEO_Flush();
			throw(0);
		}
		usleep(THREAD_SLEEP);
	}
	return NULL;
}

/****************************************************************************
 * ResumeGui
 *
 * Signals the GUI thread to start, and resumes the thread. This is called
 * after finishing the removal/insertion of new elements, and after initial
 * GUI setup.
 ***************************************************************************/
static void
ResumeGui()
{
	guiHalt = false;

	if(guithread == LWP_THREAD_NULL)
		LWP_CreateThread (&guithread, UpdateGUI, NULL, NULL, 0, 66);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the GUI thread to stop, and waits for GUI thread to stop
 * This is necessary whenever removing/inserting new elements into the GUI.
 * This eliminates the possibility that the GUI is in the middle of accessing
 * an element that is being changed.
 ***************************************************************************/
static void
HaltGui()
{
	guiHalt = true;

	if(guithread == LWP_THREAD_NULL)
		return;

	// wait for thread to finish
	LWP_JoinThread(guithread, NULL);
	guithread = LWP_THREAD_NULL;
}

static void OnScreenConsole()
{
	vconsole_t& vc = *wiiio_get_vconsole();
	GuiWindow terminal(screenwidth, screenheight);

	GuiImageData backgroundData(bg_console_png);
	GuiImage backgroundImg(&backgroundData);
	backgroundImg.SetParent(&terminal);
	backgroundImg.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	backgroundImg.SetPosition(23, 38);
	terminal.Append(&backgroundImg);

	GuiImageData fontData(font8x8_basic_png);
	GuiMonoText terminalOutput(&fontData,
			16, //glyphCountX
			6, //glyphCountY
			32, //firstGlyphAsciiCode
			8, //glyphWidth
			8, //glyphHeight
			1, //glyphMarginLeft
			1, //glyphMarginRight
			1, //glyphMarginTop
			1); //glyphMarginBottom
	terminalOutput.SetParent(&terminal);
	terminalOutput.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	terminalOutput.SetPosition(31, 48);
	terminalOutput.SetVirtualConsole(&vc, 2.0f);
	terminal.Append(&terminalOutput);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;

	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiText okBtnTxt("Close", 22, (GXColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(80, -35);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetSoundOver(&btnSoundOver);
	okBtn.SetTrigger(&trigA);
	okBtn.SetEffectGrow();

	terminal.Append(&okBtn);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&terminal);
	mainWindow->ChangeFocus(&terminal);
	ResumeGui();

	for (;;) {
		usleep(THREAD_SLEEP);

		if(okBtn.GetState() == STATE_CLICKED) {
			break;
			// for testing: comment the break and uncomment this.
			//okBtn.ResetState();
			//vconsole_print(&vc, "012345678901234567890123456789");
		}
	}

	HaltGui();
	mainWindow->Remove(&terminal);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
}

/****************************************************************************
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable.
 ***************************************************************************/
static void OnScreenKeyboard(char * var, u32 maxlen)
{
	int save = -1;

	GuiKeyboard keyboard(var, maxlen);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText okBtnTxt("OK", 24, (GXColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(25, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetSoundOver(&btnSoundOver);
	okBtn.SetSoundClick(&btnSoundClick);
	okBtn.SetTrigger(&trigA);
	okBtn.SetEffectGrow();

	GuiText cancelBtnTxt("Cancel", 24, (GXColor){0, 0, 0, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-25, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetSoundOver(&btnSoundOver);
	cancelBtn.SetSoundClick(&btnSoundClick);
	cancelBtn.SetTrigger(&trigA);
	cancelBtn.SetEffectGrow();

	keyboard.Append(&okBtn);
	keyboard.Append(&cancelBtn);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&keyboard);
	mainWindow->ChangeFocus(&keyboard);
	ResumeGui();

	while(save == -1)
	{
		usleep(THREAD_SLEEP);

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}

	if(save)
	{
		snprintf(var, maxlen, "%s", keyboard.kbtextstr);
	}

	HaltGui();
	mainWindow->Remove(&keyboard);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
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

	GuiWindow creditsWindow(528,408);
	creditsWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiImageData creditsBox(credits_box_png);
	GuiImage creditsBoxImg(&creditsBox);
	creditsBoxImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	creditsWindow.Append(&creditsBoxImg);

	int numEntries = 11;
	GuiText * txt[numEntries];

	txt[i] = new GuiText("Credits", 30, (GXColor){0, 0, 0, 255});
	txt[i]->SetAlignment(ALIGN_CENTRE, ALIGN_TOP); txt[i]->SetPosition(0,y); i++; y+=32;

	txt[i] = new GuiText("Official Site: http://code.google.com/p/dosbox-wii/", 20, (GXColor){0, 0, 0, 255});
	txt[i]->SetAlignment(ALIGN_CENTRE, ALIGN_TOP); txt[i]->SetPosition(0,y); i++; y+=40;

	txt[i]->SetPresets(20, (GXColor){0, 0, 0, 255}, 0,
			FTGX_JUSTIFY_CENTER | FTGX_ALIGN_TOP, ALIGN_CENTRE, ALIGN_TOP);

	txt[i] = new GuiText("Porting & Menu Coding:");
	txt[i]->SetPosition(0,y); i++; y+=36;
	txt[i] = new GuiText("Tantric");
	txt[i]->SetPosition(0,y); i++; y+=60;

	txt[i] = new GuiText("Thanks to:");
	txt[i]->SetPosition(0,y); i++; y+=36;

	txt[i] = new GuiText("DOSBox Team");
	txt[i]->SetPosition(0,y); i++; y+=22;

	txt[i] = new GuiText("shagkur & wintermute (libogc / devkitPPC)");
	txt[i]->SetPosition(0,y); i++; y+=22;

	txt[i] = new GuiText("Carl Kenner & Armin Tamzarian");
	txt[i]->SetPosition(0,y); i++; y+=60;

	txt[i]->SetPresets(18, (GXColor){0, 0, 0, 255}, 0,
		FTGX_JUSTIFY_CENTER | FTGX_ALIGN_TOP, ALIGN_CENTRE, ALIGN_TOP);

	txt[i] = new GuiText("This software is open source and may be copied,");
	txt[i]->SetPosition(0,y); i++; y+=20;
	txt[i] = new GuiText("distributed, or modified under the terms of the");
	txt[i]->SetPosition(0,y); i++; y+=20;
	txt[i] = new GuiText("GNU General Public License (GPL) Version 2.");
	txt[i]->SetPosition(0,y); i++; y+=20;

	for(i=0; i < numEntries; i++)
		creditsWindow.Append(txt[i]);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&creditsWindow);
	mainWindow->ChangeFocus(&creditsWindow);
	ResumeGui();
	
	while(!exit)
	{
		for(i=0; i < 4; i++)
		{
			if(userInput[i].wpad->btns_d || userInput[i].pad.btns_d)
				exit = true;
		}
		usleep(THREAD_SLEEP);
	}

	HaltGui();
	mainWindow->Remove(&creditsWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	
	for(i=0; i < numEntries; i++)
		delete txt[i];
}

static void updateCyclesText(GuiText * cycleText)
{
	char tmpCyclesTxt[15];
	if (CPU_CycleAutoAdjust) {
		sprintf(tmpCyclesTxt, "%d%%", MENU_CyclesDisplay);
	}
	else {
		sprintf(tmpCyclesTxt, "%d", MENU_CyclesDisplay);
	}
	cycleText->SetText(tmpCyclesTxt);
}

static void updateFskipText(GuiText * fskipText)
{
	char tmpFskipTxt[15];
	sprintf(tmpFskipTxt, "%d", MENU_FrameskipDisplay);
	fskipText->SetText(tmpFskipTxt);
}

/****************************************************************************
 * HomeMenu
 ***************************************************************************/
void HomeMenu ()
{
	ResetVideo_Menu();
	
	pointer[0] = new GuiImageData(player1_point_png);
	pointer[1] = new GuiImageData(player2_point_png);
	pointer[2] = new GuiImageData(player3_point_png);
	pointer[3] = new GuiImageData(player4_point_png);

	mainWindow = new GuiWindow(screenwidth, screenheight);

	GuiImage screenImg(screenTex, screenwidth, screenheight);
	screenImg.SetAlpha(192);
	screenImg.ColorStripe(30);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiSound btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	GuiSound enterSound(enter_ogg, enter_ogg_size, SOUND_OGG);
	GuiSound exitSound(exit_ogg, exit_ogg_size, SOUND_OGG);

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
	bgBottomImg.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	
	GuiImageData logo(logo_png);
	GuiImage logoImg(&logo);
	GuiImageData logoOver(logo_over_png);
	GuiImage logoImgOver(&logoOver);
	GuiText logoTxt(APPVERSION, 18, (GXColor){255, 255, 255, 255});
	logoTxt.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	logoTxt.SetPosition(30, 31);
	logoBtn = new GuiButton(logoImg.GetWidth(), logoImg.GetHeight());
	logoBtn->SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	logoBtn->SetPosition(-85, -40);
	logoBtn->SetImage(&logoImg);
	logoBtn->SetImageOver(&logoImgOver);
	logoBtn->SetLabel(&logoTxt);
	logoBtn->SetSoundOver(&btnSoundOver);
	logoBtn->SetSoundClick(&btnSoundClick);
	logoBtn->SetTrigger(&trigA);

	GuiText cycleText(NULL, 20, (GXColor){255, 255, 255, 255});
	cycleText.SetPosition(-215, -180);
	cycleText.SetPseudoMonospace(70);
	updateCyclesText(&cycleText);

	GuiText fskipText(NULL, 20, (GXColor){255, 255, 255, 255});
	fskipText.SetPosition(-45, -180);
	fskipText.SetPseudoMonospace(70);
	updateFskipText(&fskipText);

	GuiText cycleDecBtnTxt("-", 24, (GXColor){0, 0, 0, 255});
	GuiImageData cycleDec(keyboard_key_png);
	GuiImage cycleDecImg(&cycleDec);
	GuiImageData cycleDecOver(keyboard_key_over_png);
	GuiImage cycleDecOverImg(&cycleDecOver);
	GuiButton cycleDecBtn(cycleDec.GetWidth(), cycleDec.GetHeight());
	cycleDecBtn.SetImage(&cycleDecImg);
	cycleDecBtn.SetImageOver(&cycleDecOverImg);
	cycleDecBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	cycleDecBtn.SetPosition(-270, -180);
	cycleDecBtn.SetLabel(&cycleDecBtnTxt);
	cycleDecBtn.SetTrigger(&trigA);
	cycleDecBtn.SetEffectGrow();

	GuiText cycleIncBtnTxt("+", 24, (GXColor){0, 0, 0, 255});
	GuiImageData cycleInc(keyboard_key_png);
	GuiImage cycleIncImg(&cycleInc);
	GuiImageData cycleIncOver(keyboard_key_over_png);
	GuiImage cycleIncOverImg(&cycleIncOver);
	GuiButton cycleIncBtn(cycleInc.GetWidth(), cycleInc.GetHeight());
	cycleIncBtn.SetImage(&cycleIncImg);
	cycleIncBtn.SetImageOver(&cycleIncOverImg);
	cycleIncBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	cycleIncBtn.SetPosition(-160, -180);
	cycleIncBtn.SetLabel(&cycleIncBtnTxt);
	cycleIncBtn.SetTrigger(&trigA);
	cycleIncBtn.SetEffectGrow();

	GuiText fskipDecBtnTxt("-", 24, (GXColor){0, 0, 0, 255});
	GuiImageData fskipDec(keyboard_key_png);
	GuiImage fskipDecImg(&fskipDec);
	GuiImageData fskipDecOver(keyboard_key_over_png);
	GuiImage fskipDecOverImg(&fskipDecOver);
	GuiButton fskipDecBtn(fskipDec.GetWidth(), fskipDec.GetHeight());
	fskipDecBtn.SetImage(&fskipDecImg);
	fskipDecBtn.SetImageOver(&fskipDecOverImg);
	fskipDecBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	fskipDecBtn.SetPosition(-80, -180);
	fskipDecBtn.SetLabel(&fskipDecBtnTxt);
	fskipDecBtn.SetTrigger(&trigA);
	fskipDecBtn.SetEffectGrow();

	GuiText fskipIncBtnTxt("+", 24, (GXColor){0, 0, 0, 255});
	GuiImageData fskipInc(keyboard_key_png);
	GuiImage fskipIncImg(&fskipInc);
	GuiImageData fskipIncOver(keyboard_key_over_png);
	GuiImage fskipIncOverImg(&fskipIncOver);
	GuiButton fskipIncBtn(fskipInc.GetWidth(), fskipInc.GetHeight());
	fskipIncBtn.SetImage(&fskipIncImg);
	fskipIncBtn.SetImageOver(&fskipIncOverImg);
	fskipIncBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	fskipIncBtn.SetPosition(0, -180);
	fskipIncBtn.SetLabel(&fskipIncBtnTxt);
	fskipIncBtn.SetTrigger(&trigA);
	fskipIncBtn.SetEffectGrow();

	GuiText exitBtnTxt("Exit", 24, (GXColor){0, 0, 0, 255});
	GuiImage exitBtnImg(&btnLargeOutline);
	GuiImage exitBtnImgOver(&btnLargeOutlineOver);
	GuiButton exitBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	exitBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	exitBtn.SetPosition(-125, 120);
	exitBtn.SetLabel(&exitBtnTxt);
	exitBtn.SetImage(&exitBtnImg);
	exitBtn.SetImageOver(&exitBtnImgOver);
	exitBtn.SetSoundOver(&btnSoundOver);
	exitBtn.SetSoundClick(&btnSoundClick);
	exitBtn.SetTrigger(&trigA);
	exitBtn.SetEffectGrow();

	GuiText logBtnTxt("Logging", 24, (GXColor){0, 0, 0, 255});
	GuiImage logBtnImg(&btnLargeOutline);
	GuiImage logBtnImgOver(&btnLargeOutlineOver);
	GuiButton logBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	logBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	logBtn.SetPosition(-125, 240);
	logBtn.SetLabel(&logBtnTxt);
	logBtn.SetImage(&logBtnImg);
	logBtn.SetImageOver(&logBtnImgOver);
	logBtn.SetSoundOver(&btnSoundOver);
	logBtn.SetSoundClick(&btnSoundClick);
	logBtn.SetTrigger(&trigA);
	logBtn.SetEffectGrow();

	GuiText keyboardBtnTxt("Keyboard", 24, (GXColor){0, 0, 0, 255});
	GuiImage keyboardBtnImg(&btnLargeOutline);
	GuiImage keyboardBtnImgOver(&btnLargeOutlineOver);
	GuiButton keyboardBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	keyboardBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	keyboardBtn.SetPosition(125, 120);
	keyboardBtn.SetLabel(&keyboardBtnTxt);
	keyboardBtn.SetImage(&keyboardBtnImg);
	keyboardBtn.SetImageOver(&keyboardBtnImgOver);
	keyboardBtn.SetSoundOver(&btnSoundOver);
	keyboardBtn.SetSoundClick(&btnSoundClick);
	keyboardBtn.SetTrigger(&trigA);
	keyboardBtn.SetEffectGrow();

	GuiText closeBtnTxt("Close", 22, (GXColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	closeBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	closeBtn.SetPosition(-50, 35);
	closeBtn.SetLabel(&closeBtnTxt);
	closeBtn.SetImage(&closeBtnImg);
	closeBtn.SetImageOver(&closeBtnImgOver);
	closeBtn.SetSoundOver(&btnSoundOver);
	closeBtn.SetSoundClick(&btnSoundClick);
	closeBtn.SetTrigger(&trigA);
	closeBtn.SetTrigger(&trigHome);
	closeBtn.SetEffectGrow();

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

		batteryTxt[i] = new GuiText(txt, 22, (GXColor){255, 255, 255, 255});
		batteryTxt[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryImg[i] = new GuiImage(&battery);
		batteryImg[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryImg[i]->SetPosition(30, 0);
		batteryBarImg[i] = new GuiImage(&batteryBar);
		batteryBarImg[i]->SetTile(0);
		batteryBarImg[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryBarImg[i]->SetPosition(34, 0);

		batteryBtn[i] = new GuiButton(70, 20);
		batteryBtn[i]->SetLabel(batteryTxt[i]);
		batteryBtn[i]->SetImage(batteryImg[i]);
		batteryBtn[i]->SetIcon(batteryBarImg[i]);
		batteryBtn[i]->SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		batteryBtn[i]->SetRumble(false);
		batteryBtn[i]->SetSelectable(false);
		batteryBtn[i]->SetAlpha(150);
	}

	batteryBtn[0]->SetPosition(45, -65);
	batteryBtn[1]->SetPosition(135, -65);
	batteryBtn[2]->SetPosition(45, -40);
	batteryBtn[3]->SetPosition(135, -40);
	
	GuiWindow w(screenwidth, screenheight);

	w.Append(&bgTopImg);
	w.Append(&bgBottomImg);
	w.Append(batteryBtn[0]);
	w.Append(batteryBtn[1]);
	w.Append(batteryBtn[2]);
	w.Append(batteryBtn[3]);
	w.Append(logoBtn);
	w.Append(&closeBtn);
	w.Append(&exitBtn);
	w.Append(&logBtn);
	w.Append(&cycleText);
	w.Append(&fskipText);
	w.Append(&cycleDecBtn);
	w.Append(&cycleIncBtn);
	w.Append(&fskipDecBtn);
	w.Append(&fskipIncBtn);
	w.Append(&keyboardBtn);
	
	mainWindow->Append(&screenImg);
	mainWindow->Append(&w);

	enterSound.Play();
	bgTopImg.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 35);
	closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 35);
	bgBottomImg.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
	logoBtn->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);

	batteryBtn[0]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
	batteryBtn[1]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
	batteryBtn[2]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
	batteryBtn[3]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);

	w.SetEffect(EFFECT_FADE, 15);

	ResumeGui();

	while(1)
	{
		usleep(THREAD_SLEEP);

		for(i=0; i < 4; i++)
		{
			if(WPAD_Probe(i, NULL) == WPAD_ERR_NONE)
			{
				newStatus = true;
				newLevel = (userInput[i].wpad->battery_level / 100.0) * 4;
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
					batteryBtn[i]->SetAlpha(255);
					batteryBarImg[i]->SetTile(newLevel);

					if(newLevel == 0)
						batteryImg[i]->SetImage(&batteryRed);
					else
						batteryImg[i]->SetImage(&battery);
				}
				else // controller not connected
				{
					batteryBtn[i]->SetAlpha(150);
					batteryBarImg[i]->SetTile(0);
					batteryImg[i]->SetImage(&battery);
				}
				status[i] = newStatus;
				level[i] = newLevel;
			}
		}

		if(closeBtn.GetState() == STATE_CLICKED)
		{
			exitSound.Play();
			bgTopImg.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			bgBottomImg.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			logoBtn->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);

			batteryBtn[0]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			batteryBtn[1]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			batteryBtn[2]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			batteryBtn[3]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);

			w.SetEffect(EFFECT_FADE, -15);
			usleep(450000); // wait for effects to finish

			break;
		}
		else if(exitBtn.GetState() == STATE_CLICKED)
		{
			ExitRequested = 1;
		}
		else if(logBtn.GetState() == STATE_CLICKED)
		{
			logBtn.ResetState();
			OnScreenConsole();
		}
		else if(keyboardBtn.GetState() == STATE_CLICKED)
		{
			keyboardBtn.ResetState();
			OnScreenKeyboard(dosboxCommand, MAXPATHLEN);
			
			if(dosboxCommand[0] != 0)
				break;
		}
		else if (cycleDecBtn.GetState() == STATE_CLICKED)
		{
			cycleDecBtn.ResetState();
			MENU_CycleIncreaseOrDecrease(false);
			updateCyclesText(&cycleText);
		}
		else if (cycleIncBtn.GetState() == STATE_CLICKED)
		{
			cycleIncBtn.ResetState();
			MENU_CycleIncreaseOrDecrease(true);
			updateCyclesText(&cycleText);
		}
		else if (fskipDecBtn.GetState() == STATE_CLICKED)
		{
			fskipDecBtn.ResetState();
			MENU_IncreaseOrDecreaseFrameSkip(false);
			updateFskipText(&fskipText);
		}
		else if (fskipIncBtn.GetState() == STATE_CLICKED)
		{
			fskipIncBtn.ResetState();
			MENU_IncreaseOrDecreaseFrameSkip(true);
			updateFskipText(&fskipText);
		}
		else if (logoBtn->GetState() == STATE_CLICKED)
		{
			logoBtn->ResetState();
			WindowCredits();
		}
	}

	ShutoffRumble();

	// wait for keys to be depressed
	while(MenuRequested())
		usleep(THREAD_SLEEP);

	exitSound.Stop();

	HaltGui();

	for(i=0; i < 4; i++)
	{
		delete batteryTxt[i];
		delete batteryImg[i];
		delete batteryBarImg[i];
		delete batteryBtn[i];
	}

	delete mainWindow;
	delete logoBtn;
	delete pointer[0];
	delete pointer[1];
	delete pointer[2];
	delete pointer[3];

	mainWindow = NULL;

	if(screenTex)
	{
		free(screenTex);
		screenTex = NULL;
	}
}
