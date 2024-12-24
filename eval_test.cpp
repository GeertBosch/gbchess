#include <cstdlib>  // For std::exit
#include <iostream>
#include <string>

#include "common.h"
#include "eval.h"
#include "fen.h"
#include "moves.h"

namespace {
std::ostream& operator<<(std::ostream& os, const MoveVector& moves) {
    os << "[";
    for (const auto& move : moves) {
        if (move) os << std::string(move) << ", ";
    }
    os << "]";
    return os;
}
std::ostream& operator<<(std::ostream& os, Move mv) {
    return os << std::string(mv);
}
std::ostream& operator<<(std::ostream& os, Square sq) {
    return os << std::string(sq);
}
std::ostream& operator<<(std::ostream& os, Color color) {
    return os << (color == Color::BLACK ? 'b' : 'w');
}
std::ostream& operator<<(std::ostream& os, Score score) {
    return os << std::string(score);
}
std::ostream& operator<<(std::ostream& os, const Eval& eval) {
    return os << eval.move << " " << eval.evaluation;
}

std::string cmdName = "eval-test";

using GridDrawing = std::array<std::string, 11>;
GridDrawing gridDrawing = {
    " ┌─",
    "─",
    "─┬─",
    "─┐ ",
    " │ ",
    " ├─",
    "─┼─",
    "─┤ ",
    " └─",
    "─┴─",
    "─┘ ",
};

std::string operator*(std::string str, int n) {
    if (!n) return "";
    return n == 1 ? str : str * (n / 2) + str * (n - n / 2);
}

std::string gridTop(int width) {
    std::string res;
    res += gridDrawing[0];
    res += (gridDrawing[1] + gridDrawing[2]) * (width - 1);
    res += gridDrawing[1] + gridDrawing[3];
    return res;
}
std::string gridRow(std::string row) {
    std::string res;
    res += gridDrawing[4];
    for (int i = 0; i < row.size(); ++i) {
        res += row[i];
        if (i < row.size() - 1) res += gridDrawing[4];
    }
    res += gridDrawing[4];
    return res;
}
std::string gridMiddle(int width) {
    std::string res;
    res += gridDrawing[5];
    res += (gridDrawing[1] + gridDrawing[6]) * (width - 1);
    res += gridDrawing[1] + gridDrawing[7];
    return res;
}
std::string gridBottom(int width) {
    std::string res;
    res += gridDrawing[8];
    res += (gridDrawing[1] + gridDrawing[9]) * (width - 1);
    res += gridDrawing[1] + gridDrawing[10];
    return res;
}

void usage(std::string cmdName, std::string errmsg) {
    std::cerr << "Error: " << errmsg << "\n\n";
    std::cerr << "Usage: " << cmdName << " <FEN-string> [move...]" << std::endl;
    std::exit(1);
}
}  // namespace

void printAvailableMoves(const Position& position) {
    MoveVector moves;
    addAvailableMoves(moves, position.board, position.activeColor());
    std::cout << "Moves: " << moves << std::endl;
}

void printAvailableCaptures(const Position& position) {
    MoveVector captures;
    addAvailableCaptures(captures, position.board, position.activeColor());
    std::cout << "Captures: " << captures << std::endl;
}

/**
 * Prints the chess board to the specified output stream in grid notation.
 */
void printBoard(std::ostream& os, const Board& board) {
    os << " " << gridTop(kNumFiles) << std::endl;
    for (int rank = kNumRanks - 1; rank >= 0; --rank) {
        os << rank + 1;
        std::string row;
        for (int file = 0; file < kNumFiles; ++file) {
            auto piece = board[Square(rank, file)];
            row.push_back(to_char(piece));
        }
        os << gridRow(row) << std::endl;
        if (rank > 0) os << " " << gridMiddle(kNumFiles) << std::endl;
    }
    os << " " << gridBottom(kNumFiles) << std::endl;
    os << " ";
    for (char file = 'a'; file <= 'h'; ++file) {
        os << "   " << file;
    }
    os << std::endl;
}

std::vector<std::string> split(std::string line, char delim) {
    std::vector<std::string> res;
    std::string word;
    for (auto c : line) {
        if (c == delim) {
            res.emplace_back(std::move(word));
            word = "";
        } else {
            word.push_back(c);
        }
    }
    if (word.size()) res.emplace_back(std::move(word));
    return res;
}

void testScore() {
    Score zero;
    Score q = -9'00_cp;
    Score Q = 9'00_cp;
    assert(Q == -q);
    assert(Q + q == zero);
    assert(q < Q);
    assert(std::string(q) == "-9.00");
    assert(std::string(Q) == "9.00");
    Score f = -1'23_cp;
    Score F = 1'23_cp;
    assert(F == -f);
    assert(F + f == zero);
    assert(std::string(f) == "-1.23");
    assert(std::string(F) == "1.23");
    std::cout << "Score tests passed" << std::endl;
}

void testMateScore() {
    Score M1 = bestEval;
    assert(std::string(M1) == "M1");
    assert(M1.mate() == 1);

    Score m1 = -M1;
    assert(m1.mate() == -1);
    assert(m1 == worstEval);
    assert(std::string(m1) == "-M1");

    Score M2 = M1.adjustDepth();
    Score m2 = -((-m1).adjustDepth());

    assert(M2.mate() == 2);
    assert(m2.mate() == -2);
    assert(M2 == -m2);
    assert(std::string(M2) == "M2");
    assert(std::string(m2) == "-M2");
    std::cout << "Mate score tests passed" << std::endl;
}

void testEvaluateBoard() {
    std::string piecePlacement = "8/8/8/8/4p3/5pNN/4p3/2K1k3";
    auto board = fen::parsePiecePlacement(piecePlacement);
    auto score = evaluateBoard(board, false);
    assert(score == 300_cp);  // 2 knights vs 3 pawns
    board = fen::parsePiecePlacement(fen::initialPiecePlacement);
    score = evaluateBoard(board, true);
    assert(score == 0_cp);
    std::cout << "evaluateBoard tests passed" << std::endl;
}

void testEval() {
    {
        Eval none;
        Eval mateIn3 = {Move("f6"_sq, "e5"_sq, Move::QUIET), bestEval.adjustDepth().adjustDepth()};
        Eval mateIn1 = {Move("e7"_sq, "g7"_sq, Move::QUIET), bestEval};
        assert(std::string(none) == "0000@-M1");
        assert(std::string(mateIn3) == "f6e5@M3");
        assert(std::string(mateIn1) == "e7g7@M1");
        assert(none < mateIn3);
        assert(mateIn3 < mateIn1);
        assert(none < mateIn1);
        assert(mateIn1.evaluation == bestEval);
        assert(bestEval == Score::max());
    }
    {
        Eval stalemate = {Move("f6"_sq, "e5"_sq, Move::QUIET), 0_cp};
        Eval upQueen = {Move("f7"_sq, "a2"_sq, Move::CAPTURE), 9'00_cp};
        assert(stalemate < upQueen);
        assert(stalemate.evaluation == drawEval);
    }
    std::cout << "Eval tests passed" << std::endl;
}

int main(int argc, char* argv[]) {
    cmdName = argv[0];

    // Need at least one argument
    if (argc < 2) usage(argv[0], "missing argument");

    testScore();
    testMateScore();
    testEvaluateBoard();
    testEval();

    std::string fen(argv[1]);

    // Parse the FEN string into a Position
    Position position = fen::parsePosition(fen);

    MoveVector moves;
    for (int j = 2; j < argc; ++j) {
        moves.push_back(parseMoveUCI(position, argv[j]));
        if (!moves.back()) usage(argv[0], std::string(argv[j]) + " is not a valid move");
        position = applyMove(position, moves.back());
    }

    if (moves.size()) std::cout << "New position: " << fen::to_string(position) << "\n";

    // Print the board in grid notation
    printBoard(std::cout, position.board);

    // Evaluate the board
    Score simpleEval = evaluateBoard(position.board, false);
    std::cout << "Simple Board Evaluation: " << std::string(simpleEval) << std::endl;

    Score pieceSquareEval = evaluateBoard(position.board, true);
    std::cout << "Piece-Square Board Evaluation: " << std::string(pieceSquareEval) << std::endl;

    printAvailableCaptures(position);
    printAvailableMoves(position);

    return 0;
}
