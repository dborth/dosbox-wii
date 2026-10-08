/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutBulkMemory.cpp
 *
 * Wii U has a single large heap, so bulk allocations use it directly.
 ***************************************************************************/
#include "../BulkMemory.h"

#include <malloc.h>

void * BulkMemory::allocate(size_t bytes)
{
	return memalign(64, bytes);
}
