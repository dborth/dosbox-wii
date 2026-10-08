/****************************************************************************
 * Platform Abstraction Layer (Wii driver)
 * Daryl Borth 2026
 * WiiBulkMemory.cpp
 *
 * Bulk allocations live in MEM2. Plain malloc()/new use the MEM1 heap, which
 * has to stay free for the framebuffers, GX and the GUI.
 ***************************************************************************/
#include "../../BulkMemory.h"

#include <malloc.h>

void * BulkMemory::allocate(size_t bytes)
{
	return mem2_memalign(32, bytes);
}
