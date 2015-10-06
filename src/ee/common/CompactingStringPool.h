/* This file is part of VoltDB.
 * Copyright (C) 2008-2015 VoltDB Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with VoltDB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _EE_COMMON_COMPACTINGSTRINGPOOL_H_
#define _EE_COMMON_COMPACTINGSTRINGPOOL_H_

#include "structures/ContiguousAllocator.h"

#include <cstring>

namespace voltdb
{
/// CompactingStringPool is just a typical contiguous allocator except that its
/// entries are assumed to start with a back-pointer to the one and only pointer
/// to that entry.
/// This allows entries to be safely relocated.
/// In particular, it makes continuous compaction trivial.
class CompactingStringPool : public ContiguousAllocator {
public:
    CompactingStringPool(int32_t elementSize, int32_t elementsPerBuf)
        : ContiguousAllocator(elementSize, elementsPerBuf)
    { }

    /// Free a no-longer-needed element without leaving a hole at its location.
    /// This is acheived by filling any hole with the contents of the last entry,
    /// updating the one external pointer to that last entry, and shrinking
    /// the used area to exclude the new hole left at the last entry's location.
    void free(void* element)
    {
        void* trimmable = last();
        if (trimmable != element) {
            // Free element indirectly by copying the last entry over it
            // and trimming away the last entry.
            ::memcpy(element, trimmable, allocSize());
            // Use the back pointer copied from the head of trimmable
            // to locate the forward pointer to trimmable and reset it
            // to element, which is the new location for the entry that
            // was at trimmable.
            // This has no effect on the existing forward pointer to element,
            // which is hopefully about to fall out of use or get
            // overwritten by the caller.
            void** to_trimmable = *reinterpret_cast<void***>(element);
            *to_trimmable = element;
        }
        trim(); // reclaim the last entry's location
    }

    // Undefned and private to discourage callers from forgetting to provide the
    // back-pointer value that makes entry relocation and compaction possible.
    // The char* return (vs. void*) is a concession to the fact that the caller
    // will often need to use char offsets to skip around the back-pointer and get
    // to the allocated content.
    char* alloc(char** referringAddress)
    {
        void* allocation = ContiguousAllocator::alloc();
        // Set the back-pointer.
        char*** backPointer = reinterpret_cast<char***>(allocation);
        *backPointer = referringAddress;
        // It doesn't hurt to get the referrer properly initialized,
        // though it may be syntactically more natural for the callers
        // to handle this anyway using the return value, like
        // "pointer = pool->alloc(&pointer);"
        char* result = reinterpret_cast<char*>(allocation);
        *referringAddress = result;
        return result;
    }

 private:
    // Undefned and private to discourage callers from forgetting to provide the
    // back-pointer value that makes entry relocation and compaction possible.
    void *alloc();
};

}

#endif // _EE_COMMON_COMPACTINGSTRINGPOOL_H_
