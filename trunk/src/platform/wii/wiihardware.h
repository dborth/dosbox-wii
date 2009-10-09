/****************************************************************************
 * DOSBox Wii
 * Tantric 2009
 ***************************************************************************/

#ifndef WIIHARDWARE_H
#define WIIHARDWARE_H
#include <wiiuse/wpad.h>

void WiiInit();
void WiiMenu();
void CreateAppPath(char origpath[]);
void WiiFinished();

extern char appPath[1024];
extern char dosboxCommand[1024];

#endif
