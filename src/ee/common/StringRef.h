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

#ifndef STRINGREF_H
#define STRINGREF_H

#include <cassert>
#include <cstddef>

namespace voltdb
{
class Pool;

/**
 * An object to use in lieu of raw char* pointers for strings which are not
 * inlined into tuple storage.  This provides a constant address to be stored
 * in tuple storage while allowing the memory containing the actual string to
 * be moved around as the result of string pool compaction.
 */
class StringRef
{
public:
    /// Utility method to compute the amount of memory that will
    /// be used by non-inline storage of a string/varbinary of the
    /// given length.  Includes the size of pooled StringRef object,
    /// backpointer, and excess memory allocated in the compacting
    /// string pool.
    static std::size_t computeStringMemoryUsed(std::size_t length);

    /// Create and return a new StringRef object which points to an
    /// allocated memory block of the requested size.  The caller
    /// may provide an optional Pool from which the memory (and
    /// the memory for the StringRef object itself) will be
    /// allocated, intended for temporary strings.  If no Pool
    /// object is provided, the StringRef and the string memory will be
    /// allocated out of the persistent ThreadLocalPool.
    static StringRef* create(std::size_t size, Pool* tempPool);

    /// Destroy the given StringRef object and free any memory
    /// allocated from persistent pools.
    /// sref must have been allocated and returned by a call to
    /// StringRef::create()
    static void destroy(StringRef* sref);

    char* get() { return m_stringPtr + sizeof(void*); }
    const char* get() const { return m_stringPtr + sizeof(void*); }

private:
    /// Constructor strictly for use with persistent string pool,
    /// which allocates and assigns the char* referent in the constructor
    /// as separate memory that is relocatable by the CompactingStringPool.
    /// Layout of persistent StringRef and its relocatable persistent string data:
    ///
    ///   -------StringRef------             --separate char[] allocation--
    ///  | m_size | m_stringPtr |           | back pointer | string data   |
    ///   --------^-----------v-            ^------------v-----------------
    ///           |           |_____________|            |
    ///           |______________________________________|
    ///
    StringRef(std::size_t size);

    /// Constructor strictly for use with temporary string pool,
    /// which assumes that the char* referent was preallocated at a fixed
    /// location contiguous with this StringRef -- both are purposely
    /// leaked until the pool is purged.
    /// Layout of temporary StringRef and its contiguous string data:
    ///
    ///   -------StringRef------ ---extra allocated bytes---
    ///  | m_size | m_stringPtr | string data               |
    ///   --------^-----------v- ---------------------------
    ///           |___________|
    ///
    StringRef()
        : m_size(0)
        // The string buffer was already allocated at the end of the StringRef.
        // To save 8 bytes per StringRef, the allocation did not account for a
        // useless back pointer prefix in the string buffer.
        // Still, for speed and simplicity, the "get" method unconditionally
        // adds 8-bytes from m_stringPtr to find the string data.
        // So, m_stringPtr needs to be initialized here to 8 bytes BEFORE
        // the start of the string data.
        // Since the string data is contiguous to the end of the StringRef,
        // this is 8 bytes before the end of the StringRef.
        // Conveniently, if we assert (as we do here) that m_stringPtr takes up
        // the last 8 bytes of the StringRef, m_stringPtr can simply be initialized
        // to its own address.
        // Coincidentally, this makes it a valid back pointer,
        // but it is never used that way -- temporary strings are never relocated.
        , m_stringPtr(reinterpret_cast<char*>(&m_stringPtr))
        {
            // Ensure that m_stringPtr is actually located at the last 8 bytes
            // of the StringRef so that "get" will work correctly.
            assert (m_stringPtr ==
                    reinterpret_cast<char*>(this) + sizeof(StringRef) - sizeof(char*));
            // Equivalently, assert that "get" gives the expected address just past
            // this StringRef object -- one StringRef size away from "this".
            assert (get() == reinterpret_cast<char*>(this) + sizeof(StringRef));
        }

    bool wasTempPoolAllocated() const
    { return m_stringPtr == reinterpret_cast<const char*>(&m_stringPtr); }

    /// Only used within "destroy", and only for a persistent string.
    ~StringRef();

    /// Only used within "destroy", and only for a persistent string.
    void operator delete(void* persistent);

    std::size_t m_size;
    char* m_stringPtr;
};

}

#endif // STRINGREF_H
