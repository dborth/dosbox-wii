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
#ifdef EXTENDED_KEYBOARD
#include <string>
#endif

#define KB_FONTSIZE 22

static char tmptxt[MAX_KEYBOARD_DISPLAY];

#ifdef EXTENDED_KEYBOARD
static void str_replace(std::string &str, const char *search, const char *replace)
{
	int searchLen = strlen(search);
	int replaceLen = strlen(replace);

	std::string::size_type pos=0;

	while ((pos=str.find(search, pos)) != std::string::npos)
	{
		str.erase(pos, searchLen);
		str.insert(pos, replace);
		pos += replaceLen;
	}
}

static const char * GetDisplayText(const char * t)
{
	if(!t || t[0] == 0)
		return t;

	std::string tempStr(t);
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

#else

static const char * GetDisplayText(const char * t)
{
	if(!t)
		return NULL;

	snprintf(tmptxt, MAX_KEYBOARD_DISPLAY, "%s", t);
	return &tmptxt[0];
}

#endif


/**
 * Constructor for the GuiKeyboard class.
 */

GuiKeyboard::GuiKeyboard(char * t, u32 max)
{
#ifdef EXTENDED_KEYBOARD
	width = 610;
	height = 440;
#else
	width = 540;
	height = 400;
#endif
	shift = 0;
	caps = 0;
	selectable = true;
	focus = 0; // allow focus
	alignmentHor = ALIGN_CENTRE;
	alignmentVert = ALIGN_MIDDLE;
	snprintf(kbtextstr, 255, "%s", t);
	kbtextmaxlen = max;

#ifdef EXTENDED_KEYBOARD
	Key thekeys[KB_ROWS][KB_COLUMNS] = {
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
		{25,0}  // F12
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
#else
	Key thekeys[KB_ROWS][KB_COLUMNS] = {
	{
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
		{'\0','\0'}
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
		{'-','_'}
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
		{'\'','"'}
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
		{'\0','\0'}
	}
	};
#endif
	memcpy(keys, thekeys, sizeof(thekeys));

#ifdef EXTENDED_KEYBOARD
	int yoff_1 = 20;
#else
	int yoff_1 = 0;
#endif
	keyTextbox = new GuiImageData(keyboard_textbox_png);
	keyTextboxImg = new GuiImage(keyTextbox);
	keyTextboxImg->SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	keyTextboxImg->SetPosition(0, yoff_1);
	this->Append(keyTextboxImg);

	kbText = new GuiText(GetDisplayText(kbtextstr), KB_FONTSIZE, (GXColor){0, 0, 0, 0xff});
	kbText->SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	kbText->SetPosition(0, yoff_1 + 13);
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
	trig2 = new GuiTrigger;
	trig2->SetSimpleTrigger(-1, WPAD_BUTTON_2, 0);

	int yoff = 80;

#ifdef EXTENDED_KEYBOARD
	keyEscImg = new GuiImage(keyMedium);
	keyEscOverImg = new GuiImage(keyMediumOver);
	keyEscText = new GuiText("Esc", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff});
	keyEsc = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyEsc->SetImage(keyEscImg);
	keyEsc->SetImageOver(keyEscOverImg);
	keyEsc->SetLabel(keyEscText);
	keyEsc->SetSoundOver(keySoundOver);
	keyEsc->SetSoundClick(keySoundClick);
	keyEsc->SetTrigger(trigA);
	keyEsc->SetTrigger(trig2);
	keyEsc->SetPosition(0, yoff);
	keyEsc->SetEffectGrow();
	this->Append(keyEsc);
	
	keyEnterImg = new GuiImage(keyMedium);
	keyEnterOverImg = new GuiImage(keyMediumOver);
	keyEnterText = new GuiText("Enter", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff});
	keyEnter = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyEnter->SetImage(keyEnterImg);
	keyEnter->SetImageOver(keyEnterOverImg);
	keyEnter->SetLabel(keyEnterText);
	keyEnter->SetSoundOver(keySoundOver);
	keyEnter->SetSoundClick(keySoundClick);
	keyEnter->SetTrigger(trigA);
	keyEnter->SetTrigger(trig2);
	keyEnter->SetPosition(12*42+18, 4*42+yoff);
	keyEnter->SetEffectGrow();
	this->Append(keyEnter);
#endif

	keyBackImg = new GuiImage(keyMedium);
	keyBackOverImg = new GuiImage(keyMediumOver);
	keyBackText = new GuiText("Back", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff});
	keyBack = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyBack->SetImage(keyBackImg);
	keyBack->SetImageOver(keyBackOverImg);
	keyBack->SetLabel(keyBackText);
	keyBack->SetSoundOver(keySoundOver);
	keyBack->SetSoundClick(keySoundClick);
	keyBack->SetTrigger(trigA);
	keyBack->SetTrigger(trig2);
	keyBack->SetPosition((KB_COLUMNS-1)*42+40, (KB_ROWS-4)*42+yoff);
	keyBack->SetEffectGrow();
	this->Append(keyBack);

	keyCapsImg = new GuiImage(keyMedium);
	keyCapsOverImg = new GuiImage(keyMediumOver);
	keyCapsText = new GuiText("Caps", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff});
	keyCaps = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyCaps->SetImage(keyCapsImg);
	keyCaps->SetImageOver(keyCapsOverImg);
	keyCaps->SetLabel(keyCapsText);
	keyCaps->SetSoundOver(keySoundOver);
	keyCaps->SetSoundClick(keySoundClick);
	keyCaps->SetTrigger(trigA);
	keyCaps->SetTrigger(trig2);
	keyCaps->SetPosition(0, (KB_ROWS-2)*42+yoff);
	keyCaps->SetEffectGrow();
	this->Append(keyCaps);

	keyShiftImg = new GuiImage(keyMedium);
	keyShiftOverImg = new GuiImage(keyMediumOver);
	keyShiftText = new GuiText("Shift", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff});
	keyShift = new GuiButton(keyMedium->GetWidth(), keyMedium->GetHeight());
	keyShift->SetImage(keyShiftImg);
	keyShift->SetImageOver(keyShiftOverImg);
	keyShift->SetLabel(keyShiftText);
	keyShift->SetSoundOver(keySoundOver);
	keyShift->SetSoundClick(keySoundClick);
	keyShift->SetTrigger(trigA);
	keyShift->SetTrigger(trig2);
	keyShift->SetPosition(21, (KB_ROWS-1)*42+yoff);
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
	keySpace->SetTrigger(trig2);
	keySpace->SetPosition(0, KB_ROWS*42+yoff);
	keySpace->SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	keySpace->SetEffectGrow();
	this->Append(keySpace);

	char txt[2] = { 0, 0 };

	for(int i=0; i<KB_ROWS; i++)
	{
		for(int j=0; j<KB_COLUMNS; j++)
		{
			if(keys[i][j].ch != '\0')
			{
				txt[0] = keys[i][j].ch;
				keyImg[i][j] = new GuiImage(key);
				keyImgOver[i][j] = new GuiImage(keyOver);
#ifdef EXTENDED_KEYBOARD
				switch(keys[i][j].ch)
				{
					case 14: keyTxt[i][j] = new GuiText("F1", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff}); break;
					case 15: keyTxt[i][j] = new GuiText("F2", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff}); break;
					case 16: keyTxt[i][j] = new GuiText("F3", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff}); break;
					case 17: keyTxt[i][j] = new GuiText("F4", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff}); break;
					case 18: keyTxt[i][j] = new GuiText("F5", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff}); break;
					case 19: keyTxt[i][j] = new GuiText("F6", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff}); break;
					case 20: keyTxt[i][j] = new GuiText("F7", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff}); break;
					case 21: keyTxt[i][j] = new GuiText("F8", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff}); break;
					case 22: keyTxt[i][j] = new GuiText("F9", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff}); break;
					case 23: keyTxt[i][j] = new GuiText("F10", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff}); break;
					case 24: keyTxt[i][j] = new GuiText("F11", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff}); break;
					case 25: keyTxt[i][j] = new GuiText("F12", KB_FONTSIZE, (GXColor){0, 0, 0, 0xff}); break;

					default: 
						keyTxt[i][j] = new GuiText(txt, KB_FONTSIZE, (GXColor){0, 0, 0, 0xff});
						break;
				}
#else
				keyTxt[i][j] = new GuiText(txt, KB_FONTSIZE, (GXColor){0, 0, 0, 0xff});
#endif
				keyTxt[i][j]->SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
				keyTxt[i][j]->SetPosition(0, -10);
				keyBtn[i][j] = new GuiButton(key->GetWidth(), key->GetHeight());
				keyBtn[i][j]->SetImage(keyImg[i][j]);
				keyBtn[i][j]->SetImageOver(keyImgOver[i][j]);
				keyBtn[i][j]->SetSoundOver(keySoundOver);
				keyBtn[i][j]->SetSoundClick(keySoundClick);
				keyBtn[i][j]->SetTrigger(trigA);
				keyBtn[i][j]->SetTrigger(trig2);
				keyBtn[i][j]->SetLabel(keyTxt[i][j]);

#ifdef EXTENDED_KEYBOARD
				int stagger;
				switch(i)
				{
					case 0: stagger = 42; break;
					case 1: stagger = -42; break;
					default: stagger = 21*(i-1); break;
				}

				keyBtn[i][j]->SetPosition(j*42+stagger+40, i*42+yoff);
#else
				keyBtn[i][j]->SetPosition(j*42+21*i+40, i*42+yoff);
#endif
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
#ifdef EXTENDED_KEYBOARD
	delete keyEscText;
	delete keyEscImg;
	delete keyEscOverImg;
	delete keyEsc;
	delete keyEnterText;
	delete keyEnterImg;
	delete keyEnterOverImg;
	delete keyEnter;
#endif
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
	delete trig2;

	for(int i=0; i<KB_ROWS; i++)
	{
		for(int j=0; j<KB_COLUMNS; j++)
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

#ifdef EXTENDED_KEYBOARD
	if(keyEsc->GetState() == STATE_CLICKED)
	{
		size_t len = strlen(kbtextstr);
		if(len < kbtextmaxlen-1)
		{
			kbtextstr[len] = 27; // Esc key code
			kbtextstr[len+1] = '\0';
			kbText->SetText(GetDisplayText(kbtextstr));
		}
		keyEsc->SetState(STATE_SELECTED, t->chan);
	}
	else if(keyEnter->GetState() == STATE_CLICKED)
	{
		size_t len = strlen(kbtextstr);
		if(len < kbtextmaxlen-1)
		{
			kbtextstr[len] = 13; // Enter key code
			kbtextstr[len+1] = '\0';
			kbText->SetText(GetDisplayText(kbtextstr));
		}
		keyEnter->SetState(STATE_SELECTED, t->chan);
	}
	else if(keySpace->GetState() == STATE_CLICKED)
#else
	if(keySpace->GetState() == STATE_CLICKED)
#endif
	{ // body for if from macro!
		size_t len = strlen(kbtextstr);
		if(len < kbtextmaxlen-1)
		{
			kbtextstr[len] = ' ';
			kbtextstr[len+1] = '\0';
			kbText->SetText(kbtextstr);
		}
		keySpace->SetState(STATE_SELECTED, t->chan);
	}
	else if(keyBack->GetState() == STATE_CLICKED)
	{
		size_t len = strlen(kbtextstr);
		if(len > 0)
		{
			kbtextstr[len-1] = '\0';
			kbText->SetText(GetDisplayText(kbtextstr));
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

	for(int i=0; i<KB_ROWS; i++)
	{
		for(int j=0; j<KB_COLUMNS; j++)
		{
			if(keys[i][j].ch != '\0')
			{
#ifdef EXTENDED_KEYBOARD
				if(update && keys[i][j].ch >= 32)
#else
				if(update)
#endif
				{
					if(shift || caps)
						txt[0] = keys[i][j].chShift;
					else
						txt[0] = keys[i][j].ch;

					keyTxt[i][j]->SetText(txt);
				}

				if(keyBtn[i][j]->GetState() == STATE_CLICKED)
				{
					size_t len = strlen(kbtextstr);

					if(len < kbtextmaxlen-1)
					{
						if(shift || caps)
						{
							kbtextstr[len] = keys[i][j].chShift;
						}
						else
						{
							kbtextstr[len] = keys[i][j].ch;
						}
						kbtextstr[len+1] = '\0';
					}
					kbText->SetText(GetDisplayText(kbtextstr));
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
