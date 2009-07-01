#ifndef WIIHARDWARE_H
#define WIIHARDWARE_H
#include <wiiuse/wpad.h>

void WiiInit();
void CreateAppPath(char origpath[]);
void WiiFinished();

extern char appPath[1024];

#endif
