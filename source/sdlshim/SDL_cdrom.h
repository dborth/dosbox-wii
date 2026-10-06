/****************************************************************************
 * DOSBox Wii
 * SDL_cdrom.h
 *
 * SDL 1.2's CD-ROM audio API, as a stub. The console has no drive DOSBox
 * can play audio from, and the core only needs a physical CD for
 * `mount d /dev/cdrom -t cdrom`, so there are no drives: SDL_CDNumDrives()
 * returns 0, and the core's SDL CD interface (CDROM_Interface_SDL) then
 * fails to bind a device and the mount is refused. CD images (.iso/.cue)
 * go through CDROM_Interface_Image and are unaffected, including their
 * audio tracks.
 *
 * The types and macros are SDL 1.2's, because cdrom.cpp reads the track
 * table directly.
 *
 * Deliberately includes only <stdint.h>: it is used by DOSBox core files
 * and platform files whose headers collide.
 ***************************************************************************/
#ifndef SDL_CDROM_H
#define SDL_CDROM_H

#include <stdint.h>

typedef uint8_t  Uint8;
typedef uint32_t Uint32;
typedef int32_t  Sint32;

#define SDL_MAX_TRACKS 99

#define SDL_AUDIO_TRACK 0x00
#define SDL_DATA_TRACK  0x04

typedef enum {
	CD_TRAYEMPTY,
	CD_STOPPED,
	CD_PLAYING,
	CD_PAUSED,
	CD_ERROR = -1
} CDstatus;

#define CD_INDRIVE(status) ((int)(status) > 0)

typedef struct SDL_CDtrack {
	Uint8  id;
	Uint8  type;
	Uint32 length;  /* in frames */
	Uint32 offset;  /* in frames */
} SDL_CDtrack;

typedef struct SDL_CD {
	int      id;
	CDstatus status;
	int      numtracks;
	int      cur_track;
	int      cur_frame;
	SDL_CDtrack track[SDL_MAX_TRACKS + 1];
} SDL_CD;

#define CD_FPS 75
#define FRAMES_TO_MSF(f, M, S, F) {                 \
	int value = f;                                  \
	*(F) = value % CD_FPS;                          \
	value /= CD_FPS;                                \
	*(S) = value % 60;                              \
	value /= 60;                                    \
	*(M) = value;                                   \
}
#define MSF_TO_FRAMES(M, S, F) ((M) * 60 * CD_FPS + (S) * CD_FPS + (F))

//! Always 0: there are no drives.
int SDL_CDNumDrives(void);

//! NULL: there are no drives to name.
const char * SDL_CDName(int drive);

//! Always NULL.
SDL_CD * SDL_CDOpen(int drive);

//! CD_ERROR for any handle (and for NULL).
CDstatus SDL_CDStatus(SDL_CD * cdrom);

//! The remaining operations all fail, returning -1.
int SDL_CDPlay(SDL_CD * cdrom, int start, int length);
int SDL_CDPause(SDL_CD * cdrom);
int SDL_CDResume(SDL_CD * cdrom);
int SDL_CDStop(SDL_CD * cdrom);
int SDL_CDEject(SDL_CD * cdrom);

//! Closing NULL is a no-op.
void SDL_CDClose(SDL_CD * cdrom);

#endif
