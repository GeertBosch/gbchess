#include "book/book.h"
#include "book/pgn/pgn.h"
#include "core/core.h"
#include "core/hash/hash.h"
#include "engine/fen/fen.h"
#include "move/move.h"

#include <fcntl.h>
#include <string_view>
#include <unistd.h>

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

using namespace book;
namespace {

// Minimum game time in seconds (excludes bullet and blitz games)
static constexpr int kMinGameTimeSeconds = 180;
static constexpr int kMaxOpeningMoves = 12;
static constexpr int kMinElo = 2700;  // Average ELO to include the game into the book
static constexpr int kMaxEloSpread = 300;

// Chunk size for parallel processing ( MB)
static constexpr size_t kChunkSize = 8 * 1024 * 1024;

/** Reference to a position in a PGN file for lazy FEN generation */
struct PositionRef {
    PositionRef() = default;
    uint64_t offset;     // Byte offset in the file where game starts
    uint16_t fileIndex;  // Index into the list of PGN files
    uint16_t ply;        // Ply number (half-moves from start, 0 means startpos)
    operator bool() const { return ply; }
};

enum ECOCode : uint16_t {
    // clang-format off

    // A Codes (Flank Openings)
    A00, A01, A02, A03, A04, A05, A06, A07, A08, A09, // A00-A09: Uncommon openings
    A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, // A10-A39: English Opening
    A20, A21, A22, A23, A24, A25, A26, A27, A28, A29,
    A30, A31, A32, A33, A34, A35, A36, A37, A38, A39,
    A40, A41, A42, A43, A44,                          // A40-44: Queen's Pawn Game
    A45, A46, A47, A48, A49,                          // A45-99: Indian Defenses
    A50, A51, A52, A53, A54, A55, A56, A57, A58, A59,
    A60, A61, A62, A63, A64, A65, A66, A67, A68, A69,
    A70, A71, A72, A73, A74, A75, A76, A77, A78, A79,
    A80, A81, A82, A83, A84, A85, A86, A87, A88, A89,
    A90, A91, A92, A93, A94, A95, A96, A97, A98, A99,

    // B Codes (Semi-Open Games)
    B00, B01, B02, B03, B04, B05, B06, B07, B08, B09, // B00-B09: King's Pawn Game
    B10, B11, B12, B13, B14, B15, B16, B17, B18, B19, // B10-B19: Caro-Kann Defense
    B20, B21, B22, B23, B24, B25, B26, B27, B28, B29, // B20-B99: Sicilian Defense
    B30, B31, B32, B33, B34, B35, B36, B37, B38, B39,
    B40, B41, B42, B43, B44, B45, B46, B47, B48, B49,
    B50, B51, B52, B53, B54, B55, B56, B57, B58, B59,
    B60, B61, B62, B63, B64, B65, B66, B67, B68, B69,
    B70, B71, B72, B73, B74, B75, B76, B77, B78, B79,
    B80, B81, B82, B83, B84, B85, B86, B87, B88, B89,
    B90, B91, B92, B93, B94, B95, B96, B97, B98, B99,

    // C Codes (Open Games)
    C00, C01, C02, C03, C04, C05, C06, C07, C08, C09, // C00-C19: French Defense
    C10, C11, C12, C13, C14, C15, C16, C17, C18, C19,
    C20, C21, C22, C23, C24, C25, C26, C27, C28, C29, // C20-C29: King's Pawn Game
    C30, C31, C32, C33, C34, C35, C36, C37, C38, C39, // C30-C39: King's Gambit
    C40, C41, C42, C43, C44, C45, C46, C47, C48, C49, // C40-C49: King's Knight Opening
    C50, C51, C52, C53, C54, C55, C56, C57, C58, C59, // C50-C59: Italian Game
    C60, C61, C62, C63, C64, C65, C66, C67, C68, C69, // C60-C99: Ruy Lopez
    C70, C71, C72, C73, C74, C75, C76, C77, C78, C79,
    C80, C81, C82, C83, C84, C85, C86, C87, C88, C89,
    C90, C91, C92, C93, C94, C95, C96, C97, C98, C99,

    // D Codes (Closed Games)
    D00, D01, D02, D03, D04, D05,                     // D00-D05: Queen's Pawn Game
    D06, D07, D08, D09, D10, D11, D12, D13, D14, D15, // D06-D69: Queen's Gambit
    D16, D17, D18, D19, D20, D21, D22, D23, D24, D25,
    D26, D27, D28, D29, D30, D31, D32, D33, D34, D35,
    D36, D37, D38, D39, D40, D41, D42, D43, D44, D45,
    D46, D47, D48, D49, D50, D51, D52, D53, D54, D55,
    D56, D57, D58, D59, D60, D61, D62, D63, D64, D65,
    D66, D67, D68, D69,
    D70, D71, D72, D73, D74, D75,                     // D70-D99: Grünfeld Defense
    D76, D77, D78, D79, D80, D81, D82, D83, D84, D85,
    D86, D87, D88, D89, D90, D91, D92, D93, D94, D95,
    D96, D97, D98, D99,

    // E Codes (Indian Defenses)
    E00, E01, E02, E03, E04, E05, E06, E07, E08, E09, // E00-E09: Catalan Opening
    E10, E11, E12, E13, E14, E15, E16, E17, E18, E19, // E10-E19: Queen's Indian Defense
    E20, E21, E22, E23, E24, E25, E26, E27, E28, E29, // E20-E59: Nimzo-Indian Defense
    E30, E31, E32, E33, E34, E35, E36, E37, E38, E39,
    E40, E41, E42, E43, E44, E45, E46, E47, E48, E49,
    E50, E51, E52, E53, E54, E55, E56, E57, E58, E59,
    E60, E61, E62, E63, E64, E65, E66, E67, E68, E69, // E60-E99: King's Indian Defense
    E70, E71, E72, E73, E74, E75, E76, E77, E78, E79,
    E80, E81, E82, E83, E84, E85, E86, E87, E88, E89,
    E90, E91, E92, E93, E94, E95, E96, E97, E98, E99,
    // clang-format on
};

std::string to_string(ECOCode code) {
    char volume = 'A' + code / 100;
    char digit1 = '0' + (code % 100) / 10;
    char digit2 = '0' + code % 10;
    return std::string{volume, digit1, digit2};
}

bool validECO(std::string_view eco) {
    return eco.size() == 3 && eco[0] >= 'A' && eco[0] <= 'E' && eco[1] >= '0' && eco[1] <= '9' &&
        eco[2] >= '0' && eco[2] <= '9';
}

struct ECORange {
    ECOCode min;
    ECOCode max;
    ECORange() : min(ECOCode::E99), max(ECOCode::A00) {}  // Default to invalid range
    ECORange(ECOCode min, ECOCode max) : min(min), max(max) {}
    ECORange(std::string_view eco) : ECORange() {
        if (!validECO(eco)) return;

        min = max = ECOCode((eco[0] - 'A') * 100 + (eco[1] - '0') * 10 + (eco[2] - '0'));
    }
    explicit operator bool() const { return min <= max; }
    operator std::string() const {
        if (!*this) return "";
        if (min == max) return to_string(min);
        return to_string(min) + "-" + to_string(max);
    }
};

/** Name and ECO code of an opening */
struct Opening {
    Opening() = default;
    Opening(std::string name, std::string eco, int plies)
        : name(std::move(name)), eco(eco), plies(plies) {}
    Opening(pgn::PGN const& game)
        : Opening(game["Opening"], game["ECO"], std::distance(game.begin(), game.end())) {}

    operator bool() const { return plies; }
    std::string name;  // Like "Grünfeld Defense: Exchange Variation"
    ECORange eco;      // Range of ECO codes covered by this opening
    int plies = 0;     // Number of plies in this opening, 0 means no opening
};

std::string commonPrefix(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;

    size_t prefixLength = 0;
    while (prefixLength < a.size() && prefixLength < b.size() && a[prefixLength] == b[prefixLength])
        ++prefixLength;
    return a.substr(0, prefixLength);
}

ECORange merge(const ECORange& a, const ECORange& b) {
    return {std::min(a.min, b.min), std::max(a.max, b.max)};
}

bool isTrailingPunctuation(char c) {
    return c == ' ' || c == ':' || c == '-' || c == ',';
}

Opening merge(const Opening& a, const Opening& b) {
    Opening result;
    // As we try to register a given game at many different plies, we should avoid overriding
    // more general openings with more specific ones.
    if (!a) return b;
    if (!b) return a;
    if (a.plies < b.plies) return a;
    if (b.plies < a.plies) return b;

    result.eco = merge(a.eco, b.eco);

    // We expect the name to start with the most general name, followed by more specific
    // variations. We cannot depend on the ECO code to help with merging names, as for example
    // A00 covers a wide variety of openings with different names. Instead, we look for the
    // longest common prefix of the names and use that as the merged result. At the end, if
    // we end up with an empty name, we'll fall back to using the ECO code range to generate a name.
    result.name = commonPrefix(a.name, b.name);

    while (result.name.size() && isTrailingPunctuation(result.name.back())) result.name.pop_back();

    return result;
}

/** Combined book entry with game statistics and position reference */
struct BookPosition {
    BookEntry entry;
    PositionRef ref;
    Opening opening;
    int ply() const { return ref ? ref.ply : 0; }
};

struct BookStats {
    uint64_t gamesProcessed = 0;
    uint64_t gamesAccepted = 0;
    uint64_t droppedLowElo = 0;
    uint64_t droppedShortTime = 0;
    uint64_t droppedVariants = 0;
    uint64_t droppedOverflow = 0;  // Games dropped due to position count overflow

    uint64_t totalDropped() const {
        return droppedLowElo + droppedShortTime + droppedVariants + droppedOverflow;
    }
};

/** Safe positive integer parsing without exceptions - returns 0 for invalid input */
static int parsePositiveInt(const std::string& str) {
    if (str.empty()) return 0;

    // Skip leading '+' sign if any
    size_t pos = str[0] == '+';

    // Avoid overflow by rejecting input over 9 digits
    if (str.length() - pos > 9) return 0;

    int value = 0;
    for (; pos < str.length(); ++pos) {
        char c = str[pos];
        if (c < '0' || c > '9') return 0;

        value = value * 10 + (c - '0');
    }

    return value;
}

/** Check if a game meets quality criteria for inclusion in the book */
bool shouldIncludeGame(const pgn::PGN& game, BookStats& stats) {
    auto whiteElo = parsePositiveInt(game["WhiteElo"]);
    auto blackElo = parsePositiveInt(game["BlackElo"]);
    auto eloSpread = std::abs(whiteElo - blackElo);

    // Use average if spread is reasonable, otherwise penalize the higher ELO
    auto gameElo = eloSpread <= kMaxEloSpread ? (whiteElo + blackElo) / 2
                                              : std::min(whiteElo, blackElo) + kMaxEloSpread / 2;

    // Exclude games with low ELO, as they are more likely to contain moves that pollute the book.
    if (gameElo < kMinElo) return ++stats.droppedLowElo, false;

    // Exclude non-standard variants and games starting from a specific position
    if (game["Variant"].size() && game["Variant"] != "Standard") {
        return ++stats.droppedVariants, false;
    }
    if (game["FEN"].size()) return ++stats.droppedVariants, false;


    // Parse TimeControl format: "base+increment" (in seconds)
    auto timeControl = game["TimeControl"];
    if (timeControl.empty() || timeControl == "?" || timeControl == "-")
        return ++stats.droppedShortTime, false;

    // Either no increment, or base + increment, all assumed in seconds
    size_t plusPos = timeControl.find('+');
    if (plusPos == std::string::npos)
        return parsePositiveInt(timeControl) < kMinGameTimeSeconds ? ++stats.droppedShortTime,
               false                                               : true;

    // For 40 moves, effective time = base + 40 * increment
    auto base = parsePositiveInt(timeControl.substr(0, plusPos));
    auto increment = parsePositiveInt(timeControl.substr(plusPos + 1));
    if (base + 40 * increment < kMinGameTimeSeconds) return ++stats.droppedShortTime, false;

    return true;
}

/** Add a game to local book entries (no locking needed) */
void insertGame(const pgn::VerifiedGame& verifiedGame,
                std::unordered_map<uint64_t, BookPosition>& positions,
                const PositionRef& gameRef,
                const Opening& opening) {
    auto&& [moves, term] = verifiedGame;

    if (term != pgn::Termination::WHITE_WIN && term != pgn::Termination::BLACK_WIN &&
        term != pgn::Termination::DRAW)
        return;

    Position position = Position::initial();
    uint16_t ply = 0;
    for (auto move : moves) {
        position = moves::applyMove(position, move);
        ++ply;
        uint64_t key = Hash(position)();

        auto& pos = positions[key];
        auto& entry = pos.entry;

        // Stop adding to this position once any component is saturated to avoid bias
        if (entry.full()) continue;

        if (term == pgn::Termination::WHITE_WIN) ++entry.white;
        if (term == pgn::Termination::BLACK_WIN) ++entry.black;
        if (term == pgn::Termination::DRAW) ++entry.draw;

        // Store position reference (file offset + ply) for later FEN reconstruction
        if (!pos.ref) pos.ref = {gameRef.offset, gameRef.fileIndex, ply};

        // Merge opening info, so early moves will have more general opening names and ECO ranges,
        // while later moves will have more specific variations.
        pos.opening = merge(pos.opening, opening);

        if (position.turn.fullmove() >= kMaxOpeningMoves) break;  // Only index opening moves
    }
}

/** Merge local entries into global entries (with mutex), returns count of dropped overflow games */
uint64_t mergeEntries(std::unordered_map<uint64_t, BookPosition>& localPositions,
                      std::unordered_map<uint64_t, BookPosition>& globalPositions,
                      std::mutex& mutex) {
    std::lock_guard<std::mutex> lock(mutex);
    uint64_t droppedOverflow = 0;

    for (const auto& [key, localPos] : localPositions) {
        auto& globalPos = globalPositions[key];
        const auto& localEntry = localPos.entry;
        auto& globalEntry = globalPos.entry;

        // Check if adding this local entry would overflow
        uint64_t globalTotal = globalEntry.total();
        uint64_t localTotal = localEntry.total();

        // If adding would exceed max, skip this entry to preserve ratios
        if (globalTotal + localTotal > BookEntry::kMaxResultCount) {
            droppedOverflow += localTotal;
            continue;
        }

        // Add counts - we already verified no overflow
        globalEntry.white += localEntry.white;
        globalEntry.draw += localEntry.draw;
        globalEntry.black += localEntry.black;

        // Merge opening information
        if (localPos.ply() && localPos.ply() <= globalPos.ply())
            globalPos.opening = merge(globalPos.opening, localPos.opening);

        // Store position reference if we have one and global doesn't yet
        if (localPos.ref && !globalPos.ref) globalPos.ref = localPos.ref;
    }

    return droppedOverflow;
}

/** Process a single chunk into local maps (no merging) */
void processChunk(int fd,
                  uint16_t fileIndex,
                  size_t startPos,
                  size_t endPos,
                  std::unordered_map<uint64_t, BookPosition>& localPositions,
                  BookStats& stats) {
    // Read entire chunk with single large read() for efficient I/O
    size_t chunkSize = endPos - startPos;
    std::string pgnData(chunkSize, '\0');
    ssize_t bytesRead = pread(fd, &pgnData[0], chunkSize, startPos);
    if (bytesRead != static_cast<ssize_t>(chunkSize)) return;

    std::istringstream stream{std::move(pgnData)};
    size_t currentOffset = startPos;
    size_t gamesInChunk = 0;
    constexpr size_t kMaxGamesPerChunk = 100000;  // Safety limit

    while (stream && !stream.eof() && gamesInChunk < kMaxGamesPerChunk) {
        size_t gameStartOffset = currentOffset + stream.tellg();
        auto game = pgn::readPGN(stream);
        if (!game) break;  // No more games (empty movetext)

        ++stats.gamesProcessed;
        ++gamesInChunk;

        if (!shouldIncludeGame(game, stats)) continue;

        auto verifiedGame = pgn::verify(game);
        PositionRef gameRef = {gameStartOffset, fileIndex, 0};
        insertGame(verifiedGame, localPositions, gameRef, Opening(game));
        ++stats.gamesAccepted;
    }
}

/** Open a file and return its file descriptor and size */
static std::pair<int, off_t> openFileAndGetSize(const std::string& pgnFile) {
    int fd = open(pgnFile.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Could not open PGN file: " << pgnFile << "\n";
        return {-1, 0};
    }

    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize == -1) {
        std::cerr << "Could not get size of PGN file: " << pgnFile << "\n";
        close(fd);
        return {-1, 0};
    }

    return {fd, fileSize};
}

/** Calculate adaptive chunk size based on file size and thread count */
static size_t calculateChunkSize(off_t fileSize, unsigned int numThreads) {
    // Target: 10-20x number of cores (e.g., 160-320 chunks for 16 cores)
    size_t targetChunks = numThreads * 15;  // 15x cores as sweet spot
    size_t adaptiveChunkSize =
        std::max(kChunkSize, (static_cast<size_t>(fileSize) + targetChunks - 1) / targetChunks);
    return std::min(adaptiveChunkSize, kChunkSize);
}

/** Split file into chunks at game boundaries */
static std::vector<std::pair<size_t, size_t>> createChunkBoundaries(int fd,
                                                                    off_t fileSize,
                                                                    size_t chunkSize) {
    std::vector<std::pair<size_t, size_t>> chunks;
    size_t chunkStart = 0;
    constexpr size_t kScanBufferSize = 8192;
    std::vector<char> scanBuffer(kScanBufferSize);

    while (chunkStart < static_cast<size_t>(fileSize)) {
        size_t chunkEnd = std::min(chunkStart + chunkSize, static_cast<size_t>(fileSize));

        // Adjust chunk end to game boundary (except for last chunk)
        if (chunkEnd < static_cast<size_t>(fileSize)) {
            ssize_t scanSize = pread(fd, scanBuffer.data(), kScanBufferSize, chunkEnd);
            if (scanSize > 0) {
                bool foundEmptyLine = false;
                for (ssize_t i = 0; i < scanSize; ++i) {
                    if (scanBuffer[i] == '\n') {
                        if (foundEmptyLine && i + 1 < scanSize && scanBuffer[i + 1] == '[') {
                            chunkEnd += i + 1;
                            break;
                        }
                        if (i + 1 < scanSize && scanBuffer[i + 1] == '\n') {
                            foundEmptyLine = true;
                        }
                    } else if (scanBuffer[i] != '\r') {
                        foundEmptyLine = false;
                    }
                }
            }
        }

        chunks.push_back({chunkStart, chunkEnd});
        chunkStart = chunkEnd;
    }

    return chunks;
}

/** Process multiple chunks assigned to this thread, then merge once */
void processChunksForThread(int fd,
                            uint16_t fileIndex,
                            const std::vector<size_t>& chunkIndices,
                            const std::vector<std::pair<size_t, size_t>>& chunks,
                            std::unordered_map<uint64_t, BookPosition>& globalPositions,
                            std::mutex& mutex,
                            BookStats& stats,
                            std::atomic<size_t>& chunksProcessed,
                            size_t totalChunks) {
    // Thread-local map - process all chunks before merging
    std::unordered_map<uint64_t, BookPosition> localPositions;

    for (size_t i = 0; i < chunkIndices.size(); ++i) {
        size_t idx = chunkIndices[i];
        processChunk(fd, fileIndex, chunks[idx].first, chunks[idx].second, localPositions, stats);

        // Update progress counter and report every 100 chunks
        size_t completed = ++chunksProcessed;
        if (completed % 100 == 0) {
            double percent = (100.0 * completed) / totalChunks;
            std::cout << completed << "/" << totalChunks << " chunks processed (" << std::fixed
                      << std::setprecision(1) << percent << "%)\r" << std::flush;
        }
    }

    // Single merge at the end for all chunks processed by this thread
    uint64_t dropped = mergeEntries(localPositions, globalPositions, mutex);
    stats.droppedOverflow = dropped;
}

/** Process a PGN file with multi-threading support */
BookStats processPGNFile(const std::string& pgnFile,
                         uint16_t fileIndex,
                         std::unordered_map<uint64_t, BookPosition>& positions) {
    auto [fd, fileSize] = openFileAndGetSize(pgnFile);
    if (fd == -1) return {};

    unsigned int numThreads = std::thread::hardware_concurrency() + 4;
    size_t adaptiveChunkSize = calculateChunkSize(fileSize, numThreads);
    auto chunks = createChunkBoundaries(fd, fileSize, adaptiveChunkSize);

    std::cout << "  Processing " << chunks.size() << " chunks with " << numThreads << " threads";
    if (adaptiveChunkSize > kChunkSize)
        std::cout << " (adaptive chunk size: " << adaptiveChunkSize / (1024 * 1024) << " MB)";

    std::cout << " (~" << (chunks.size() + numThreads - 1) / numThreads << " chunks/thread)\n";

    // Distribute chunks round-robin to threads for load balancing
    std::vector<std::vector<size_t>> threadChunks(numThreads);
    for (size_t i = 0; i < chunks.size(); ++i) threadChunks[i % numThreads].push_back(i);

    // Spawn threads and track statistics
    std::mutex mutex;
    std::atomic<size_t> chunksProcessed{0};
    std::vector<std::thread> threads;
    std::vector<BookStats> threadStats(numThreads);

    for (unsigned int t = 0; t < numThreads; ++t) {
        if (threadChunks[t].empty()) continue;
        threads.emplace_back(processChunksForThread,
                             fd,
                             fileIndex,
                             std::cref(threadChunks[t]),
                             std::cref(chunks),
                             std::ref(positions),
                             std::ref(mutex),
                             std::ref(threadStats[t]),
                             std::ref(chunksProcessed),
                             chunks.size());
    }

    // Join all threads
    for (auto& thread : threads) thread.join();

    // Print final 100% progress line if we printed any progress
    if (chunks.size() >= 100)
        std::cout << chunks.size() << "/" << chunks.size() << " chunks processed (100.0%)\n";

    // Aggregate stats
    BookStats totalStats;
    for (const auto& stats : threadStats) {
        totalStats.gamesProcessed += stats.gamesProcessed;
        totalStats.gamesAccepted += stats.gamesAccepted;
        totalStats.droppedLowElo += stats.droppedLowElo;
        totalStats.droppedShortTime += stats.droppedShortTime;
        totalStats.droppedVariants += stats.droppedVariants;
        totalStats.droppedOverflow += stats.droppedOverflow;
    }

    close(fd);
    return totalStats;
}

/** Reconstruct FEN from a position reference by replaying the game */
std::string reconstructFEN(const PositionRef& ref, const std::vector<std::string>& pgnFiles) {
    if (ref.fileIndex >= pgnFiles.size()) return "";

    std::ifstream in(pgnFiles[ref.fileIndex], std::ios::binary);
    if (!in || !in.is_open()) return "";

    // Seek to the game start
    in.seekg(ref.offset);

    // Read and parse the game
    auto game = pgn::readPGN(in);
    auto verifiedGame = pgn::verify(game);
    auto&& [moves, term] = verifiedGame;

    // Replay moves up to the target ply
    Position position = Position::initial();
    for (uint16_t ply = 0; ply < ref.ply && ply < moves.size(); ++ply)
        position = moves::applyMove(position, moves[ply]);

    return fen::to_string(position);
}

/** Reconstruct FENs for a batch of positions (thread worker) */
void reconstructFENBatch(const std::vector<std::pair<uint64_t, PositionRef>>& batch,
                         const std::vector<std::string>& pgnFiles,
                         std::unordered_map<uint64_t, std::string>& fens) {
    for (const auto& [key, ref] : batch) {
        std::string fen = reconstructFEN(ref, pgnFiles);
        if (!fen.empty()) {
            fens[key] = std::move(fen);
        }
    }
}

/** Write book entries to a CSV file */
size_t writeBookCSV(const std::string& csvFile,
                    const std::unordered_map<uint64_t, BookPosition>& positions,
                    const std::vector<std::string>& pgnFiles) {
    std::ofstream out(csvFile);
    if (!out || !out.is_open()) {
        std::cerr << "Could not open output file: " << csvFile << "\n";
        return 0;
    }

    // Collect positions that need FEN reconstruction
    std::vector<std::pair<uint64_t, PositionRef>> toReconstruct;
    for (const auto& [key, pos] : positions)
        if (pos.entry.total() >= kMinGames && pos.ref) toReconstruct.push_back({key, pos.ref});

    std::cout << "Reconstructing " << toReconstruct.size() << " FENs...\n";

    // Reconstruct FENs in parallel
    unsigned int numThreads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::vector<std::unordered_map<uint64_t, std::string>> threadFENs(numThreads);

    // Distribute positions round-robin to threads
    size_t positionsPerThread = (toReconstruct.size() + numThreads - 1) / numThreads;
    for (unsigned int t = 0; t < numThreads; ++t) {
        size_t start = t * positionsPerThread;
        size_t end = std::min(start + positionsPerThread, toReconstruct.size());
        if (start >= toReconstruct.size()) break;

        std::vector<std::pair<uint64_t, PositionRef>> batch(toReconstruct.begin() + start,
                                                            toReconstruct.begin() + end);
        threads.emplace_back(
            reconstructFENBatch, std::move(batch), std::cref(pgnFiles), std::ref(threadFENs[t]));
    }

    // Wait for all threads
    for (auto& thread : threads) thread.join();

    // Merge FENs from all threads
    std::unordered_map<uint64_t, std::string> allFENs;
    for (const auto& threadMap : threadFENs) allFENs.insert(threadMap.begin(), threadMap.end());

    // Write to CSV
    out << "eco,name,fen,white,draw,black\n";
    size_t writtenCount = 0;

    // Sort positions by ECO code and name for better readability
    std::vector<std::pair<uint64_t, BookPosition>> sortedPositions(positions.begin(),
                                                                   positions.end());
    std::sort(sortedPositions.begin(), sortedPositions.end(), [](const auto& a, const auto& b) {
        const auto& posA = a.second;
        const auto& posB = b.second;
        if (posA.opening.eco.min != posB.opening.eco.min)
            return posA.opening.eco.min < posB.opening.eco.min;
        return posA.opening.name < posB.opening.name;
    });

    for (const auto& [key, pos] : sortedPositions) {
        const auto& entry = pos.entry;
        if (entry.total() >= kMinGames && pos.ref) {
            auto it = allFENs.find(key);
            if (it != allFENs.end()) {
                // Quote the name field to handle commas in opening names
                std::string quotedName = "\"" + pos.opening.name + "\"";
                out << std::string(pos.opening.eco) << "," << quotedName << "," << it->second << ","
                    << entry.white << "," << entry.draw << "," << entry.black << "\n";
                ++writtenCount;
            }
        }
    }

    return writtenCount;
}
/** Process multiple PGN files and aggregate statistics */
BookStats processPGNFiles(const std::vector<std::string>& pgnFiles,
                          std::unordered_map<uint64_t, BookPosition>& positions) {
    BookStats totalStats;

    for (size_t i = 0; i < pgnFiles.size(); ++i) {
        std::cout << "Processing " << pgnFiles[i] << "...\n";

        BookStats stats = processPGNFile(pgnFiles[i], i, positions);
        totalStats.gamesProcessed += stats.gamesProcessed;
        totalStats.gamesAccepted += stats.gamesAccepted;
        totalStats.droppedLowElo += stats.droppedLowElo;
        totalStats.droppedShortTime += stats.droppedShortTime;
        totalStats.droppedVariants += stats.droppedVariants;
        totalStats.droppedOverflow += stats.droppedOverflow;

        double acceptPercent =
            stats.gamesProcessed > 0 ? (100.0 * stats.gamesAccepted) / stats.gamesProcessed : 0.0;
        std::cout << "  " << stats.gamesAccepted << " games accepted out of "
                  << stats.gamesProcessed << " total (" << std::fixed << std::setprecision(3)
                  << acceptPercent << "%)\n";
    }

    return totalStats;
}

/** Print a drop statistic with percentage if non-zero */
void printDropStat(const char* label, uint64_t count, uint64_t total) {
    if (!count) return;
    double pct = (100.0 * count) / total;
    std::cout << "  " << label << ": " << count << " (" << std::fixed << std::setprecision(1) << pct
              << "%)\n";
}

/** Print dropped games breakdown by reason */
void printDroppedGames(const BookStats& stats) {
    std::cout << "Dropped " << stats.totalDropped() << " games:\n";
    printDropStat("Low ELO", stats.droppedLowElo, stats.gamesProcessed);
    printDropStat("Short time", stats.droppedShortTime, stats.gamesProcessed);
    printDropStat("Variants/FEN", stats.droppedVariants, stats.gamesProcessed);
    printDropStat("Overflow", stats.droppedOverflow, stats.gamesProcessed);
}

/** Print summary statistics including drop reasons */
void printSummaryStats(const BookStats& stats, size_t uniquePositions) {
    double totalAcceptPercent =
        stats.gamesProcessed > 0 ? (100.0 * stats.gamesAccepted) / stats.gamesProcessed : 0.0;
    std::cout << "\nTotal: " << stats.gamesAccepted << " games out of " << stats.gamesProcessed
              << " total (" << std::fixed << std::setprecision(3) << totalAcceptPercent << "%)\n";

    if (stats.totalDropped()) printDroppedGames(stats);
    std::cout << "Found " << uniquePositions << " unique positions\n";
}
}  // namespace

/** Generate a book CSV file from one or more PGN files */
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input1.pgn> [input2.pgn ...] <output.csv>\n";
        return 1;
    }

    // Parse arguments
    std::vector<std::string> pgnFiles;
    for (int i = 1; i < argc - 1; ++i) pgnFiles.push_back(argv[i]);
    const char* csvFile = argv[argc - 1];

    // Process all PGN files
    std::unordered_map<uint64_t, BookPosition> positions;
    BookStats totalStats = processPGNFiles(pgnFiles, positions);

    // Print summary
    printSummaryStats(totalStats, positions.size());

    // Write output
    size_t writtenCount = writeBookCSV(csvFile, positions, pgnFiles);
    std::cout << "Wrote " << writtenCount << " positions with at least " << kMinGames
              << " games to " << csvFile << "\n";

    return writtenCount > 0 ? 0 : 1;
}
