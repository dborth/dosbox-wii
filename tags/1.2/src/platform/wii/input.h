/****************************************************************************
 * DOSBox Wii Home Menu
 * Tantric 2009
 *
 * input.h
 *
 * Wii/Gamecube controller management
 ***************************************************************************/

#ifndef _INPUT_H_
#define _INPUT_H_

#include <gccore.h>
#include <wiiuse/wpad.h>

#define PI 				3.14159265f
#define PADCAL			50

extern int rumbleRequest[4];

void ShutoffRumble();
void DoRumble(int i);
void SetupPads();
void UpdatePads();
bool MenuRequested();

#endif
