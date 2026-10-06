/****************************************************************************
 * DOSBox Wii
 * SDL_cdrom.cpp
 *
 * See SDL_cdrom.h.
 ***************************************************************************/
#include "SDL_cdrom.h"

int SDL_CDNumDrives(void)
{
	return 0;
}

const char * SDL_CDName(int)
{
	return 0;
}

SDL_CD * SDL_CDOpen(int)
{
	return 0;
}

CDstatus SDL_CDStatus(SDL_CD *)
{
	return CD_ERROR;
}

int SDL_CDPlay(SDL_CD *, int, int)  { return -1; }
int SDL_CDPause(SDL_CD *)           { return -1; }
int SDL_CDResume(SDL_CD *)          { return -1; }
int SDL_CDStop(SDL_CD *)            { return -1; }
int SDL_CDEject(SDL_CD *)           { return -1; }

void SDL_CDClose(SDL_CD *)
{
}
