
/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * dosprogram.h
 *
 * Which program DOS is running, read from the environment block DOS_Execute
 * writes. Plain std, so it builds and tests on a PC; dosboxsupport.cpp is the
 * one that reads the block out of DOS memory.
 ***************************************************************************/
#ifndef _DOSPROGRAM_H_
#define _DOSPROGRAM_H_

#include <stddef.h>
#include <string>

/**
 * The program's whole path as DOS wrote it into an environment block ("C:\GAMES\KEEN4\KEEN4E.EXE"): after the
 * variables' closing empty string there is a word (1) and the program's full path, which is where DOS_Execute puts
 * it. Empty when the block is not in that shape. `size` is the bytes in the block.
 */
std::string Program_PathFromEnvironment(const unsigned char * env, size_t size);

/** The same block's program as a file name alone, upper case, with its extension ("KEEN4E.EXE"); empty when the block is not in that shape. */
std::string Program_FromEnvironment(const unsigned char * env, size_t size);

#endif
