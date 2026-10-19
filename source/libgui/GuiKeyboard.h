/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * GuiKeyboard.h
 ***************************************************************************/
#pragma once

constexpr int KB_ROWS = 4;
constexpr int KB_COLUMNS = 11;

#define MAX_KEYBOARD_DISPLAY	32

//!Unshifted/shifted character pair for a single keyboard key.
typedef struct _keytype {
	char ch;      //!< Unshifted character
	char chShift; //!< Character produced while shift/caps is active
} Key;

//!On-screen keyboard
//!\ingroup grp_core
class GuiKeyboard : public GuiWindow
{
	public:
		//!\param t Initial text buffer contents (copied into kbtextstr), or nullptr for empty
		//!\param m Maximum text length, must be < sizeof(kbtextstr)
		GuiKeyboard(char * t, uint32_t m);
		~GuiKeyboard();
		void update(InputController * c);
		char kbtextstr[256] = {}; //!< Current entered text - read this after the keyboard closes
	protected:
		//!\cond INTERNAL
		uint32_t kbtextmaxlen;
		int shift;
		int caps;
		GuiText * kbText;
		GuiShape * keyTextboxShape;
		GuiText * keyCapsText;
		GuiShape * keyCapsImg;
		GuiButton * keyCaps;
		GuiText * keyShiftText;
		GuiShape * keyShiftImg;
		GuiButton * keyShift;
		GuiText * keyBackText;
		GuiShape * keyBackImg;
		GuiButton * keyBack;
		GuiText * keyEnterText;
		GuiShape * keyEnterImg;
		GuiButton * keyEnter;
		GuiShape * keySpaceImg;
		GuiButton * keySpace;
		GuiButton * keyBtn[KB_ROWS][KB_COLUMNS];
		GuiShape * keyImg[KB_ROWS][KB_COLUMNS];
		GuiText * keyTxt[KB_ROWS][KB_COLUMNS];
		GuiTrigger * trigA;
		Key keys[KB_ROWS][KB_COLUMNS]; //!< two chars = less space than one pointer
		//!\endcond
};
