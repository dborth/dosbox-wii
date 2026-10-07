/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * Codegen.h
 *
 * JIT code cache support for the DOSBox dynrec core
 *
 * Owns where generated code lives and how it becomes executable. The core
 * never allocates, protects or flushes code memory itself; it brackets every
 * write to the code cache in a write window:
 *
 *     beginWrite();
 *     ... emit code, markDirty() the written ranges ...
 *     endWrite();
 *
 * Generated code is never executed while a write window is open, and is
 * never modified outside one. Platforms that must flip the region between
 * writable and executable (Wii U) rely on this; platforms that don't (Wii)
 * only use endWrite() as the point to make the written range visible to
 * instruction fetch.
 *
 * Implementations: WiiCodegen.cpp (ogc/wii), WutCodegen.cpp (wut)
 ***************************************************************************/
#pragma once

#include <cstddef>
#include <cstdint>

class Codegen
{
	public:
		//! Claims the memory the code cache will live in. Called once.
		//! The platform decides how much it can give: the region may be
		//! smaller than preferred, but never smaller than minimum.
		//! \param preferred the size wanted; the pointer need not be page
		//! aligned (the caller aligns what it needs)
		//! \param minimum the smallest size worth having
		//! \param got set to the size of the returned region
		//! \return the region, or nullptr if less than minimum is available
		static uint8_t * acquire(size_t preferred, size_t minimum, size_t & got);

		//! Opens a write window. Nestable - only the outermost call
		//! changes the region's state.
		static void beginWrite();

		//! Records [p, p+n) as written during the open window. Does not
		//! touch the cache or hardware; that happens once, for the whole
		//! accumulated range, in endWrite().
		static void markDirty(const void * p, size_t n);

		//! Closes a write window. On the outermost matching call, makes
		//! every range marked dirty since beginWrite() visible to
		//! instruction fetch.
		static void endWrite();
};
