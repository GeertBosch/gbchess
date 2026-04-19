#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>

#include "book/eco.h"
#include "book/pgn/pgn.h"
#include "core/core.h"
#include "core/hash/hash.h"
#include "engine/fen/fen.h"
#include "move/move.h"

namespace {

// Minimum game time in seconds (excludes bullet and blitz games)
static constexpr int kMinGameTimeSeconds = 180;
static constexpr int kMaxOpeningMoves = 12;
// Inclusion of a game depends on the average ELO of the players, but if there is a large difference
// the game ELO is capped to that of the weakest player plus half of the max spread. This reflects
// the expectation that a game between to slightly differently rated players may still be a strong
// game and is a balance between exclusing too many games and including games of lower quality.
static constexpr int kMinElo = 2300;
static constexpr int kMaxEloSpread = 200;
static constexpr int kMinGames = 25;

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

/** Name and ECO code of an opening */
struct Opening {
    Opening() = default;
    Opening(std::string name, std::string eco, int plies)
        : name(std::move(name)), eco(eco), plies(plies) {}
    Opening(pgn::PGN const& game)
        : Opening(game["Opening"], game["ECO"], std::distance(game.begin(), game.end())) {}

    operator bool() const { return plies; }
    std::string name;  // Like "Grünfeld Defense: Exchange Variation"
    ECO eco;
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

bool isTrailingPunctuation(char c) {
    return c == ' ' || c == ':' || c == '-' || c == ',';
}

ECO merge(ECO a, ECO b) {
    if (!a) return b;
    if (!b) return a;

    return std::min(a, b);
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

struct BookEntry {
    static constexpr size_t kCountBits = 20;
    static constexpr uint64_t kMaxResultCount = (1ull << kCountBits) - 1;
    uint64_t white : kCountBits;  // White wins
    uint64_t draw : kCountBits;   // Draws
    uint64_t black : kCountBits;  // Black wins

    uint64_t add(pgn::Termination term);  // Accepts: WHITE_WIN, DRAW, BLACK_WIN
    bool full() const {
        return std::max({uint64_t(white), uint64_t(draw), uint64_t(black)}) == kMaxResultCount;
    }
    uint64_t total() const { return white + draw + black; }  // May be up to 3 * kMaxResultCount
};

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
static int parsePositiveInt(std::string str) {
    if (str.empty()) return 0;

    // Skip leading '+' sign if any
    if (str[0] == '+') str = str.substr(1);

    // Avoid overflow by rejecting input over 9 digits
    if (str.length() > 9) return 0;

    int value = 0;
    for (auto digit : str)
        if (digit < '0' || digit > '9')
            return 0;
        else
            value = value * 10 + (digit - '0');

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

/** Reconstruct FEN strings for positions that meet book output criteria */
std::unordered_map<uint64_t, std::string> reconstructFENs(
    const std::unordered_map<uint64_t, BookPosition>& positions,
    const std::vector<std::string>& pgnFiles) {
    std::vector<std::pair<uint64_t, PositionRef>> toReconstruct;
    for (const auto& [key, pos] : positions)
        if (pos.entry.total() >= kMinGames && pos.ref) toReconstruct.push_back({key, pos.ref});

    std::cout << "Reconstructing " << toReconstruct.size() << " FENs...\n";

    unsigned int numThreads = std::thread::hardware_concurrency();
    if (!numThreads) numThreads = 1;

    std::vector<std::thread> threads;
    std::vector<std::unordered_map<uint64_t, std::string>> threadFENs(numThreads);

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

    for (auto& thread : threads) thread.join();

    std::unordered_map<uint64_t, std::string> allFENs;
    for (const auto& threadMap : threadFENs) allFENs.insert(threadMap.begin(), threadMap.end());

    return allFENs;
}

/** Sort positions by opening ECO and name for deterministic output */
std::vector<std::pair<uint64_t, BookPosition>> sortPositions(
    const std::unordered_map<uint64_t, BookPosition>& positions) {
    std::vector<std::pair<uint64_t, BookPosition>> sortedPositions(positions.begin(),
                                                                   positions.end());
    std::sort(sortedPositions.begin(), sortedPositions.end(), [](const auto& a, const auto& b) {
        const auto& posA = a.second;
        const auto& posB = b.second;
        if (posA.opening.eco != posB.opening.eco) return posA.opening.eco < posB.opening.eco;
        return posA.opening.name < posB.opening.name;
    });

    return sortedPositions;
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

/** Write book entries to a CSV file */
size_t writeBookCSV(const std::string& csvFile,
                    const std::unordered_map<uint64_t, BookPosition>& positions,
                    const std::vector<std::string>& pgnFiles) {
    std::ofstream out(csvFile);
    if (!out || !out.is_open()) {
        std::cerr << "Could not open output file: " << csvFile << "\n";
        return 0;
    }

    auto allFENs = reconstructFENs(positions, pgnFiles);

    // Write to CSV
    out << "eco,name,fen,white,draw,black\n";
    size_t writtenCount = 0;

    // Sort positions by ECO code and name for better readability
    auto sortedPositions = sortPositions(positions);

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

/** Write book entries to an EPD file for fastchess opening book use */
size_t writeBookEPD(const std::string& epdFile,
                    const std::unordered_map<uint64_t, BookPosition>& positions,
                    const std::vector<std::string>& pgnFiles) {
    std::ofstream out(epdFile);
    if (!out || !out.is_open()) {
        std::cerr << "Could not open output file: " << epdFile << "\n";
        return 0;
    }

    auto allFENs = reconstructFENs(positions, pgnFiles);
    auto sortedPositions = sortPositions(positions);

    size_t writtenCount = 0;
    for (const auto& [key, pos] : sortedPositions) {
        const auto& entry = pos.entry;
        if (entry.total() < kMinGames || !pos.ref) continue;

        auto it = allFENs.find(key);
        if (it == allFENs.end()) continue;

        std::string epdPosition = fenToEPD(it->second);
        if (epdPosition.empty()) continue;

        out << epdPosition << " id \"" << escapeEPDString(pos.opening.name) << "\";"
            << " eco \"" << std::string(pos.opening.eco) << "\";"
            << " c0 \"" << entry.white << "," << entry.draw << "," << entry.black << "\";"
            << "\n";
        ++writtenCount;
    }

    return writtenCount;
}

/** Print CLI usage for book generation */
void usage(const char* cmd) {
    std::cerr << "Usage: " << cmd << "<input1.pgn> [input2.pgn ...] <output.{csv|epd} ...>\n";
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

bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
        !str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

}  // namespace

/** Generate a book CSV or EPD file from one or more PGN files */
int main(int argc, char** argv) {
    // Non-empty means output that format to the specified file.
    std::string outputCSV;
    std::string outputEPD;

    // Check if the last argument is an output file with supported extension
    while (argc > 1) {
        if (endsWith(argv[argc - 1], ".csv") && outputCSV.empty())
            outputCSV = argv[argc - 1];
        else if (endsWith(argv[argc - 1], ".epd") && outputEPD.empty())
            outputEPD = argv[argc - 1];
        else
            break;
        --argc;  // Exclude output file from PGN files}
    }

    // Parse remaining arguments, which should be PGN files
    std::vector<std::string> pgnFiles;
    for (int i = 1; i < argc; ++i) pgnFiles.push_back(argv[i]);

    // Remaining arguments should be PGN files
    if (pgnFiles.empty() || (outputCSV.empty() && outputEPD.empty())) return usage(argv[0]), 1;

    // Process all PGN files
    std::unordered_map<uint64_t, BookPosition> positions;
    BookStats totalStats = processPGNFiles(pgnFiles, positions);

    // Print summary
    printSummaryStats(totalStats, positions.size());

    // Write output files
    if (auto countCSV = outputCSV.empty() ? 0 : writeBookCSV(outputCSV, positions, pgnFiles))
        std::cout << "Wrote " << countCSV << " positions to " << outputCSV << "\n";
    if (auto countEPD = outputEPD.empty() ? 0 : writeBookEPD(outputEPD, positions, pgnFiles))
        std::cout << "Wrote " << countEPD << " positions to " << outputEPD << "\n";

    // Success if we found any positions
    return positions.empty();
}
