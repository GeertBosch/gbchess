#pragma once

#include <string>
#include <vector>

namespace options {

/**
 * A self-registering UCI option. Stores an int value (bools are stored as 0/1).
 * Implicitly converts to int/bool so it can be used directly in search code.
 */
struct UCIOption {
    const char* const name;
    int value;
    const int defaultVal : 31;
    const bool isBool : 1;

    UCIOption(const char* name, int defaultVal)
        : name(name), value(defaultVal), defaultVal(defaultVal), isBool(false) {
        registry().push_back(this);
    }
    UCIOption(const char* name, bool defaultVal)
        : name(name), value(defaultVal), defaultVal(defaultVal), isBool(true) {
        registry().push_back(this);
    }

    operator int() const { return value; }

    bool isDefault() const { return value == defaultVal; }

    void set(const std::string& s) { value = isBool ? int(s == "true") : std::stoi(s); }

    static std::vector<UCIOption*>& registry() {
        static std::vector<UCIOption*> reg;
        return reg;
    }
};

/**
 * Search tuning options. All are runtime-mutable and exposed as UCI options.
 * Self-register into UCIOption::registry() on construction.
 */
inline UCIOption defaultDepth{"DefaultDepth", 15};
inline UCIOption iterativeDeepening{"IterativeDeepening", true};
inline UCIOption lateMoveReductions{"LateMoveReductions", true};
inline UCIOption staticExchangeEvaluation{"StaticExchangeEvaluation", true};
inline UCIOption maxKillerMoves{"MaxKillerMoves", 2};
inline UCIOption maxKillerDepth{"MaxKillerDepth", 256};
inline UCIOption historyHeuristic{"HistoryHeuristic", true};
inline UCIOption aspirationWindow1{"AspirationWindow1", 65};   // centipawns
inline UCIOption aspirationWindow2{"AspirationWindow2", 135};  // centipawns
inline UCIOption aspirationWindowMinDepth{"AspirationWindowMinDepth", 2};
inline UCIOption promotionMinDepthLeft{"PromotionMinDepthLeft", 3};
inline UCIOption checksMinDepthLeft{"ChecksMinDepthLeft", 5};  // == quiescenceDepth
inline UCIOption checksMaxPiecesLeft{"ChecksMaxPiecesLeft", 12};
inline UCIOption quiescenceDepth{"QuiescenceDepth", 5};
inline UCIOption currmoveMinDepthLeft{"CurrmoveMinDepthLeft", 1};
inline UCIOption transpositionTableMB{"Hash", 16};  // Zero means not enabled
inline UCIOption useNNUE{"UseNNUE", true};
inline UCIOption incrementalEvaluation{"IncrementalEvaluation", true};
inline UCIOption PVS{"PVS", true};
inline UCIOption reverseFutilityPruning{"ReverseFutilityPruning", true};
inline UCIOption reverseFutilityMaxDepthLeft{"ReverseFutilityMaxDepthLeft", 3};
inline UCIOption nullMovePruning{"NullMovePruning", true};
inline UCIOption nullMoveReduction{"NullMoveReduction", 3};
inline UCIOption nullMoveMinDepth{"NullMoveMinDepth", 2};
inline UCIOption useCountermove{"UseCountermove", true};
inline UCIOption useQsTT{"UseQsTT", true};
inline UCIOption rootPVSMaxDepthLeft{"RootPVSMaxDepthLeft", 3};
inline UCIOption qsFrontierInMainMaxDepthLeft{"QsFrontierInMainMaxDepthLeft", 1};
inline UCIOption moveLevelFutilityPruning{"MoveLevelFutilityPruning", true};
inline UCIOption moveFutilityMaxDepthLeft{"MoveFutilityMaxDepthLeft", 3};
inline UCIOption moveFutilityMinMoveCount{"MoveFutilityMinMoveCount", 2};
inline UCIOption moveFutilityMarginSlope{"MoveFutilityMarginSlope", 150};
inline UCIOption moveFutilityMarginBase{"MoveFutilityMarginBase", 240};
inline UCIOption internalIterativeDeepening{"InternalIterativeDeepening", true};
inline UCIOption iidMinDepthLeft{"IidMinDepthLeft", 4};
inline UCIOption iidDepthReduction{"IidDepthReduction", 2};
inline UCIOption nodestime{"nodestime", 250};  // nodes/ms, like Stockfish; 0 = disabled

/**
 * Caching options for the perft program.
 */
inline UCIOption cachePerft{"CachePerft", true};
inline UCIOption cachePerftMB{"CachePerftMB", 2 * 1024};
inline UCIOption cachePerftMinNodes{"CachePerftMinNodes", 100};
inline UCIOption perftProgressMillis{"PerftProgressMillis", 100};

};  // namespace options
