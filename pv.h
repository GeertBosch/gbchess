
#include "common.h"
#include "eval.h"

#pragma once

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
    PrincipalVariation(Eval eval) : score(eval.score) {
        if (eval.move) moves.push_back(eval.move);
    }
    PrincipalVariation& operator=(const PrincipalVariation& other) = default;
    Move front() const { return moves.empty() ? Move() : moves.front(); }
    Move operator[](size_t i) const { return i < moves.size() ? moves[i] : Move(); }

    operator MoveVector() const { return moves; }
    operator Eval() const { return {front(), score}; }
    PrincipalVariation operator-() const { return {-score, moves}; }

private:
    PrincipalVariation(Score score, MoveVector moves) : score(score), moves(moves) {}
};