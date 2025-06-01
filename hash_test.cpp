#include <iomanip>
#include <iostream>

#include "common.h"
#include "fen.h"
#include "hash.h"
#include "moves.h"

namespace {
using changes = std::vector<int>;
std::string hashVectorName(int i) {
    switch (i) {
    case Hash::BLACK_TO_MOVE: return "BLACK_TO_MOVE";
    case Hash::CASTLING_0: return "CASTLING_0";
    case Hash::CASTLING_1: return "CASTLING_1";
    case Hash::CASTLING_2: return "CASTLING_2";
    case Hash::CASTLING_3: return "CASTLING_3";
    case Hash::EN_PASSANT_A: return "EN_PASSANT_A";
    case Hash::EN_PASSANT_B: return "EN_PASSANT_B";
    case Hash::EN_PASSANT_C: return "EN_PASSANT_C";
    case Hash::EN_PASSANT_D: return "EN_PASSANT_D";
    case Hash::EN_PASSANT_E: return "EN_PASSANT_E";
    case Hash::EN_PASSANT_F: return "EN_PASSANT_F";
    case Hash::EN_PASSANT_G: return "EN_PASSANT_G";
    case Hash::EN_PASSANT_H: return "EN_PASSANT_H";
    default: {
    }
    }
    int pieceIndex = i / kNumSquares;
    int squareIndex = i % kNumSquares;
    assert(pieceIndex < kNumPieces);
    assert(squareIndex < kNumSquares);
    auto piece = Piece(pieceIndex);
    auto square = Square(squareIndex);
    std::string vectorName;
    vectorName += to_char(piece);
    vectorName += "@";
    vectorName += to_string(square);
    return vectorName;
}
void reportChanges(const changes& changed) {
    if (changed.empty()) return;
    std::cout << "Hash vectors changed:";
    for (auto i : changed) {
        std::cout << " " << i << ":" << hashVectorName(i);
    }
    std::cout << "\n";
}
void findOneChange(uint64_t diff) {
    for (int i = 0; i < kNumHashVectors; ++i)
        if (diff == hashVectors[i]) reportChanges({i});
}
void findTwoChanges(uint64_t diff) {
    for (int i = 0; i < kNumHashVectors; ++i)
        for (int j = i + 1; j < kNumHashVectors; ++j)
            if (diff == (hashVectors[i] ^ hashVectors[j])) reportChanges({i, j});
}
void findThreeChanges(uint64_t diff) {
    for (int i = 0; i < kNumHashVectors; ++i)
        for (int j = i + 1; j < kNumHashVectors; ++j)
            for (int k = j + 1; k < kNumHashVectors; ++k)
                if (diff == (hashVectors[i] ^ hashVectors[j] ^ hashVectors[k]))
                    reportChanges({i, j, k});
}

bool checkSameHash(Hash hash1, Hash hash2) {
    if (auto diff = hash1() ^ hash2()) {
        // Print both hashes and their difference using 16 hex digits each.
        std::cout << "Hash mismatch:\n";
        std::cout << std::hex;
        std::cout << "Hash 1: " << std::setw(16) << std::setfill('0') << hash1() << "\n";
        std::cout << "Hash 2: " << std::setw(16) << std::setfill('0') << hash2() << "\n";
        std::cout << "Diff:   " << std::setw(16) << std::setfill('0') << diff << "\n";
        std::cout << std::dec;

        findOneChange(diff);
        findTwoChanges(diff);
        findThreeChanges(diff);
    }
    return hash1 == hash2;
}

void testTrivialHash() {
    Hash hash1;
    Hash hash2;
    assert(hash1 == hash2);
    assert(hash1() == hash2());
}

void testBasicHash() {
    auto startPos = fen::parsePosition(fen::initialPosition);
    auto e2e3 = applyUCIMove(startPos, "e2e3");
    auto e2e3e7e5 = applyUCIMove(e2e3, "e7e5");
    auto e2e3e7e5g1f3 = applyUCIMove(e2e3e7e5, "g1f3");
    auto g1f3 = applyUCIMove(startPos, "g1f3");
    auto g1f3e7e5 = applyUCIMove(g1f3, "e7e5");
    auto g1f3e7e5e2e3 = applyUCIMove(g1f3e7e5, "e2e3");  // Same as e2e3e7e5g1f3

    {
        Hash hash1(startPos);
        Hash hash2(e2e3);
        assert(hash1() != hash2());
    }
    {
        // Different paths leading to the same position should yield the same hash
        Hash hash1(e2e3e7e5g1f3);
        Hash hash2(g1f3e7e5e2e3);
        checkSameHash(hash1, hash2);
    }
}

void testToggleCastlingRights() {
    auto pos1 = fen::parsePosition("k7/p1p1p1p1/P1P1P1P1/8/8/8/8/R3K2R w KQ - 0 1");
    auto pos2 = fen::parsePosition("k7/p1p1p1p1/P1P1P1P1/8/8/8/8/R3K2R w - - 0 1");
    auto hash1 = Hash(pos1);
    auto hash2 = Hash(pos2);
    Hash hash = hash1;
    auto canceled = CastlingMask::K | CastlingMask::Q;
    hash.toggle(canceled);
    assert(hash1() != hash2());
    assert(checkSameHash(hash, hash2));
}

void testEnPassant() {
    auto pos1 =
        fen::parsePosition("r3k2r/pb1p2pp/1b4q1/1Q2Pp2/8/2NP1PP1/PP4P1/R1B2R1K w kq f6 0 18");
    assert(pos1.turn.enPassant() == "f6"_sq);
    auto hash1 = Hash(pos1);
    auto move = parseUCIMove(pos1, "e5f6");
    assert(move.kind == MoveKind::En_Passant);

    auto pos2 = applyMove(pos1, move);
    assert(pos2.turn.enPassant() == noEnPassantTarget);
    auto hash2 = Hash(pos2);
    assert(hash1() != hash2());
    auto hash = hash1;
    hash.applyMove(pos1, move);
    assert(checkSameHash(hash, hash2));
}

void checkApplyMove(std::string position, std::string move) {
    auto pos1 = fen::parsePosition(position);
    Hash hash(pos1);
    auto mv = parseUCIMove(pos1, move);
    hash.applyMove(pos1, mv);
    auto pos2 = applyMove(pos1, mv);
    bool ok = checkSameHash(hash, Hash(pos2));
    if (!ok) std::cout << "Failed to apply move " << move << " on position " << position << "\n";
    assert(ok);
}

void testHashApplyMove() {
    // Simple quiet move, no en passant
    checkApplyMove(fen::initialPosition, "e2e3");
    // Simple capture
    checkApplyMove("4k3/8/8/8/8/8/8/4Kr2 w - - 0 1", "e1f1");
    // King side castling
    checkApplyMove("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1g1");
    // Queen side castling
    checkApplyMove("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1c1");
}

void testPromotionKind() {
    {
        Square from = "a7"_sq;
        Square to = "a8"_sq;
        Move move(from, to, MoveKind::Queen_Promotion);
        assert(move.isPromotion());
        assert(promotionType(move.kind) == PieceType::QUEEN);
    }
}
}  // namespace

int main() {
    testPromotionKind();
    testTrivialHash();
    testBasicHash();
    testToggleCastlingRights();
    testEnPassant();
    testHashApplyMove();
    std::cout << "All hash tests passed!" << std::endl;
    return 0;
}