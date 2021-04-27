/****************************************************************************
 * DOSBox Wii Home Menu
 * Tantric 2009-2010
 *
 * filelist.h
 *
 * Contains a list of all of the files stored in the images/, fonts/, and
 * sounds/ folders
 ***************************************************************************/

#ifndef _FILELIST_H_
#define _FILELIST_H_

#ifdef OLD_LIBS

#include <gccore.h>

extern const u8		font_ttf[];
extern const u32	font_ttf_size;

extern const u8		enter_ogg[];
extern const u32	enter_ogg_size;

extern const u8		exit_ogg[];
extern const u32	exit_ogg_size;

extern const u8		button_over_pcm[];
extern const u32	button_over_pcm_size;

extern const u8		button_click_pcm[];
extern const u32	button_click_pcm_size;

extern const u8		logo_png[];
extern const u32	logo_png_size;

extern const u8		logo_over_png[];
extern const u32	logo_over_png_size;

extern const u8		bg_top_png[];
extern const u32	bg_top_png_size;

extern const u8		bg_bottom_png[];
extern const u32	bg_bottom_png_size;

extern const u8		button_png[];
extern const u32	button_png_size;

extern const u8		button_over_png[];
extern const u32	button_over_png_size;

extern const u8		button_small_png[];
extern const u32	button_small_png_size;

extern const u8		button_small_over_png[];
extern const u32	button_small_over_png_size;

extern const u8		button_large_png[];
extern const u32	button_large_png_size;

extern const u8		button_large_over_png[];
extern const u32	button_large_over_png_size;

extern const u8		battery_png[];
extern const u32	battery_png_size;

extern const u8		battery_red_png[];
extern const u32	battery_red_png_size;

extern const u8		battery_bar_png[];
extern const u32	battery_bar_png_size;

extern const u8		credits_box_png[];
extern const u32	credits_box_png_size;

extern const u8		keyboard_textbox_png[];
extern const u32	keyboard_textbox_png_size;

extern const u8		keyboard_key_png[];
extern const u32	keyboard_key_png_size;

extern const u8		keyboard_key_over_png[];
extern const u32	keyboard_key_over_png_size;

extern const u8		keyboard_mediumkey_png[];
extern const u32	keyboard_mediumkey_png_size;

extern const u8		keyboard_mediumkey_over_png[];
extern const u32	keyboard_mediumkey_over_png_size;

extern const u8		keyboard_largekey_png[];
extern const u32	keyboard_largekey_png_size;

extern const u8		keyboard_largekey_over_png[];
extern const u32	keyboard_largekey_over_png_size;

extern const u8		player1_point_png[];
extern const u32	player1_point_png_size;

extern const u8		player2_point_png[];
extern const u32	player2_point_png_size;

extern const u8		player3_point_png[];
extern const u32	player3_point_png_size;

extern const u8		player4_point_png[];
extern const u32	player4_point_png_size;

#else

#include "font_ttf.h"

#include "enter_ogg.h"
#include "exit_ogg.h"

#include "button_over_pcm.h"
#include "button_click_pcm.h"

#include "logo_png.h"
#include "logo_over_png.h"

#include "bg_top_png.h"
#include "bg_bottom_png.h"

#include "button_png.h"
#include "button_over_png.h"

#include "button_small_png.h"
#include "button_small_over_png.h"
#include "button_large_png.h"
#include "button_large_over_png.h"

#include "battery_png.h"
#include "battery_red_png.h"
#include "battery_bar_png.h"

#include "credits_box_png.h"

#include "keyboard_textbox_png.h"
#include "keyboard_key_png.h"
#include "keyboard_key_over_png.h"
#include "keyboard_mediumkey_png.h"
#include "keyboard_mediumkey_over_png.h"
#include "keyboard_largekey_png.h"
#include "keyboard_largekey_over_png.h"

#include "player1_point_png.h"
#include "player2_point_png.h"
#include "player3_point_png.h"
#include "player4_point_png.h"

#endif

#endif
