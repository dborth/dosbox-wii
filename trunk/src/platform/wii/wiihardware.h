#ifndef WIIHARDWARE_H
#define WIIHARDWARE_H
#include <wiiuse/WPAD.h> 

void WiiInit();
void WiiFinished();
bool WiiMessagePause(const char *s);

#endif
