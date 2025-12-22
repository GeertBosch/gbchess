#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <vector>

#include "core/core.h"

namespace pgn {

/** Represents a SAN move parsed from PGN, or a termination marker. While that marker is not
 *  strictly a SAN move, using the same type helps with iterating over the movetext.
 */
struct SAN {
    enum CheckKind : uint8_t { NONE, CHECK, CHECKMATE };
    enum NotationKind : uint8_t {
        NOTATION_ERROR,
        MOVE,
        CAPTURE,
        O_O,
        O_O_O,
        TERMINATION_WHITE_WIN,
        TERMINATION_BLACK_WIN,
        TERMINATION_DRAW,
        TERMINATION_UNKNOWN
    };
    char disambiguationFile = 0;
    char disambiguationRank = 0;
    Square to = {};  // Not set for castles as side to play is unknown here
    PieceType piece = PieceType::PAWN;
    PieceType promotion = PieceType::EMPTY;
    CheckKind check = NONE;
    NotationKind kind = NOTATION_ERROR;

    SAN(std::string_view move);

    bool terminator() const { return kind >= TERMINATION_WHITE_WIN && kind <= TERMINATION_UNKNOWN; }

    operator bool() const { return kind != NOTATION_ERROR; }

    operator std::string() const;
};

/**
 * Converts the SAN move to a Move object given the current position. Returns Move{} if the SAN
 * move is invalid in the given position.
 */
Move move(Position position, SAN san);

/**
 * Represents a PGN game with tags and movetext.
 */
struct PGN {
    using TagPair = std::pair<std::string, std::string>;
    using Tags = std::vector<TagPair>;
    Tags tags;
    std::string movetext;

    /** Returns true iff the PGN object contains a non-empty movetext. */
    explicit operator bool() const { return !movetext.empty(); }

    /** Returns the value of the specified tag, or an empty string if not found. */
    std::string operator[](std::string_view tag) const {
        for (auto&& [key, value] : tags)
            if (key == tag) return value;
        return {};
    }

    /**
     *  Iterator over SAN moves and game result in the PGN movetext. Skips comments, annotations,
     *  move numbers, NAGs, and variations. Does not do any validation of the moves or game result.
     */
    class iterator {
        friend struct PGN;

    public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = std::string;
        using pointer = std::string*;
        using reference = std::string&;

        iterator(const char* it) : it(it) {}
        iterator(const std::string& movetext) : it(movetext.data()) {}
        iterator& operator++();
        SAN operator*() const;
        bool operator!=(const iterator& other) const { return it != other.it; }
        bool operator==(const iterator& other) const { return it == other.it; }

    private:
        const char* it;
    };

    iterator begin() const;
    iterator end() const { return iterator(movetext.data() + movetext.size()); }

    /**
     * Call with it == end for games with missing termination, or !*it for SAN errors. If the SAN
     * move is syntactically valid, assumes that the error is due to move legality. Produces an
     * appropriate error string with line, column and message
     */
    std::string error(iterator it) const;
};

/** Represents the termination marker of the PGN game, or ERROR if the game was invalid. */
enum class Termination : uint8_t {
    NOTATION_ERROR,
    MOVE_ERROR,
    INCOMPLETE_ERROR,
    WHITE_WIN,
    BLACK_WIN,
    DRAW,
    UNKNOWN,
    COUNT
};
using VerifiedGame = std::pair<MoveVector, Termination>;  // For now assume initial position

/** Verifies validity of the moves in the PGN. Returns an ERROR termination for invalid games. */
VerifiedGame verify(PGN const& pgn);

PGN readPGN(std::istream& in);
}  // namespace pgn