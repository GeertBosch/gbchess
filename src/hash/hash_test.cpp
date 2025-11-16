#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>

#include "core/common.h"
#include "fen/fen.h"
#include "hash.h"
#include "move/move.h"

namespace {
std::string toHex(HashValue value) {
    using Limb = uint32_t;
    constexpr size_t limbCount = sizeof(HashValue) / sizeof(Limb);
    std::array<Limb, limbCount> limbs;
    for (auto it = limbs.rbegin(); it != limbs.rend(); ++it, value >>= 8 * sizeof(Limb))
        *it = static_cast<Limb>(value);

    std::ostringstream oss;
    for (auto limb : limbs)
        oss << std::hex << std::setw(sizeof(Limb) * 2) << std::setfill('0') << limb;
    return oss.str();
}

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
void findOneChange(HashValue diff) {
    for (int i = 0; i < kNumHashVectors; ++i)
        if (diff == hashVectors[i]) reportChanges({i});
}
void findTwoChanges(HashValue diff) {
    for (int i = 0; i < kNumHashVectors; ++i)
        for (int j = i + 1; j < kNumHashVectors; ++j)
            if (diff == (hashVectors[i] ^ hashVectors[j])) reportChanges({i, j});
}
void findThreeChanges(HashValue diff) {
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
        std::cout << "Hash 1: " << toHex(hash1()) << "\n";
        std::cout << "Hash 2: " << toHex(hash2()) << "\n";
        std::cout << "Diff:   " << toHex(diff) << "\n";

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

Position applyUCIMove(Position position, const std::string& move) {
    return moves::applyMove(position, fen::parseUCIMove(position.board, move));
    throw moves::MoveError("Invalid move: " + move);
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
    assert(pos1.turn.enPassant() == f6);
    auto hash1 = Hash(pos1);
    auto move = fen::parseUCIMove(pos1.board, "e5f6");
    assert(move.kind == MoveKind::En_Passant);

    auto pos2 = moves::applyMove(pos1, move);
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
    auto mv = fen::parseUCIMove(pos1.board, move);
    hash.applyMove(pos1, mv);
    auto pos2 = moves::applyMove(pos1, mv);
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

void testSameHash() {
    // Move count doesn't change hashes
    std::string fen1 = "rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR b KQkq - 0 1";
    std::string fen2 = "rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR b KQkq - 4 3";
    Position pos1 = fen::parsePosition(fen1);
    Position pos2 = fen::parsePosition(fen2);
    auto hash1 = Hash(pos1);
    auto hash2 = Hash(pos2);
    assert(hash1() == hash2());
}

void testPromotionKind() {
    Square from = a7;
    Square to = a8;
    Move move(from, to, MoveKind::Queen_Promotion);
    assert(isPromotion(move.kind));
    assert(promotionType(move.kind) == PieceType::QUEEN);
}
void printHashedArgument(int argc, char** argv) {
    std::string fen = argv[1];
    auto position = fen::parsePosition(fen);
    for (int i = 1; i < argc; ++i) {
        if (i > 1) position = applyUCIMove(position, argv[i]);
        auto hash = Hash(position);
        std::cout << toHex(hash()) << " " << fen::to_string(position) << "\n";
    }
    std::cout << "\n";
}
}  // namespace

int main(int argc, char** argv) {
    if (argc >= 2) printHashedArgument(argc, argv);
    testPromotionKind();
    testTrivialHash();
    testBasicHash();
    testSameHash();
    testToggleCastlingRights();
    testEnPassant();
    testHashApplyMove();
    std::cout << "All hash tests passed!" << std::endl;
    return 0;
}