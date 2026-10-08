/****************************************************************************
 * Platform Abstraction Layer (Wii driver)
 * Daryl Borth 2026
 * WiiBulkMemory.cpp
 *
 * Bulk allocations are taken from the MEM2 heap explicitly
 ***************************************************************************/
#include "../../BulkMemory.h"

#include <malloc.h>

void * BulkMemory::allocate(size_t bytes)
{
	return mem2_memalign(32, bytes);
}

void BulkMemory::release(void * block)
{
	if (block) mem2_free(block);
}
