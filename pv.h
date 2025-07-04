#pragma once

#include "common.h"
#include "eval.h"

/**
 * Structure to store the principle variation of a chess game. This structure is used to store
 * the best sequence of moves found by the search algorithm.
 */
struct PrincipalVariation {
    Score score = Score::min();
    MoveVector moves;

    PrincipalVariation() = default;
    PrincipalVariation(Move move, Score score) : score(score) {
        if (move) moves.push_back(move);
    }
    PrincipalVariation(Move move, PrincipalVariation pv) : score(pv.score) {
        if (move) moves.push_back(move);
        moves.insert(moves.end(), pv.moves.begin(), pv.moves.end());
    }
    PrincipalVariation(const PrincipalVariation&) = default;
    PrincipalVariation& operator=(const PrincipalVariation& other) = default;
    Move front() const { return moves.empty() ? Move() : moves.front(); }
    Move operator[](size_t i) const { return i < moves.size() ? moves[i] : Move(); }

    explicit operator MoveVector() const { return moves; }
    PrincipalVariation operator-() const { return {-score, moves}; }
    bool operator<(const PrincipalVariation& other) const {
        return score < other.score || (score == other.score && moves.size() > other.moves.size());
    }
    bool operator>(const PrincipalVariation& other) const { return other < *this; }

    operator bool() { return !moves.empty(); }

private:
    PrincipalVariation(Score score, MoveVector moves) : score(score), moves(moves) {}
};

inline std::string to_string(const PrincipalVariation& pv) {
    auto& [score, moves] = pv;
    std::string str;
    auto depth = std::to_string(std::max(1, int(moves.size())));
    if (score.mate() > 0)
        str += "mate " + depth + " ";
    else if (score.mate() < 0)
        str += "mate -" + depth + " ";
    else
        str += "cp " + std::to_string(score.cp()) + " ";

    if (moves.size()) str += "pv ";

    // While adding moves the string remains ending with a space
    for (const auto& move : moves)
        if (move) str += to_string(move) + " ";

    str.pop_back();  // Remove the final space
    return str;
}
