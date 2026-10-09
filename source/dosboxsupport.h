/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * dosboxsupport.h
 *
 * What the DOSBox core needs from the app. Included by main.cpp, which
 * must stay free of DOSBox headers.
 ***************************************************************************/

#ifndef _DOSBOXSUPPORT_H_
#define _DOSBOXSUPPORT_H_

//! Reads the config and runs DOSBox until the user exits it. The platform
//! and GUI must already be up. Returns when DOSBox has shut down.
void RunDOSBox(int argc, char* argv[]);

//! True when DOSBox is at the DOS prompt rather than inside a program, which
//! is when it is safe to change hardware settings (sound card, joystick, CPU
//! core) under it. False before the first shell exists.
bool IsShellIdle();

//! The fixed output format of the emulator audio driver, as DOSBox's mixer
//! runs it: sample rate in Hz and stereo frames per buffer. Returns false
//! when there is no emulator audio driver (nothing to play through), in
//! which case neither value is set.
bool GetEmulatorAudioInfo(int * sampleRate, int * framesPerBuffer);

#endif
