#pragma once

#include <istream>
#include <string>
#include <vector>

namespace pgn {
struct PGN {
    using TagPair = std::pair<std::string, std::string>;
    using Tags = std::vector<TagPair>;
    Tags tags;
    std::string movetext;

    /** Returns true iff the PGN object contains a non-empty movetext. */
    operator bool() const { return !movetext.empty(); }

    /** Returns the value of the specified tag, or an empty string if not found. */
    std::string operator[](const std::string& tag) const {
        for (auto&& t : tags)
            if (t.first == tag) return t.second;
        return {};
    }
    // 
};

/**
 * Reads a PGN string from an input stream and returns a PGN object.
 * Multiple games may be in the same stream; this function reads only one game.
 */
PGN readPGN(std::istream& in);

/**
 * Extracts just the SAN moves from PGN movetext, filtering out comments, annotations,
 * move numbers, NAGs, and variations. Returns a string containing only the moves and
 * the game termination marker.
 */
std::string extractSANMoves(const std::string& movetext);

}  // namespace pgn