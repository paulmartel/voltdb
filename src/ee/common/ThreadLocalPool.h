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

#ifndef THREADLOCALPOOL_H_
#define THREADLOCALPOOL_H_

#include "boost/pool/pool.hpp"
#include "boost/shared_ptr.hpp"

namespace voltdb {

struct voltdb_pool_allocator_new_delete
{
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;

  static char * malloc(const size_type bytes);
  static void free(char * const block);
};

/**
 * A wrapper around a set of pools that are local to the current thread.
 * An instance of the thread local pool must be maintained somewhere in the thread to ensure initialization
 * and destruction of the thread local pools. Creating multiple instances is fine, it is reference counted. The thread local
 * instance of pools will be freed once the last ThreadLocalPool reference in the thread is destructed.
 */
class ThreadLocalPool {
public:
    ThreadLocalPool();
    ~ThreadLocalPool();

    static const int POOLED_MAX_VALUE_LENGTH;

    /// This generic packaging of fixed-size allocation hides the underlying pool.
    static void* allocateExactSize(std::size_t size)
    { return getExact(size)->malloc(); }

    /// This generic packaging of fixed-size deallocation hides the underlying pool.
    static void freeExactSizeAllocation(std::size_t size, void* allocated)
    { getExact(size)->free(allocated); }

    /// This generic packaging of variable approximate-size allocation hides the underlying pool.
    /// It also encapsulates the disabling of the StringPool when MEMCHECK is enabled.
    static char* allocateRelocatable(std::size_t size, char** referringAddress);

    /// This generic packaging of variable approximate-size deallocation hides the underlying pool.
    /// It also encapsulates the disabling of the StringPool when MEMCHECK is enabled.
    static void freeRelocatable(std::size_t size, char* allocated);

     /**
     * Return the nearest power-of-two-plus-or-minus buffer size that
     * will be allocated for an object of the given length
     */
    static std::size_t getAllocationSizeForObject(std::size_t length);

    /**
     * Retrieve a pool that allocates approximately sized chunks of memory. Provides pools that
     * are powers of two and powers of two + the previous power of two.
     */
    static boost::shared_ptr<boost::pool<voltdb_pool_allocator_new_delete> > get(std::size_t size);

    /**
     * Retrieve a pool that allocate chunks that are exactly the requested size. Only creates
     * pools up to 1 megabyte + 4 bytes.
     */
    static boost::shared_ptr<boost::pool<voltdb_pool_allocator_new_delete> > getExact(std::size_t size);

    static std::size_t getPoolAllocationSize();
};

/// This alternative implementation approximate-size allocation
/// encapsulates the disabling of the StringPool when MEMCHECK is enabled.
/// Since it doesn't implement compaction/relocation of the relocatable,
/// It goes through the motions of setting up back pointer and forward pointer,
/// for consistency that can be asserted in the caller regardless of whether
/// MEMCHECK is enabled. The backpointer will never need to be used or changed
/// because MEMCHECK bypasses the CompactingStringPool with its
/// pooling/compacting/relocating functionality.
#ifdef MEMCHECK
char* ThreadLocalPool::allocateRelocatable(std::size_t sz, char** referringAddress)
{
    // There had better be enough space allocated for at least the back-pointer.
    assert(sz >= 8);
    char* result = new char[sz];
    // Set the back-pointer.
    *reinterpret_cast<char***>(result) = referringAddress;
    // Set the forward pointer -- though caller will likely do this as well.
    *referringAddress = result;
    return result;
}

/// This alternative implementation of approximate-size-specific deallocation
/// encapsulates the disabling of the StringPool when MEMCHECK is enabled.
void ThreadLocalPool::freeRelocatable(std::size_t sz, char* string)
{
    assert(sz >= 8);
    // There had better have been enough space allocated for at least the back-pointer.
    delete[] string;
}
#endif

} // namespace voltdb

#endif /* THREADLOCALPOOL_H_ */
