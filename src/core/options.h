#pragma once

#include <string>
#include <vector>

namespace options {

/**
 * Registry entry for a UCIOption. Holds static metadata and a pointer to the
 * live option. The UCI layer uses this to enumerate, display, and set options
 * without touching search-hot memory.
 */
struct UCIOption;
struct UCIOptionInfo {
    const char* const name;
    UCIOption* const opt;
    const int defaultVal;
    const int minVal;
    const int maxVal;
    const bool isBool;

    bool isDefault() const;
    void set(const std::string& s) const;

    static std::vector<UCIOptionInfo>& registry() {
        static std::vector<UCIOptionInfo> reg;
        return reg;
    }
};

/**
 * A self-registering UCI option. Stores only the current int value (bools are
 * stored as 0/1). Implicitly converts to int/bool so it can be used directly
 * in search code. Static metadata (name, defaultVal, minVal, maxVal, isBool)
 * lives in the paired UCIOptionInfo in the registry.
 */
struct UCIOption {
    int value;

    UCIOption(const char* name, int defaultVal, int minVal, int maxVal) : value(defaultVal) {
        UCIOptionInfo::registry().push_back({name, this, defaultVal, minVal, maxVal, false});
    }
    UCIOption(const char* name, bool defaultVal) : value(int(defaultVal)) {
        UCIOptionInfo::registry().push_back({name, this, int(defaultVal), 0, 1, true});
    }

    operator int() const { return value; }
};

inline bool UCIOptionInfo::isDefault() const {
    return opt->value == defaultVal;
}
inline void UCIOptionInfo::set(const std::string& s) const {
    opt->value = isBool ? int(s == "true") : std::stoi(s);
}

/**
 * Search tuning options. All are runtime-mutable and exposed as UCI options.
 * Self-register into UCIOptionInfo::registry() on construction.
 */
//                                                           default  min    max
inline UCIOption defaultDepth{"DefaultDepth", 15, 1, 100};
inline UCIOption iterativeDeepening{"IterativeDeepening", true};
inline UCIOption lateMoveReductions{"LateMoveReductions", true};
inline UCIOption staticExchangeEvaluation{"StaticExchangeEvaluation", true};
inline UCIOption maxKillerMoves{"MaxKillerMoves", 2, 0, 8};
inline UCIOption maxKillerDepth{"MaxKillerDepth", 256, 1, 1024};
inline UCIOption historyHeuristic{"HistoryHeuristic", true};
inline UCIOption aspirationWindow1{"AspirationWindow1", 65, 1, 1000};   // centipawns
inline UCIOption aspirationWindow2{"AspirationWindow2", 135, 1, 1000};  // centipawns
inline UCIOption aspirationWindowMinDepth{"AspirationWindowMinDepth", 2, 1, 20};
inline UCIOption promotionMinDepthLeft{"PromotionMinDepthLeft", 1, 0, 20};
inline UCIOption checksMinDepthLeft{"ChecksMinDepthLeft", 1, 0, 20};  // == quiescenceDepth
inline UCIOption checksMaxPiecesLeft{"ChecksMaxPiecesLeft", 12, 2, 32};
inline UCIOption quiescenceDepth{"QuiescenceDepth", 3, 0, 30};
inline UCIOption currmoveMinDepthLeft{"CurrmoveMinDepthLeft", 1, 0, 20};
inline UCIOption transpositionTableMB{"Hash", 16, 0, 65536};  // Zero means not enabled
inline UCIOption useNNUE{"UseNNUE", true};
inline UCIOption incrementalEvaluation{"IncrementalEvaluation", true};
inline UCIOption PVS{"PVS", true};
inline UCIOption reverseFutilityPruning{"ReverseFutilityPruning", true};
inline UCIOption reverseFutilityMaxDepthLeft{"ReverseFutilityMaxDepthLeft", 3, 0, 20};
inline UCIOption nullMovePruning{"NullMovePruning", true};
inline UCIOption nullMoveReduction{"NullMoveReduction", 3, 1, 10};
inline UCIOption nullMoveMinDepth{"NullMoveMinDepth", 2, 1, 20};
inline UCIOption useCountermove{"UseCountermove", true};
inline UCIOption useQsTT{"UseQsTT", true};
inline UCIOption rootPVSMaxDepthLeft{"RootPVSMaxDepthLeft", 3, 0, 20};
inline UCIOption qsFrontierInMainMaxDepthLeft{"QsFrontierInMainMaxDepthLeft", 1, 0, 10};
inline UCIOption moveLevelFutilityPruning{"MoveLevelFutilityPruning", true};
inline UCIOption moveFutilityMaxDepthLeft{"MoveFutilityMaxDepthLeft", 3, 0, 20};
inline UCIOption moveFutilityMinMoveCount{"MoveFutilityMinMoveCount", 2, 1, 20};
inline UCIOption moveFutilityMarginSlope{"MoveFutilityMarginSlope", 150, 0, 1000};
inline UCIOption moveFutilityMarginBase{"MoveFutilityMarginBase", 240, 0, 2000};
inline UCIOption internalIterativeDeepening{"InternalIterativeDeepening", true};
inline UCIOption iidMinDepthLeft{"IidMinDepthLeft", 4, 1, 20};
inline UCIOption iidDepthReduction{"IidDepthReduction", 2, 1, 10};
inline UCIOption nodestime{"nodestime", 250, 0, 100000};  // nodes/ms; 0 = disabled
inline UCIOption softTimecheckPct{"SoftTimecheckPct", 70, 1, 100};  // Root node time check pct

/**
 * Caching options for the perft program.
 */
inline UCIOption cachePerft{"CachePerft", true};
inline UCIOption cachePerftMB{"CachePerftMB", 2048, 1, 65536};
inline UCIOption cachePerftMinNodes{"CachePerftMinNodes", 100, 0, 1000000};
inline UCIOption perftProgressMillis{"PerftProgressMillis", 100, 0, 60000};

};  // namespace options
