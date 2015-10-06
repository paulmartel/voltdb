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
#include "common/CompactingStringStorage.h"

#include "common/FatalException.hpp"
#include "common/ThreadLocalPool.h"

namespace voltdb {

CompactingStringStorage::PoolPtr CompactingStringStorage::get(std::size_t size)
{
    size_t alloc_size = ThreadLocalPool::getAllocationSizeForObject(size);
    if (alloc_size == 0) {
        throwFatalException("Attempted to allocate an object then the 1 meg limit. Requested size was %d",
                            static_cast<int32_t>(size));
    }
    PoolMapIter iter = m_poolMap.find(alloc_size);
    if (iter == m_poolMap.end()) {
        // compute num_elements to be closest multiple
        // leading to a 2Meg buffer
        int32_t ssize = static_cast<int32_t>(alloc_size);
        int32_t num_elements = (2 * 1024 * 1024 / ssize) + 1;
        PoolPtr pool(new CompactingStringPool(alloc_size, num_elements));
        m_poolMap.insert(std::pair<std::size_t, PoolPtr>(alloc_size, pool));
        return pool;
    }
    return iter->second;
}

} // namespace voltdb
