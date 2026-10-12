/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * buttons.h
 *
 * The Buttons page's model: what each button of player 1's pad does in DOS,
 * and the choices for changing it. A button can do nothing, press a joystick
 * button, a mouse button, a key, or run a hotkey (more cycles, frameskip,
 * swap disk). It does what the mapper says (sdl_mapper.cpp), and a change is
 * put in effect and written to the mapper file at once.
 *
 * Like status.h this is plain C++ with no DOSBox and no libgui types: the
 * menu sees only text and indexes.
 ***************************************************************************/

#ifndef _BUTTONS_H_
#define _BUTTONS_H_

#include <stddef.h>

//! Reads the mapper again and rebuilds every row. Call when the page opens
//! and after anything is changed.
void Buttons_Refresh();

//! Rows: HOME (fixed), then each button, then the pointer's clicks (fixed),
//! then "Restore all buttons".
int Buttons_RowCount();
const char * Buttons_RowLabel(int row);
void Buttons_RowValue(int row, char * buf, size_t size);
void Buttons_RowHelp(int row, char * buf, size_t size);

//! What a row is for: BUTTONS_ROW_BUTTON rows open the choices, _RESTORE gives every button its original
//! action, the others are information.
enum { BUTTONS_ROW_INFO, BUTTONS_ROW_BUTTON, BUTTONS_ROW_RESTORE };
int Buttons_RowKind(int row);

//! The lists a button's choices are in: the first has the actions, two of which open the others.
enum { BUTTONS_LIST_ACTIONS, BUTTONS_LIST_KEYS, BUTTONS_LIST_HOTKEYS };
int Buttons_ListCount(int list);
const char * Buttons_ListLabel(int list, int item);

//! What choosing an item did.
enum { BUTTONS_PICK_DONE, BUTTONS_PICK_OPEN_KEYS, BUTTONS_PICK_OPEN_HOTKEYS, BUTTONS_PICK_FAILED };

//! Chooses an item of a list for the button at this row. DONE and FAILED fill message (why, or that it was
//! saved) and have already changed what the button does; the OPEN ones change nothing.
int Buttons_Pick(int row, int list, int item, char * message, size_t size);

//! Gives every button its original action, and says whether that worked.
bool Buttons_RestoreAll(char * message, size_t size);

#endif
