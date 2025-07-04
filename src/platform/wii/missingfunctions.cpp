#include <cstdio>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/param.h>
#include "dos_inc.h"

static char tmp[MAXPATHLEN];

char * dirname(char * file)
{
	if(!file || file[0] == 0)
		return ".";

	char * sep = strrchr(file, '/');
	if (sep == NULL)
		sep = strrchr(file, '\\');
	if (sep == NULL)
		return ".";

	int len = (int)(sep - file);
	safe_strncpy(tmp, file, len+1);
	return tmp;
}
int access(const char *path, int amode)
{
	struct stat st;
	bool folderExists = (stat(path, &st) == 0);
	if (folderExists) return 0;
	else return ENOENT;
}
int execlp(const char *file, const char *arg, ...)
{
	return -1;
}
