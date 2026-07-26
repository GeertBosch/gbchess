#include "book/sprt_suite.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "book/book.h"
#include "core/core.h"
#include "core/hash/hash.h"
#include "core/text_util.h"
#include "engine/fen/fen.h"
#include "move/move.h"
#include "move/move_gen.h"

namespace sprt_suite {
namespace {

// Balance window on White's raw empirical score at the frontier position itself: diversity is
// the goal here, not balance, so this only screens out positions that are already decided.
// A tight [0.45, 0.55] window leaves too few positions (193 of 581 frontier positions in the
// current book.csv); widened to keep several hundred.
constexpr double kMinBalanceScore = 0.35;
constexpr double kMaxBalanceScore = 0.65;
// Cap positions per ECO code so one popular family (e.g. one Sicilian line) cannot dominate.
constexpr int kMaxPerECO = 20;

struct SuiteEntry {
    std::string eco;
    std::string name;
    std::string fen;
    uint64_t white;
    uint64_t draw;
    uint64_t black;
};

/** book.csv quotes the name field to protect embedded commas; undo that for output. */
std::string stripQuotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') return s.substr(1, s.size() - 2);
    return s;
}

/** Convert FEN string to EPD by keeping only the first 4 fields */
std::string fenToEPD(const std::string& fen) {
    std::istringstream in(fen);
    std::string board;
    std::string turn;
    std::string castling;
    std::string ep;
    if (!(in >> board >> turn >> castling >> ep)) return "";
    return board + " " + turn + " " + castling + " " + ep;
}

/** Escape string for quoted EPD opcode values */
std::string escapeEPDString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        if (c == '\\' || c == '"') escaped.push_back('\\');
        escaped.push_back(c);
    }
    return escaped;
}

/** True if some legal successor of `position` is itself a book position with enough games. */
bool hasBookContinuation(const Position& position,
                         const std::unordered_map<uint64_t, book::BookEntry>& entries) {
    for (auto move : moves::allLegalMoves(position.turn, position.board)) {
        Position next = moves::applyMove(position, move);
        auto it = entries.find(Hash(next)());
        if (it != entries.end() && it->second.total() >= book::kMinGames) return true;
    }
    return false;
}

/** Collect book-frontier positions from `bookFile`, i.e. book positions with no legal successor
 *  that is itself a well-supported book position, filtered to a balanced score window. */
std::vector<SuiteEntry> collectFrontierPositions(const std::string& bookFile, book::Book& bk) {
    std::ifstream in(bookFile);
    if (!in || !in.is_open()) {
        std::cerr << "Could not open book: " << bookFile << "\n";
        return {};
    }

    std::string line;
    std::getline(in, line);
    auto header = split(line, ',');
    auto fenCol = find(header, "fen");

    std::unordered_map<std::string, int> ecoCounts;
    std::vector<SuiteEntry> candidates;

    while (std::getline(in, line)) {
        auto columns = split(line, ',');
        if (columns.size() < header.size()) continue;

        std::string fenStr = columns[fenCol];
        Position position = fen::parsePosition(fenStr);

        auto it = bk.entries.find(Hash(position)());
        if (it == bk.entries.end()) continue;  // Should not happen: same file we just loaded
        const book::BookEntry& entry = it->second;

        if (hasBookContinuation(position, bk.entries)) continue;  // Not a frontier position

        // Raw empirical white score, not the book's own Bayesian-shrunk posteriorMean: with
        // kPriorStrength=2500 (book.h), that shrinkage is tuned for move *selection* and pulls
        // every sample this size toward the global average, making it useless for measuring
        // this position's own balance.
        double score = (entry.white + 0.5 * entry.draw) / double(entry.total());
        if (score < kMinBalanceScore || score > kMaxBalanceScore) continue;

        std::string ecoStr = std::string(entry.eco);
        if (++ecoCounts[ecoStr] > kMaxPerECO) continue;

        candidates.push_back(
            {ecoStr, stripQuotes(entry.name), fenStr, entry.white, entry.draw, entry.black});
    }

    return candidates;
}

/** Write frontier positions to an EPD file, in the same format as book_gen's writeBookEPD. */
size_t writeSuite(const std::string& outputFile, std::vector<SuiteEntry>& entries) {
    std::sort(entries.begin(), entries.end(), [](const SuiteEntry& a, const SuiteEntry& b) {
        if (a.eco != b.eco) return a.eco < b.eco;
        return a.name < b.name;
    });

    std::ofstream out(outputFile);
    if (!out || !out.is_open()) {
        std::cerr << "Could not open output file: " << outputFile << "\n";
        return 0;
    }

    size_t writtenCount = 0;
    for (const auto& e : entries) {
        std::string epdPosition = fenToEPD(e.fen);
        if (epdPosition.empty()) continue;

        out << epdPosition << " id \"" << escapeEPDString(e.name) << "\";"
            << " eco \"" << e.eco << "\";"
            << " c0 \"" << e.white << "," << e.draw << "," << e.black << "\";"
            << "\n";
        ++writtenCount;
    }

    return writtenCount;
}

}  // namespace

int run(const std::string& bookFile, const std::string& outputFile) {
    book::Book bk = book::loadBook(bookFile);
    if (!bk) {
        std::cerr << "Could not load book: " << bookFile << "\n";
        return 1;
    }

    auto candidates = collectFrontierPositions(bookFile, bk);
    if (candidates.empty()) {
        std::cerr << "No frontier positions found in " << bookFile << "\n";
        return 1;
    }

    size_t written = writeSuite(outputFile, candidates);
    if (!written) return 1;

    std::cout << "Wrote " << written << " SPRT suite positions to " << outputFile << "\n";
    return 0;
}

}  // namespace sprt_suite
