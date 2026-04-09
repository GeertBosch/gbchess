#include <cassert>
#include <iostream>

#include "move_table.h"

void testPossibleMoves() {
    // Test rook moves
    assert((MovesTable::possibleMoves(Piece::R, a1) ==
            SquareSet{b1, c1, d1, e1, f1, g1, h1, a2, a3, a4, a5, a6, a7, a8}));

    // Test bishop moves
    assert((MovesTable::possibleMoves(Piece::b, a1) == SquareSet{b2, c3, d4, e5, f6, g7, h8}));

    // Test knight moves
    assert((MovesTable::possibleMoves(Piece::N, a1) == SquareSet{b3, c2}));

    // Test king moves
    assert((MovesTable::possibleMoves(Piece::k, a1) == SquareSet{a2, b1, b2}));

    // Test queen moves
    assert((MovesTable::possibleMoves(Piece::Q, a1) == SquareSet{b1, c1, d1, e1, f1, g1, h1,
                                                                 a2, a3, a4, a5, a6, a7, a8,
                                                                 b2, c3, d4, e5, f6, g7, h8}));

    // Test white pawn moves
    assert((MovesTable::possibleMoves(Piece::P, a2) == SquareSet{a3, a4}));

    // Test black pawn moves
    assert((MovesTable::possibleMoves(Piece::p, a7) == SquareSet{a5, a6}));

    std::cout << "All possibleMoves tests passed!" << std::endl;
}

void testPossibleCaptures() {
    // Knight at e5: all 8 knight moves
    assert(
        (MovesTable::possibleCaptures(Piece::N, e5) == SquareSet{d3, f3, d7, f7, c4, g4, c6, g6}));

    // Knight at h8: edge case, only 2 moves
    assert((MovesTable::possibleCaptures(Piece::n, h8) == SquareSet{g6, f7}));

    // Bishop at e5: all 4 diagonals
    assert((MovesTable::possibleCaptures(Piece::B, e5) ==
            SquareSet{a1, b2, c3, d4, d6, c7, b8, f4, g3, h2, f6, g7, h8}));

    // Rook at e5: full rank and file
    assert((MovesTable::possibleCaptures(Piece::r, e5) ==
            SquareSet{a5, b5, c5, d5, f5, g5, h5, e1, e2, e3, e4, e6, e7, e8}));

    // Queen at e5: bishop + rook combined
    assert((MovesTable::possibleCaptures(Piece::Q, e5) ==
            SquareSet{a1, b2, c3, d4, d6, c7, b8, f4, g3, h2, f6, g7, h8, a5,
                      b5, c5, d5, f5, g5, h5, e1, e2, e3, e4, e6, e7, e8}));

    // King at e5: all 8 surrounding squares
    assert(
        (MovesTable::possibleCaptures(Piece::k, e5) == SquareSet{d4, e4, f4, d5, f5, d6, e6, f6}));

    // White pawn at e5: diagonal captures only
    assert((MovesTable::possibleCaptures(Piece::P, e5) == SquareSet{d6, f6}));

    // Black pawn at e5: diagonal captures only
    assert((MovesTable::possibleCaptures(Piece::p, e5) == SquareSet{d4, f4}));

    std::cout << "All possibleCaptures tests passed!" << std::endl;
}

int main() {
    testPossibleMoves();
    testPossibleCaptures();
    std::cout << "All moves_table tests passed!" << std::endl;
    return 0;
}