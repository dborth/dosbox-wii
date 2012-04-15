/****************************************************************************
 * DOSBox Wii
 * Tantric 2009-2010
 ***************************************************************************/

#ifndef WIIHARDWARE_H
#define WIIHARDWARE_H
#include <wiiuse/wpad.h>

#define MAX_APP_DRIVE_LEN		16
#define MAX_APP_PATH_LEN		128

void WiiInit();
void WiiMenu();
void CreateAppPath(char origpath[]);
void WiiFinished();

extern char appDrive[MAX_APP_DRIVE_LEN];
extern char appPath[MAX_APP_PATH_LEN];
extern char dosboxCommand[1024];

#endif
