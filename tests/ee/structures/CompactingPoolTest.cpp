/* This file is part of VoltDB.
 * Copyright (C) 2008-2015 VoltDB Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "common/CompactingStringPool.h"

#include "harness.h"
#include <iostream>
#include <cstring>
#include <boost/scoped_ptr.hpp>

using namespace voltdb;
using namespace std;

class CompactingPoolTest : public Test
{
public:
    CompactingPoolTest() { }

    ~CompactingPoolTest() { }
};

TEST_F(CompactingPoolTest, basic_ops)
{
    int32_t size = 17;
    int32_t num_elements = 7;
    CompactingStringPool dut(size, num_elements);

    // test freeing with just one element is happy
    char* elem = dut.alloc(&elem);
    EXPECT_EQ(&elem, *reinterpret_cast<char***>(elem));
    EXPECT_EQ(size * num_elements, dut.bytesAllocated());
    dut.free(elem);
    EXPECT_EQ(0, dut.bytesAllocated());

    // fill up a buffer + 1, then free something in the middle and
    // verify that we shrink appropriately
    char* elems[num_elements + 1];
    for (int i = 0; i <= num_elements; i++) {
        elems[i] = dut.alloc(&(elems[i]));
        // Quickly mark the allocated space for later identification,
        // except for the first 8 bytes of back-pointer.
        memset(elems[i]+sizeof(char*), i, size-sizeof(char*));
        // Validate the required back-pointer into elems.
        EXPECT_EQ(&(elems[i]), *reinterpret_cast<char***>(elems[i]));
    }
    // validate the id mark after the 8-byte back-pointer
    EXPECT_EQ(2, reinterpret_cast<int8_t*>(elems[2])[8]);
    EXPECT_EQ(size * num_elements * 2, dut.bytesAllocated());
    dut.free(elems[2]);
    // 2 should now have the last element, filled with num_elements
    EXPECT_EQ(num_elements, reinterpret_cast<int8_t*>(elems[2])[8]);
    // elem[num_elements] should have been patched to use the new location.
    EXPECT_EQ(elems[num_elements], elems[2]);
    // and we should have shrunk back to 1 buffer
    EXPECT_EQ(size * num_elements, dut.bytesAllocated());

    // add an element and free it and verify that we don't mutate anything else
    elems[num_elements + 1] = dut.alloc(&(elems[num_elements + 1]));
    // Quickly mark the allocated space for later identification.
    memset(elems[num_elements + 1]+sizeof(char*), num_elements + 1, size-sizeof(char*));
    EXPECT_EQ(&(elems[num_elements + 1]), *reinterpret_cast<char***>(elems[num_elements + 1]));
    EXPECT_EQ(size * num_elements * 2, dut.bytesAllocated());
    dut.free(elems[num_elements + 1]);
    EXPECT_EQ(size * num_elements, dut.bytesAllocated());
}

TEST_F(CompactingPoolTest, bytes_allocated_test)
{
    int32_t size = 1024 * 512; // half a meg object
    int32_t num_elements = ((2 * 1024 * 1024) / size) + 1;

    // need to top 2GB to overflow
    int64_t bigsize = 2L * (1024L * 1024L * 1024L) + (1024L * 1024L * 10L);
    int64_t elems_needed = bigsize / size + 1;
    char* elems[elems_needed];

    CompactingStringPool dut(size, num_elements);
    for (int i = 0; i < elems_needed; ++i) {
        elems[i] = dut.alloc(&(elems[i]));
        // Quickly mark the allocated space for later identification,
        // except for the first 8 bytes of back-pointer.
        memset(elems[i]+sizeof(char*), i, size-sizeof(char*));
        // Validate the required back-pointer into elems.
        EXPECT_EQ(&(elems[i]), *reinterpret_cast<char***>(elems[i]));
        // return value of bytesAllocated() is unsigned.  However,
        // when it overflows internally, we get a HUGE value back.
        // Our sanity check is that the value is less than twice the
        // giant memory we're trying to fill
        EXPECT_TRUE(dut.bytesAllocated() < (bigsize * 2L));
    }
    // Ghetto way to get INT_MAX
    // Make sure that we would have, in fact, overflowed an int32_t
    EXPECT_TRUE(dut.bytesAllocated() > 0x7fffffff);

    for (int i = 0; i < elems_needed; ++i) {
        // bonus extra hack test.  If we keep freeing the first
        // element, it should get compacted into and we can free it
        // again!
        dut.free(elems[0]);
    }
}

int main() { return TestSuite::globalInstance()->runAll(); }
