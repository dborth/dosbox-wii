/****************************************************************************
 * DOSBox Wii
 * Tantric 2009-2010
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <zlib.h>
#include <malloc.h>
#include <fat.h>
#include <asndlib.h>
#include <sys/iosupport.h>

#include "wiihardware.h"
#include "FreeTypeGX.h"
#include "input.h"
#include "filelist.h"
#include "SDL_events.h"

extern "C" {
extern void __exception_setreload(int t);
extern void WII_AudioStart();
extern void WII_AudioStop();
extern void WII_VideoStart();
extern void WII_VideoStop();
}

void MAPPER_CheckEvent(SDL_Event * event);
void HomeMenu();

char appDrive[MAX_APP_DRIVE_LEN];
char appPath[MAX_APP_PATH_LEN];
char dosboxCommand[1024] = { 0 };
static lwp_t keythread = LWP_THREAD_NULL;
static char shiftkey[130];

/****************************************************************************
 * SwitchAudioMode
 *
 * Switches between menu sound and emulator sound
 ***************************************************************************/
static void SwitchAudioMode(int mode)
{
	if(mode == 0) // emulator
	{
		ASND_Pause(1);
		AUDIO_StopDMA();
		WII_AudioStart();
	}
	else // menu
	{
		WII_AudioStop();
		ASND_Init();
		ASND_Pause(0);
	}
}

static void * PressKeys (void *arg)
{
	SDL_Event event;
	int shift;
	u16 i;
	
	memset(shiftkey, 0, sizeof(shiftkey));
	shiftkey[33] = 49;
	shiftkey[34] = 39;
	shiftkey[35] = 51;
	shiftkey[36] = 52;
	shiftkey[37] = 53;
	shiftkey[38] = 55;
	shiftkey[40] = 57;
	shiftkey[41] = 48;
	shiftkey[42] = 56;
	shiftkey[58] = 59;
	shiftkey[60] = 44;
	shiftkey[62] = 46;
	shiftkey[63] = 47;
	shiftkey[64] = 50;
	shiftkey[94] = 54;
	shiftkey[95] = 45;
	shiftkey[126] = 96;

	while(1)
	{
		LWP_SuspendThread(keythread);
		usleep(1200);

		for(i=0; i<strlen(dosboxCommand); i++)
		{
			shift=0;

			if((dosboxCommand[i] >= 65 && dosboxCommand[i] <= 90))
			{
				dosboxCommand[i] += 32;
				shift = 1;
			}
			else if(dosboxCommand[i] > 0 && dosboxCommand[i] < 130 && 
					shiftkey[(int)dosboxCommand[i]] > 0)
			{
				dosboxCommand[i] = shiftkey[(int)dosboxCommand[i]];
				shift = 1;
			}

			if(shift)
			{
				event.type = SDL_KEYDOWN;
				event.key.keysym.sym = SDLK_LSHIFT;
				MAPPER_CheckEvent(&event);
				usleep(600);
			}
			
			// hack to allow mappings of SDL keys > 127
			int keyoffset = 0;
			if(dosboxCommand[i] >= 14 && dosboxCommand[i] <= 25)
				keyoffset = 268; // F1-F12 (282-293)

			event.type = SDL_KEYDOWN;
			event.key.keysym.sym = (SDLKey)((int)dosboxCommand[i]+keyoffset);
			MAPPER_CheckEvent(&event);
			usleep(600);

			event.type = SDL_KEYUP;
			event.key.keysym.sym = (SDLKey)((int)dosboxCommand[i]+keyoffset);
			MAPPER_CheckEvent(&event);
			usleep(600);

			if(shift)
			{
				event.type = SDL_KEYUP;
				event.key.keysym.sym = SDLK_LSHIFT;
				MAPPER_CheckEvent(&event);
				usleep(600);
			}
		}
		dosboxCommand[0] = 0;
	}
	return NULL;
}

/****************************************************************************
 * USB Gecko Debugging
 ***************************************************************************/

static bool gecko = false;
static mutex_t gecko_mutex = 0;

static ssize_t __out_write(struct _reent *r, int fd, const char *ptr, size_t len)
{
	u32 level;

	if (!ptr || len <= 0 || !gecko)
		return -1;

	LWP_MutexLock(gecko_mutex);
	level = IRQ_Disable();
	usb_sendbuffer(1, ptr, len);
	IRQ_Restore(level);
	LWP_MutexUnlock(gecko_mutex);
	return len;
}

const devoptab_t gecko_out = {
	"stdout",	// device name
	0,			// size of file structure
	NULL,		// device open
	NULL,		// device close
	__out_write,// device write
	NULL,		// device read
	NULL,		// device seek
	NULL,		// device fstat
	NULL,		// device stat
	NULL,		// device link
	NULL,		// device unlink
	NULL,		// device chdir
	NULL,		// device rename
	NULL,		// device mkdir
	0,			// dirStateSize
	NULL,		// device diropen_r
	NULL,		// device dirreset_r
	NULL,		// device dirnext_r
	NULL,		// device dirclose_r
	NULL		// device statvfs_r
};

void USBGeckoOutput()
{
	LWP_MutexInit(&gecko_mutex, false);
	gecko = usb_isgeckoalive(1);
	
	devoptab_list[STD_OUT] = &gecko_out;
	devoptab_list[STD_ERR] = &gecko_out;
}

void WiiInit()
{
	extern const devoptab_t dotab_stdnull;
	devoptab_list[STD_OUT] = &dotab_stdnull;
	devoptab_list[STD_ERR] = &dotab_stdnull;
	//USBGeckoOutput(); // uncomment to enable USB gecko output
	__exception_setreload(8);
	fatInitDefault();
	ASND_Init();
	SetupPads();
	InitFreeType((u8*)font_ttf, font_ttf_size);
	LWP_CreateThread (&keythread, PressKeys, NULL, NULL, 0, 65);
	appPath[0] = 0;
}

void CreateAppPath(char origpath[])
{
	char * path = strdup(origpath); // make a copy so we don't mess up original

	if(!path)
		return;

	char * loc = strrchr(path,'/');
	if (loc != NULL)
		*loc = 0; // strip file name

	strncpy(appPath, path, MAX_APP_PATH_LEN);
	appPath[MAX_APP_PATH_LEN - 1] = 0;

	loc = strchr(path,'/');
	if (loc != NULL)
		*loc = 0; // strip path

	strncpy(appDrive, path, MAX_APP_DRIVE_LEN);
	appDrive[MAX_APP_DRIVE_LEN - 1] = 0;

	free(path);
}

void WiiMenu()
{
	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(keythread))
		usleep(100);
	
	WII_VideoStop();
	SwitchAudioMode(1);

	HomeMenu();
	
	WII_VideoStart();
	SwitchAudioMode(0);
	
	if(dosboxCommand[0] != 0)
		LWP_ResumeThread(keythread);
}

void WiiFinished()
{

}
