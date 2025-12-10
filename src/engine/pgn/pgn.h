#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/core.h"

namespace pgn {

/** Represents a SAN move parsed from PGN, or a termination marker. While that marker is not
 *  strictly a SAN move, using the same type helps with iterating over the movetext.
 */
struct SAN {
    enum CheckKind : uint8_t { NONE, CHECK, CHECKMATE };
    enum MoveKind : uint8_t {
        ERROR,
        MOVE,
        CAPTURE,
        CASTLING_KINGSIDE,
        CASTLING_QUEENSIDE,
        TERMINATION_WHITE_WIN,
        TERMINATION_BLACK_WIN,
        TERMINATION_DRAW,
        TERMINATION_UNKNOWN
    };
    char disambiguationFile = 0;
    char disambiguationRank = 0;
    Square to = {};
    PieceType piece = PieceType::PAWN;
    PieceType promotion = PieceType::EMPTY;
    CheckKind check = NONE;
    MoveKind kind = ERROR;

    SAN(std::string_view move);

    bool terminator() const { return kind >= TERMINATION_WHITE_WIN && kind <= TERMINATION_UNKNOWN; }

    operator bool() const { return kind != ERROR; }

    operator std::string() const;
};

/**
 * Represents a PGN game with tags and movetext.
 */
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
     * Returning an iterator pointing to the termination marker of the movetext, or end() if none
     * exists. If any moves are not in valid SAN format, will return an iterator pointing to the
     * first such error.
     */
    iterator terminator() const;

    bool valid() const {
        auto it = terminator();
        return it != end() && *it && ++it == end();
    }
    std::string error(iterator it) const;
};

PGN readPGN(std::istream& in);
}  // namespace pgn