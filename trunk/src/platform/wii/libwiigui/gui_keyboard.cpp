/****************************************************************************
 * libwiigui
 *
 * Tantric 2009-2010
 *
 * gui_keyboard.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"
#include <string>

using namespace std;

static char tmptxt[MAX_KEYBOARD_DISPLAY];

static void str_replace(string &str, char *search, const char *replace)
{
	int searchLen = strlen(search);
	int replaceLen = strlen(replace);

	string::size_type pos=0;

	while ((pos=str.find(search, pos)) != string::npos)
	{
		str.erase(pos, searchLen);
		str.insert(pos, replace);
		pos += replaceLen;
	}
}

static char * GetDisplayText(char * t, int max)
{
	if(!t || t[0] == 0)
		return t;

	string tempStr(t);
	char txt[2] = { 0, 0 };
	txt[0] = 13; str_replace(tempStr, txt, "<ENTER>");
	txt[0] = 27; str_replace(tempStr, txt, "<ESC>");
	txt[0] = 14; str_replace(tempStr, txt, "<F1>");
	txt[0] = 15; str_replace(tempStr, txt, "<F2>");
	txt[0] = 16; str_replace(tempStr, txt, "<F3>");
	txt[0] = 17; str_replace(tempStr, txt, "<F4>");
	txt[0] = 18; str_replace(tempStr, txt, "<F5>");
	txt[0] = 19; str_replace(tempStr, txt, "<F6>");
	txt[0] = 20; str_replace(tempStr, txt, "<F7>");
	txt[0] = 21; str_replace(tempStr, txt, "<F8>");
	txt[0] = 22; str_replace(tempStr, txt, "<F9>");
	txt[0] = 23; str_replace(tempStr, txt, "<F10>");
	txt[0] = 24; str_replace(tempStr, txt, "<F11>");
	txt[0] = 25; str_replace(tempStr, txt, "<F12>");

	int s = tempStr.length()-MAX_KEYBOARD_DISPLAY;
	if(s < 0) s = 0;
	snprintf(tmptxt, MAX_KEYBOARD_DISPLAY, "%s", &(tempStr.c_str()[s]));
	return &tmptxt[0];
}

/**
 * Constructor for the GuiKeyboard class.
 */

GuiKeyboard::GuiKeyboard(char * t, u32 max)
{
	width = 610;
	height = 440;
	shift = 0;
	caps = 0;
	selectable = true;
	focus = 0; // allow focus
	alignmentHor = ALIGN_CENTRE;
	alignmentVert = ALIGN_MIDDLE;
	strncpy(kbtextstr, t, max);
	kbtextstr[max] = 0;
	kbtextmaxlen = max;

	Key thekeys[5][12] = {
	{
		{14,0}, // F1
		{15,0}, // F2
		{16,0}, // F3
		{17,0}, // F4
		{18,0}, // F5
		{19,0}, // F6
		{20,0}, // F7
		{21,0}, // F8
		{22,0}, // F9
		{23,0}, // F10
		{24,0}, // F11
		{25,0}, // F12
	},
	{
		{'`','~'},
		{'1','!'},
		{'2','@'},
		{'3','#'},
		{'4','$'},
		{'5','%'},
		{'6','^'},
		{'7','&'},
		{'8','*'},
		{'9','('},
		{'0',')'},
		{'-','_'}
	},
	{
		{'q','Q'},
		{'w','W'},
		{'e','E'},
		{'r','R'},
		{'t','T'},
		{'y','Y'},
		{'u','U'},
		{'i','I'},
		{'o','O'},
		{'p','P'},
		{'[','{'},
		{']','}'}
	},
	{
		{'a','A'},
		{'s','S'},
		{'d','D'},
		{'f','F'},
		{'g','G'},
		{'h','H'},
		{'j','J'},
		{'k','K'},
		{'l','L'},
		{';',':'},
		{'\'','"'},
		{'\\','|'}
	},

	{
		{'z','Z'},
		{'x','X'},
		{'c','C'},
		{'v','V'},
		{'b','B'},
		{'n','N'},
		{'m','M'},
		{',','<'},
		{'.','>'},
		{'/','?'},
		{'\0','\0'},
		{'\0','\0'}
	}
	};
	memcpy(keys, thekeys, sizeof(thekeys));

	keyTextbox = new GuiImageData(keyboard_textbox_png);
	keyTextboxImg = new GuiImage(keyTextbox);
	keyTextboxImg->SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	keyTextboxImg->SetPosition(0, 20);
	this->Append(keyTextboxImg);

	kbText = new GuiText(GetDisplayText(kbtextstr, max), 22, (GXColor){0, 0, 0, 0xff});
	kbText->SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	kbText->SetPosition(0, 33);
	this->Append(kbText);

	key = new GuiImageData(keyboard_key_png);
	keyOver = new GuiImageData(keyboard_key_over_png);
	keyMedium = new GuiImageData(keyboard_mediumkey_png);
	keyMediumOver = new GuiImageData(keyboard_mediumkey_over_png);
	keyLarge = new GuiImageData(keyboard_largekey_png);
	keyLargeOver = new GuiImageData(keyboard_largekey_over_png);

	keySoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	keySoundClick = new GuiSound(button_click_pcm, button_click_pcm_size, SOUND_PCM);
	trigA = new GuiTrigger;
	trigA->SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	trigB = new GuiTrigger;
	trigB->SetButtonOnlyTrigger(-1, WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B, PAD_BUTTON_B);
	
	int yoff = 80;
	
	keyEscImg = new GuiImage(keyMedium);
	keyEscOverImg = new GuiImage(keyMediumOver);
	keyEscText = new GuiText("Esc", 22, (GXColor){0, 0, 0, 0xff});
	keyEsc = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyEsc->SetImage(keyEscImg);
	keyEsc->SetImageOver(keyEscOverImg);
	keyEsc->SetLabel(keyEscText);
	keyEsc->SetSoundOver(keySoundOver);
	keyEsc->SetSoundClick(keySoundClick);
	keyEsc->SetTrigger(trigA);
	keyEsc->SetPosition(0, yoff);
	keyEsc->SetEffectGrow();
	this->Append(keyEsc);
	
	keyEnterImg = new GuiImage(keyMedium);
	keyEnterOverImg = new GuiImage(keyMediumOver);
	keyEnterText = new GuiText("Enter", 22, (GXColor){0, 0, 0, 0xff});
	keyEnter = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyEnter->SetImage(keyEnterImg);
	keyEnter->SetImageOver(keyEnterOverImg);
	keyEnter->SetLabel(keyEnterText);
	keyEnter->SetSoundOver(keySoundOver);
	keyEnter->SetSoundClick(keySoundClick);
	keyEnter->SetTrigger(trigA);
	keyEnter->SetPosition(12*42+18, 4*42+yoff);
	keyEnter->SetEffectGrow();
	this->Append(keyEnter);

	keyBackImg = new GuiImage(keyMedium);
	keyBackOverImg = new GuiImage(keyMediumOver);
	keyBackText = new GuiText("Back", 22, (GXColor){0, 0, 0, 0xff});
	keyBack = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyBack->SetImage(keyBackImg);
	keyBack->SetImageOver(keyBackOverImg);
	keyBack->SetLabel(keyBackText);
	keyBack->SetSoundOver(keySoundOver);
	keyBack->SetSoundClick(keySoundClick);
	keyBack->SetTrigger(trigA);
	keyBack->SetTrigger(trigB);
	keyBack->SetPosition(11*42+40, 1*42+yoff);
	keyBack->SetEffectGrow();
	this->Append(keyBack);

	keyCapsImg = new GuiImage(keyMedium);
	keyCapsOverImg = new GuiImage(keyMediumOver);
	keyCapsText = new GuiText("Caps", 22, (GXColor){0, 0, 0, 0xff});
	keyCaps = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyCaps->SetImage(keyCapsImg);
	keyCaps->SetImageOver(keyCapsOverImg);
	keyCaps->SetLabel(keyCapsText);
	keyCaps->SetSoundOver(keySoundOver);
	keyCaps->SetSoundClick(keySoundClick);
	keyCaps->SetTrigger(trigA);
	keyCaps->SetPosition(0, 3*42+yoff);
	keyCaps->SetEffectGrow();
	this->Append(keyCaps);

	keyShiftImg = new GuiImage(keyMedium);
	keyShiftOverImg = new GuiImage(keyMediumOver);
	keyShiftText = new GuiText("Shift", 22, (GXColor){0, 0, 0, 0xff});
	keyShift = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyShift->SetImage(keyShiftImg);
	keyShift->SetImageOver(keyShiftOverImg);
	keyShift->SetLabel(keyShiftText);
	keyShift->SetSoundOver(keySoundOver);
	keyShift->SetSoundClick(keySoundClick);
	keyShift->SetTrigger(trigA);
	keyShift->SetPosition(21, 4*42+yoff);
	keyShift->SetEffectGrow();
	this->Append(keyShift);

	keySpaceImg = new GuiImage(keyLarge);
	keySpaceOverImg = new GuiImage(keyLargeOver);
	keySpace = new GuiButton(keyLarge->GetWidth(), keyLarge->GetHeight());
	keySpace->SetImage(keySpaceImg);
	keySpace->SetImageOver(keySpaceOverImg);
	keySpace->SetSoundOver(keySoundOver);
	keySpace->SetSoundClick(keySoundClick);
	keySpace->SetTrigger(trigA);
	keySpace->SetPosition(0, 5*42+yoff);
	keySpace->SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	keySpace->SetEffectGrow();
	this->Append(keySpace);

	char txt[2] = { 0, 0 };

	for(int i=0; i<5; i++)
	{
		for(int j=0; j<12; j++)
		{
			if(keys[i][j].ch != '\0')
			{
				txt[0] = keys[i][j].ch;
				keyImg[i][j] = new GuiImage(key);
				keyImgOver[i][j] = new GuiImage(keyOver);
				switch(keys[i][j].ch)
				{
					case 14: keyTxt[i][j] = new GuiText("F1", 22, (GXColor){0, 0, 0, 0xff}); break;
					case 15: keyTxt[i][j] = new GuiText("F2", 22, (GXColor){0, 0, 0, 0xff}); break;
					case 16: keyTxt[i][j] = new GuiText("F3", 22, (GXColor){0, 0, 0, 0xff}); break;
					case 17: keyTxt[i][j] = new GuiText("F4", 22, (GXColor){0, 0, 0, 0xff}); break;
					case 18: keyTxt[i][j] = new GuiText("F5", 22, (GXColor){0, 0, 0, 0xff}); break;
					case 19: keyTxt[i][j] = new GuiText("F6", 22, (GXColor){0, 0, 0, 0xff}); break;
					case 20: keyTxt[i][j] = new GuiText("F7", 22, (GXColor){0, 0, 0, 0xff}); break;
					case 21: keyTxt[i][j] = new GuiText("F8", 22, (GXColor){0, 0, 0, 0xff}); break;
					case 22: keyTxt[i][j] = new GuiText("F9", 22, (GXColor){0, 0, 0, 0xff}); break;
					case 23: keyTxt[i][j] = new GuiText("F10", 22, (GXColor){0, 0, 0, 0xff}); break;
					case 24: keyTxt[i][j] = new GuiText("F11", 22, (GXColor){0, 0, 0, 0xff}); break;
					case 25: keyTxt[i][j] = new GuiText("F12", 22, (GXColor){0, 0, 0, 0xff}); break;

					default: 
						keyTxt[i][j] = new GuiText(txt, 22, (GXColor){0, 0, 0, 0xff});
						break;
				}
				keyTxt[i][j]->SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
				keyTxt[i][j]->SetPosition(0, -10);
				keyBtn[i][j] = new GuiButton(key->GetWidth(), key->GetHeight());
				keyBtn[i][j]->SetImage(keyImg[i][j]);
				keyBtn[i][j]->SetImageOver(keyImgOver[i][j]);
				keyBtn[i][j]->SetSoundOver(keySoundOver);
				keyBtn[i][j]->SetSoundClick(keySoundClick);
				keyBtn[i][j]->SetTrigger(trigA);
				keyBtn[i][j]->SetLabel(keyTxt[i][j]);

				int stagger;
				switch(i)
				{
					case 0: stagger = 42; break;
					case 1: stagger = -42; break;
					default: stagger = 21*(i-1); break;
				}

				keyBtn[i][j]->SetPosition(j*42+stagger+40, i*42+yoff);
				keyBtn[i][j]->SetEffectGrow();
				this->Append(keyBtn[i][j]);
			}
		}
	}
}

/**
 * Destructor for the GuiKeyboard class.
 */
GuiKeyboard::~GuiKeyboard()
{
	delete kbText;
	delete keyTextbox;
	delete keyTextboxImg;
	delete keyEscText;
	delete keyEscImg;
	delete keyEscOverImg;
	delete keyEsc;
	delete keyEnterText;
	delete keyEnterImg;
	delete keyEnterOverImg;
	delete keyEnter;
	delete keyCapsText;
	delete keyCapsImg;
	delete keyCapsOverImg;
	delete keyCaps;
	delete keyShiftText;
	delete keyShiftImg;
	delete keyShiftOverImg;
	delete keyShift;
	delete keyBackText;
	delete keyBackImg;
	delete keyBackOverImg;
	delete keyBack;
	delete keySpaceImg;
	delete keySpaceOverImg;
	delete keySpace;
	delete key;
	delete keyOver;
	delete keyMedium;
	delete keyMediumOver;
	delete keyLarge;
	delete keyLargeOver;
	delete keySoundOver;
	delete keySoundClick;
	delete trigA;

	for(int i=0; i<5; i++)
	{
		for(int j=0; j<12; j++)
		{
			if(keys[i][j].ch != '\0')
			{
				delete keyImg[i][j];
				delete keyImgOver[i][j];
				delete keyTxt[i][j];
				delete keyBtn[i][j];
			}
		}
	}
}

void GuiKeyboard::Update(GuiTrigger * t)
{
	if(_elements.size() == 0 || (state == STATE_DISABLED && parentElement))
		return;

	for (u8 i = 0; i < _elements.size(); i++)
	{
		try	{ _elements.at(i)->Update(t); }
		catch (const std::exception& e) { }
	}

	bool update = false;

	if(keyEsc->GetState() == STATE_CLICKED)
	{
		if(strlen(kbtextstr) < kbtextmaxlen)
		{
			kbtextstr[strlen(kbtextstr)] = 27; // Esc key code
			kbText->SetText(GetDisplayText(kbtextstr, kbtextmaxlen));
		}
		keyEsc->SetState(STATE_SELECTED, t->chan);
	}
	else if(keyEnter->GetState() == STATE_CLICKED)
	{
		if(strlen(kbtextstr) < kbtextmaxlen)
		{
			kbtextstr[strlen(kbtextstr)] = 13; // Enter key code
			kbText->SetText(GetDisplayText(kbtextstr, kbtextmaxlen));
		}
		keyEnter->SetState(STATE_SELECTED, t->chan);
	}
	else if(keySpace->GetState() == STATE_CLICKED)
	{
		if(strlen(kbtextstr) < kbtextmaxlen)
		{
			kbtextstr[strlen(kbtextstr)] = ' ';
			kbText->SetText(GetDisplayText(kbtextstr, kbtextmaxlen));
		}
		keySpace->SetState(STATE_SELECTED, t->chan);
	}
	else if(keyBack->GetState() == STATE_CLICKED)
	{
		if(strlen(kbtextstr) > 0)
		{
			kbtextstr[strlen(kbtextstr)-1] = 0;
			kbText->SetText(GetDisplayText(kbtextstr, kbtextmaxlen));
		}
		keyBack->SetState(STATE_SELECTED, t->chan);
	}
	else if(keyShift->GetState() == STATE_CLICKED)
	{
		shift ^= 1;
		keyShift->SetState(STATE_SELECTED, t->chan);
		update = true;
	}
	else if(keyCaps->GetState() == STATE_CLICKED)
	{
		caps ^= 1;
		keyCaps->SetState(STATE_SELECTED, t->chan);
		update = true;
	}

	char txt[2] = { 0, 0 };

	startloop:

	for(int i=0; i<5; i++)
	{
		for(int j=0; j<12; j++)
		{
			if(keys[i][j].ch != '\0')
			{
				if(update && keys[i][j].ch > 31)
				{
					if(shift || caps)
						txt[0] = keys[i][j].chShift;
					else
						txt[0] = keys[i][j].ch;

					keyTxt[i][j]->SetText(txt);
				}

				if(keyBtn[i][j]->GetState() == STATE_CLICKED)
				{
					if(strlen(kbtextstr) < kbtextmaxlen)
					{
						if(shift || caps)
						{
							kbtextstr[strlen(kbtextstr)] = keys[i][j].chShift;
						}
						else
						{
							kbtextstr[strlen(kbtextstr)] = keys[i][j].ch;
						}
					}
					kbText->SetText(GetDisplayText(kbtextstr, kbtextmaxlen));
					keyBtn[i][j]->SetState(STATE_SELECTED, t->chan);

					if(shift)
					{
						shift ^= 1;
						update = true;
						goto startloop;
					}
				}
			}
		}
	}

	this->ToggleFocus(t);

	if(focus) // only send actions to this window if it's in focus
	{
		// pad/joystick navigation
		if(t->Right())
			this->MoveSelectionHor(1);
		else if(t->Left())
			this->MoveSelectionHor(-1);
		else if(t->Down())
			this->MoveSelectionVert(1);
		else if(t->Up())
			this->MoveSelectionVert(-1);
	}
}
