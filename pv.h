
#include "common.h"
#include "eval.h"

#pragma once

/**
 * Structure to store the principle variation of a chess game. This structure is used to store
 * the best sequence of moves found by the search algorithm.
 */
struct PrincipalVariation {
    Score evaluation = worstEval;
    MoveVector moves;

    PrincipalVariation() = default;
    PrincipalVariation(Move move, Score score) : evaluation(score) {
        if (move) moves.push_back(move);
    }
    PrincipalVariation(Move move, PrincipalVariation pv) : evaluation(pv.evaluation) {
        if (move) moves.push_back(move);
        moves.insert(moves.end(), pv.moves.begin(), pv.moves.end());
    }
    PrincipalVariation(Eval eval) : evaluation(eval.evaluation) {
        if (eval.move) moves.push_back(eval.move);
    }
    PrincipalVariation& operator=(const PrincipalVariation& other) = default;
    Move front() const { return moves.empty() ? Move() : moves.front(); }

    operator MoveVector() const { return moves; }
    operator Eval() const { return {front(), evaluation}; }
    PrincipalVariation operator-() const { return {-evaluation, moves}; }

private:
    PrincipalVariation(Score score, MoveVector moves) : evaluation(score), moves(moves) {}
};