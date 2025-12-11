#include "core/core.h"
#include <string>
#include <string_view>


#include "pgn.h"

namespace pgn {
namespace {
bool hasQuotes(const std::string& str) {
    return str.size() >= 2 && str.front() == '"' && str.back() == '"';
}

std::string removeQuotes(const std::string& str) {
    return hasQuotes(str) ? str.substr(1, str.size() - 2) : str;
}

/**
 * Parses PGN tags from the input stream and populates the provided tags vector. Returns true if at
 * least one tag was successfully parsed; false otherwise. A malformed tag line will stop parsing
 * further tags, add an error tag, and return false.
 */
bool readTags(std::istream& in, PGN::Tags& tags) {
    std::string line;

    // Skip leading blank lines
    while (std::getline(in, line) && line.empty());

    // Check if we hit EOF while skipping blank lines
    if (!in) return false;

    // Process tag lines
    do {
        if (line[0] != '[' || line.back() != ']') break;  // Stop at non-tag line

        // Find the first space separating tag name and value
        auto spacePos = line.find(' ');
        if (spacePos == std::string::npos) continue;  // Skip malformed tags

        line.pop_back();  // Remove trailing ']'
        std::string tagName = line.substr(1, spacePos - 1);
        std::string tagValue = removeQuotes(line.substr(spacePos + 1));
        tags.emplace_back(tagName, tagValue);
    } while (std::getline(in, line) && !line.empty());

    if (!line.empty()) tags.emplace_back("Error", "Malformed tag line: " + line);

    return line.empty() && !tags.empty();
}

using sv_iterator = std::string_view::const_iterator;

sv_iterator skipWhiteSpace(sv_iterator it, sv_iterator end) {
    while (it != end && std::isspace(*it)) ++it;
    return it;
}

sv_iterator skipMoveNumber(sv_iterator it, sv_iterator end) {
    it = skipWhiteSpace(it, end);
    auto beforeDigits = it;
    while (it != end && std::isdigit(*it)) ++it;
    it = skipWhiteSpace(it, end);
    // Don't consume digits if not followed by '.' or '...' as that may be the result like "0-1".
    if (it == end || *it != '.') return beforeDigits;
    ++it;
    if (it != end && *it == '.' && it + 1 != end && *(it + 1) == '.') it += 2;  // handle 1...
    return it;
}

sv_iterator skipNAG(sv_iterator it, sv_iterator end) {
    it = skipWhiteSpace(it, end);
    if (it == end || *it != '$') return it;
    ++it;
    while (it != end && std::isdigit(*it)) ++it;
    return it;
}

sv_iterator skipBracedComment(sv_iterator it, sv_iterator end) {
    it = skipWhiteSpace(it, end);
    if (it == end || *it != '{') return it;
    ++it;
    while (it != end && *it != '}') ++it;
    if (it != end) ++it;  // skip closing brace
    return it;
}

sv_iterator skipSemicolonComment(sv_iterator it, sv_iterator end) {
    it = skipWhiteSpace(it, end);
    if (it == end || *it != ';') return it;
    while (it != end && *it != '\n') ++it;
    return it;
}

sv_iterator skipVariations(sv_iterator it, sv_iterator end) {
    it = skipWhiteSpace(it, end);
    if (it == end || *it != '(') return it;

    int depth = 1;
    ++it;
    while (it != end && depth > 0) {
        if (*it == '(')
            ++depth;
        else if (*it == ')')
            --depth;
        ++it;
    }
    return it;
}

sv_iterator skipToSANmove(sv_iterator it, sv_iterator end) {
    sv_iterator prevIt;
    do {
        prevIt = it;
        it = skipNAG(it, end);
        it = skipBracedComment(it, end);
        it = skipSemicolonComment(it, end);
        it = skipVariations(it, end);
        it = skipWhiteSpace(it, end);
    } while (it != prevIt && it != end);
    return it;
}

// Pieces, captures, promos, castles, checks, mates and annotations
std::string extraSANchars = "KQRBNx=O-+#?!";

template <int Files, int Ranks>
constexpr auto makeSquareChars() {
    std::array<char, Files + Ranks + sizeof(extraSANchars) - 1> chars = {};
    int index = 0;
    for (char c = 'a'; c < 'a' + Files; ++c) chars[index++] = c;
    for (char c = '1'; c < '1' + Ranks; ++c) chars[index++] = c;

    for (char c : extraSANchars) chars[index++] = c;
    return chars;
}
auto squareChars = makeSquareChars<kNumFiles, kNumRanks>();
auto validSANchars = std::string(squareChars.data(), sizeof(squareChars));

sv_iterator skipSANmove(sv_iterator it, sv_iterator end) {
    it = skipWhiteSpace(it, end);
    if (it == end) return it;

    // SAN moves can only start with: K,Q,R,B,N (pieces), a-h (files), or O (castling)
    // They cannot start with digits, -, +, #, x, =, ?, !
    char first = *it;
    if (first != 'K' && first != 'Q' && first != 'R' && first != 'B' && first != 'N' &&
        first != 'O' && (first < 'a' || first > 'a' + kNumFiles - 1))
        return it;

    ++it;

    // Rest of move can contain any valid SAN characters
    while (it != end && validSANchars.find(*it) != std::string::npos) ++it;

    return it;
}
std::string terminationMarkers[] = {"1-0", "0-1", "1/2-1/2", "*"};
}  // namespace

std::string extractSANMoves(const std::string& movetext) {
    std::string result;
    std::string_view view(movetext);
    auto it = view.begin();
    auto end = view.end();

    while (it != end) {
        // Skip whitespace, comments, NAGs, variations and move numbers first, but not game results
        it = skipToSANmove(it, end);
        it = skipWhiteSpace(it, end);
        if (it == end) break;

        sv_iterator prevIt;
        do {
            prevIt = it;
            it = skipNAG(it, end);
            it = skipBracedComment(it, end);
            it = skipSemicolonComment(it, end);
            it = skipVariations(it, end);
            it = skipWhiteSpace(it, end);
        } while (it != prevIt && it != end);

        if (it == end) break;

        // Check for game termination markers (before skipMoveNumber consumes digits)
        auto remaining = end - it;
        if (remaining >= 7 && std::string(it, it + 7) == "1/2-1/2") {
            result += "1/2-1/2";
            break;
        }
        if (remaining >= 3 &&
            (std::string(it, it + 3) == "1-0" || std::string(it, it + 3) == "0-1")) {
            result += std::string(it, it + 3);
            break;
        }
        if (*it == '*') {
            result += "*";
            break;
        }

        // Now skip move numbers
        it = skipMoveNumber(it, end);
        if (it == end) break;

        auto moveStart = it;
        it = skipSANmove(it, end);

        if (it == moveStart) {
            // Can't parse as a move, skip this character to avoid infinite loop
            ++it;
            continue;
        }

        std::string move(moveStart, it);
        result += move + " ";
    }

    return result;
}
PGN readPGN(std::istream& in) {
    PGN pgn;

    if (!readTags(in, pgn.tags)) return pgn;

    std::string line;
    while (std::getline(in, line)) {
        if (pgn.movetext.empty() && line.empty()) continue;  // Skip leading blank lines
        if (line.empty()) break;  // Stop at first blank line after movetext
        pgn.movetext += line + "\n";
    }
    return pgn;
}

}  // namespace pgn