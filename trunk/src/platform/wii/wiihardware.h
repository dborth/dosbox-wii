#ifndef WIIHARDWARE_H
#define WIIHARDWARE_H
#include <wiiuse/wpad.h>

void WiiInit();
void CreateAppPath(char origpath[]);
void WiiFinished();
bool WiiMessagePause(const char *s);

extern char appPath[1024];

#endif
