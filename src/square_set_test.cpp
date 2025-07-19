/**
 * Test suite for SquareSet data structure.
 *
 * This file contains isolated tests for the SquareSet class, which provides
 * an efficient bitset-based representation of chess squares. The tests are
 * separated from moves_test.cpp to facilitate future Rust migration.
 *
 * Tests include:
 * - Basic operations (empty, size, insert, contains)
 * - Iterator functionality
 * - Range operations (insert range)
 * - Edge cases and correctness
 */

#include <cassert>
#include <iostream>

#include "common.h"
#include "square_set.h"

void testSquareSet() {
    SquareSet set;

    // Test empty set
    assert(set.empty());
    assert(set.size() == 0);

    // Test insert and count
    set.insert(Square(0));
    assert(!set.empty());
    assert(set.size() == 1);
    assert(set.contains(Square(0)));
    assert(!set.contains(Square(1)));

    // Test insert and count with multiple squares
    set.insert(Square(1));
    set.insert(Square(4));
    assert(set.size() == 3);
    assert(set.contains(Square(0)));
    assert(set.contains(Square(1)));
    assert(!set.contains(Square(2)));
    assert(!set.contains(Square(3)));
    assert(set.contains(Square(4)));

    // Test iterator
    set.insert(Square(9));
    SquareSet::iterator it = set.begin();
    assert(*it == Square(0));
    ++it;
    assert(*it == Square(1));
    ++it;
    assert(*it == Square(4));
    ++it;
    assert(it != set.end());
    assert(*it == Square(9));
    ++it;
    assert(it == set.end());

    // Test end iterator
    it = set.end();
    assert(it == set.end());

    // Test copy constructor
    SquareSet set2;
    set2.insert(Square(3));
    set2.insert(Square(4));
    set2.insert(Square(5));
    set.insert(set2.begin(), set2.end());
    assert(set.size() == 6);
    assert(set.contains(Square(3)));
    assert(set.contains(Square(4)));
    assert(set.contains(Square(5)));

    std::cout << "All SquareSet tests passed!" << std::endl;
}

int main() {
    testSquareSet();
    return 0;
}
