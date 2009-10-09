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
#include "wiihardware.h"

char appPath[1024];

void WiiInit()
{
	fatInitDefault();
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

	int pos = 0;

	// replace fat:/ with sd:/
	if(strncmp(path, "fat:/", 5) == 0)
	{
		pos++;
		path[1] = 's';
		path[2] = 'd';
	}
	strncpy(appPath, &path[pos], MAXPATHLEN);
	appPath[MAXPATHLEN-1] = 0;
	free(path);
}

void WiiFinished()
{

}
