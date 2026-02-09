#include <cassert>
#include <istream>
#include <string>
#include <string_view>

#include "pgn.h"

#include "core/core.h"
#include "move/move.h"
#include "move/move_gen.h"

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

    // Skip leading blank lines or escape lines starting with '%'
    while (in.peek() == '\n' || in.peek() == '%') std::getline(in, line);

    // Check if we hit EOF or have an empty tag section
    if (in.peek() != '[') return false;

    // Process tag lines - an empty line or non-tag line ends the tag section
    while (in.peek() == '[' && std::getline(in, line)) {
        if (line.empty() || line[0] != '[' || line.back() != ']') break;  // Stop at non-tag line

        // Find the first space separating tag name and value
        auto spacePos = line.find(' ');
        if (spacePos == std::string::npos) continue;  // Skip malformed tags

        line.pop_back();  // Remove trailing ']'
        std::string tagName = line.substr(1, spacePos - 1);
        std::string tagValue = removeQuotes(line.substr(spacePos + 1));
        tags.emplace_back(tagName, tagValue);
    }

    return !tags.empty();
}

void readMoveText(std::istream& in, std::string& movetext) {
    std::string line;
    while (std::getline(in, line)) {
        if (movetext.empty() && line.empty()) continue;  // Skip leading blank lines
        if (line.empty() && in.peek() == '[') break;     // Stop at blank line followed by tag
        movetext += line + "\n";
    }
}

const char* skipWhitespace(const char* it) {
    // In PGN, whitespace includes only space, newline, carriage return, and tab.
    while (*it == ' ' || *it == '\n' || *it == '\r' || *it == '\t') ++it;
    return it;
}

const char* skipMoveNumber(const char* it) {
    it = skipWhitespace(it);
    auto beforeDigits = it;
    while (*it && std::isdigit(*it)) ++it;
    it = skipWhitespace(it);
    // Don't consume digits if not followed by '.' or '...' as that may be the result like "0-1".
    if (!*it || *it != '.') return beforeDigits;
    ++it;
    if (*it == '.' && *(it + 1) == '.') it += 2;  // handle 1...
    return it;
}

const char* skipNAG(const char* it) {
    it = skipWhitespace(it);
    if (!*it || *it != '$') return it;
    ++it;
    while (*it && std::isdigit(*it)) ++it;
    return it;
}

const char* skipBracedComment(const char* it) {
    it = skipWhitespace(it);
    if (!*it || *it != '{') return it;
    ++it;
    while (*it && *it != '}') ++it;
    if (*it) ++it;  // skip closing brace
    return it;
}

const char* skipSemicolonComment(const char* it) {
    it = skipWhitespace(it);
    if (!*it || *it != ';') return it;
    while (*it && *it != '\n') ++it;
    return it;
}

const char* skipVariations(const char* it) {
    it = skipWhitespace(it);
    if (!*it || *it != '(') return it;

    int depth = 1;
    ++it;
    while (*it && depth > 0) {
        if (*it == '(')
            ++depth;
        else if (*it == ')')
            --depth;
        ++it;
    }
    return it;
}

/**
 * Skips annotations, NAGs, comments, variations and whitespace. For correctly formatted PGN
 * movetext, returns a pointer to the next SAN move, the game result, or the nul terminator of the
 * movetext. This function does not validate the SAN move. */
const char* skipToSANmove(const char* it) {
    const char* prevIt;
    do {
        prevIt = it;
        it = skipNAG(it);
        it = skipBracedComment(it);
        it = skipSemicolonComment(it);
        it = skipVariations(it);
        it = skipMoveNumber(it);
        it = skipWhitespace(it);
    } while (it != prevIt && *it);
    return it;
}

/**
 * Skips the SAN move or game result pointed at by the iterator, but not any annotations. Skips
 * eagerly until the next annotation or whitespace, and does not validate the SAN move at all.
 */
const char* skipSANmove(const char* it) {
    // Stop at any control character, whitespace, or annotation start
    while (*it > '!' && *it != '?' && *it <= 'z') ++it;

    return it;
}

/**
 * Skips any annotations and whitespace.
 */
const char* skipAnnotationsAndWhitespace(const char* it) {
    it = skipWhitespace(it);
    if (*it != '!' && *it != '?') return it;

    do {
        ++it;  // skip '!' or '?' annotations
    } while (*it == '!' || *it == '?');
    return skipWhitespace(it);
}

bool parseEnd(std::string_view& item, char c) {
    if (item.empty() || item.back() != c) return false;
    item.remove_suffix(1);
    return true;
}

bool parseEnd(std::string_view& item, std::string_view suffix) {
    if (item.size() < suffix.size() ||
        item.substr(item.size() - suffix.size(), suffix.size()) != suffix)
        return false;
    item.remove_suffix(suffix.size());
    return true;
}

/** Consumes the last character of the item if it is within the specified range [begin, end). */
char parseRangeEnd(std::string_view& item, char begin, char end) {
    if (item.empty() || item.back() < begin || item.back() >= end) return 0;
    char result = item.back();
    item.remove_suffix(1);

    return result;
}

bool parseBegin(std::string_view& item, char c) {
    if (item.empty() || item.front() != c) return false;
    item.remove_prefix(1);
    return true;
}

}  // namespace

SAN::SAN(std::string_view move) {
    // Handle termination markers
    if (move == "1-0") kind = TERMINATION_WHITE_WIN;
    if (move == "0-1") kind = TERMINATION_BLACK_WIN;
    if (move == "1/2-1/2") kind = TERMINATION_DRAW;
    if (move == "*") kind = TERMINATION_UNKNOWN;
    if (kind != NOTATION_ERROR) return;  // Done if termination marker

    // Handle check/checkmate
    if (parseEnd(move, '+'))
        check = CHECK;
    else if (parseEnd(move, '#'))
        check = CHECKMATE;

    // Handle castling, which can have check/checkmate
    piece = PieceType::KING;
    if (move == "O-O") kind = O_O;
    if (move == "O-O-O") kind = O_O_O;
    if (kind != NOTATION_ERROR) return;  // Done if castling

    // Handle non-pawn pieces
    piece = parseBegin(move, 'N') ? PieceType::KNIGHT
        : parseBegin(move, 'B')   ? PieceType::BISHOP
        : parseBegin(move, 'R')   ? PieceType::ROOK
        : parseBegin(move, 'Q')   ? PieceType::QUEEN
        : parseBegin(move, 'K')   ? PieceType::KING
                                  : PieceType::PAWN;

    // Handle promotion
    promotion = parseEnd(move, "=Q") ? PieceType::QUEEN
        : parseEnd(move, "=R")       ? PieceType::ROOK
        : parseEnd(move, "=B")       ? PieceType::BISHOP
        : parseEnd(move, "=N")       ? PieceType::KNIGHT
                                     : PieceType::EMPTY;

    // Parse destination square
    auto rankChar = parseRangeEnd(move, '1', '1' + kNumRanks);
    auto fileChar = parseRangeEnd(move, 'a', 'a' + kNumFiles);

    if (!fileChar || !rankChar) return;  // Cannot parse destination square
    to = makeSquare(fileChar - 'a', rankChar - '1');

    // Parse capture if present
    kind = parseEnd(move, 'x') ? kind = CAPTURE : MOVE;

    // Finally handle disambiguation
    disambiguationRank = parseRangeEnd(move, '1', '1' + kNumRanks);
    disambiguationFile = parseRangeEnd(move, 'a', 'a' + kNumFiles);

    // Pawn captures require disambiguation file
    if (piece == PieceType::PAWN && kind == CAPTURE && !disambiguationFile) kind = NOTATION_ERROR;

    // Only pawns can promote
    if (promotion != PieceType::EMPTY && piece != PieceType::PAWN) kind = NOTATION_ERROR;

    // If any move text remains, parsing failed
    if (!move.empty()) kind = NOTATION_ERROR;
}

SAN::operator std::string() const {
    if (kind == NOTATION_ERROR) return "";
    std::string checkStr = check == CHECK ? "+" : check == CHECKMATE ? "#" : "";

    if (kind == O_O) return "O-O" + checkStr;
    if (kind == O_O_O) return "O-O-O" + checkStr;
    if (kind == TERMINATION_WHITE_WIN) return "1-0";
    if (kind == TERMINATION_BLACK_WIN) return "0-1";
    if (kind == TERMINATION_DRAW) return "1/2-1/2";
    if (kind == TERMINATION_UNKNOWN) return "*";

    std::string result;

    // Piece
    if (piece != PieceType::PAWN) result += "PNBRQK"[index(piece)];

    // Disambiguation
    if (disambiguationFile) result += disambiguationFile;
    if (disambiguationRank) result += disambiguationRank;

    // Capture
    if (kind == CAPTURE) result += 'x';

    // Destination square
    result += to_string(to);

    // Promotion
    if (promotion != PieceType::EMPTY) result += "=" + std::string(1, "NBRQ"[index(promotion) - 1]);

    return result + checkStr;
}

Move move(Position position, SAN san) {
    if (!san) return Move{};  // Invalid SAN
    Move result;
    int matches = 0;
    moves::forAllLegalMovesAndCaptures(
        position.turn, position.board, [&]([[maybe_unused]] Board& board, MoveWithPieces mwp) {
            // Check for piece type and destination square
            if (type(mwp.piece) != san.piece) return;
            if (mwp.move.to != san.to && !isCastles(mwp.move.kind)) return;

            // Either both are captures or both are non-captures
            if ((mwp.captured != Piece::_) != (san.kind == SAN::CAPTURE)) return;

            // Check castling
            if ((mwp.move.kind == MoveKind::O_O) != (san.kind == SAN::O_O)) return;
            if ((mwp.move.kind == MoveKind::O_O_O) != (san.kind == SAN::O_O_O)) return;

            // Check promotion
            if (promotionType(mwp.move.kind) != san.promotion) return;

            // Check disambiguation
            auto from = to_string(mwp.move.from);
            if (san.disambiguationFile && from[0] != san.disambiguationFile) return;
            if (san.disambiguationRank && from[1] != san.disambiguationRank) return;

            ++matches;
            result = mwp.move;
        });
    return matches == 1 ? result : Move{};  // Return move if exactly one match found
}

PGN::iterator PGN::begin() const {
    return iterator(skipToSANmove(movetext.data()));
}

PGN::iterator& PGN::iterator::operator++() {
    // Skip the actual SAN move to position after it, always skip something even if junk
    it = skipSANmove(++it);
    // Skip any annotations and whitespace
    it = skipAnnotationsAndWhitespace(it);
    // Skip to the next SAN move
    it = skipToSANmove(it);
    return *this;
}

SAN PGN::iterator::operator*() const {
    // Find the end of the current SAN move
    auto start = it;
    auto end = skipSANmove(std::next(start));
    return SAN(std::string_view(start, end - start));
}

std::string PGN::error(iterator it) const {
    if (it == end()) return "No terminator found";
    SAN move = *it;
    std::string_view parsed = std::string_view(begin().it, it.it - begin().it);
    auto end = skipSANmove(std::next(it.it));
    while (static_cast<unsigned char>(*end) >= 128) ++end;  // Keep UTF-8 chars intact
    std::string_view token = std::string_view(it.it, end - it.it);
    size_t row = std::count(parsed.begin(), parsed.end(), '\n') + 1;
    size_t col = parsed.size() - parsed.rfind('\n');
    std::string msg =
        move ? "Invalid move " + std::string(token) : "Invalid SAN notation " + std::string(token);
    return std::to_string(row) + ":" + std::to_string(col) + ": " + msg;
}

VerifiedGame verify(PGN const& pgn) {

    Position position = Position::initial();
    MoveVector moves;
    for (auto san : pgn) {
        if (!san) return {moves, Termination::NOTATION_ERROR};
        if (san.terminator())
            return {moves,
                    san.kind == SAN::TERMINATION_WHITE_WIN       ? Termination::WHITE_WIN
                        : san.kind == SAN::TERMINATION_BLACK_WIN ? Termination::BLACK_WIN
                        : san.kind == SAN::TERMINATION_DRAW      ? Termination::DRAW
                                                                 : Termination::UNKNOWN};
        if (auto move = pgn::move(position, san))
            position = moves::applyMove(position, moves.emplace_back(move));
        else
            return {moves, Termination::MOVE_ERROR};
    }
    return {moves, Termination::INCOMPLETE_ERROR};  // No terminator found
}

PGN readPGN(std::istream& in) {
    PGN pgn;

    if (!readTags(in, pgn.tags)) return pgn;

    readMoveText(in, pgn.movetext);
    return pgn;
}

}  // namespace pgn