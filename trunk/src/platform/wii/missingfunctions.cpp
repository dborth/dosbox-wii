#include <cstdio>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include "dos_inc.h"
#define MAX_FILENAME_LENGTH 256

static char tmp[MAX_FILENAME_LENGTH];
char * dirname(char * file) {
	// CAKTODO
	char * sep = strrchr(file, '/');
	if (sep == NULL)
		sep = strrchr(file, '\\');
	if (sep == NULL)
		return "";
	else {
		int len = (int)(sep - file);
		safe_strncpy(tmp, file, len+1);
		return tmp;
	}
}
int access(const char *path, int amode) {
	FILE *F = fopen(path, "rb");
	if (F) {
		fclose(F);
		return 0;
	} else {
		return ENOENT;
	}
}
int rmdir(const char *path) {
	return remove(path);
}
int execlp(const char *file, const char *arg, ...) {
	return -1;
}
