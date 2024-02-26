#include <iomanip>
#include <iostream>

#include "common.h"

#pragma once

#ifdef __SSE2__
#include <emmintrin.h>
inline std::ostream& operator<<(std::ostream& os, __m128i x) {
    os << "0x" << std::hex << std::setfill('0') << std::setw(16) << x[1] << "'" << std::setw(16)
       << x[0] << std::dec << std::setfill(' ');
    return os;
}
#endif

inline void printBoard(std::ostream& os, const Board& board) {
    for (int rank = 7; rank >= 0; --rank) {
        os << rank + 1 << "  ";
        for (int file = 0; file < 8; ++file) {
            auto piece = board[Square(rank, file)];
            os << ' ' << to_char(piece);
        }
        os << std::endl;
    }
    os << "   ";
    for (char file = 'a'; file <= 'h'; ++file) {
        os << ' ' << file;
    }
    os << std::endl;
}

inline bool expectEqual(const Board& left, const Board& right, const std::string& message = "") {
    bool equal = left == right;
    if (!equal) {
        std::cerr << message << std::endl;
        std::cerr << "Expected:" << std::endl;
        printBoard(std::cerr, left);
        std::cerr << "Actual:" << std::endl;
        printBoard(std::cerr, right);
        assert(false);
    }
    return equal;
}
