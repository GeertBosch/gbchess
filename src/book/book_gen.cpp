#include "book/book.h"
#include "book/pgn/pgn.h"
#include "core/core.h"
#include "core/hash/hash.h"
#include "engine/fen/fen.h"
#include "move/move.h"

#include <fcntl.h>
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

// Minimum game time in seconds (excludes bullet and blitz games)
static constexpr int kMinGameTimeSeconds = 180;
static constexpr int kMaxOpeningMoves = 12;
static constexpr int kMinElo = 2400;

// Chunk size for parallel processing ( MB)
static constexpr size_t kChunkSize = 8 * 1024 * 1024;

/** Reference to a position in a PGN file for lazy FEN generation */
struct PositionRef {
    uint16_t fileIndex;  // Index into the list of PGN files
    uint64_t offset;     // Byte offset in the file where game starts
    uint16_t ply;        // Ply number (half-moves from start)
};

/** Combined book entry with game statistics and position reference */
struct BookPosition {
    BookEntry entry;
    PositionRef ref = {};
    bool hasRef = false;  // Track whether ref is valid
};

struct BookStats {
    uint64_t gamesProcessed = 0;
    uint64_t gamesAccepted = 0;
    uint64_t gamesDropped = 0;  // Games dropped due to overflow
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
bool shouldIncludeGame(const pgn::PGN& game) {
    auto whiteElo = game["WhiteElo"];
    auto blackElo = game["BlackElo"];

    if (parsePositiveInt(whiteElo) < kMinElo) return false;
    if (parsePositiveInt(blackElo) < kMinElo) return false;

    // Parse TimeControl format: "base+increment" (in seconds)
    auto timeControl = game["TimeControl"];
    if (timeControl.empty() || timeControl == "?" || timeControl == "-") return false;

    // Either no increment, or base + increment, all assumed in seconds
    size_t plusPos = timeControl.find('+');
    if (plusPos == std::string::npos) return parsePositiveInt(timeControl) >= kMinGameTimeSeconds;

    // For 40 moves, effective time = base + 40 * increment
    auto base = parsePositiveInt(timeControl.substr(0, plusPos));
    auto increment = parsePositiveInt(timeControl.substr(plusPos + 1));
    return base + 40 * increment >= kMinGameTimeSeconds;
}

/** Add a game to local book entries (no locking needed) */
void insertGame(const pgn::VerifiedGame& verifiedGame,
                std::unordered_map<uint64_t, BookPosition>& positions,
                const PositionRef& gameRef) {
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
        if (!pos.hasRef) {
            pos.ref = {gameRef.fileIndex, gameRef.offset, ply};
            pos.hasRef = true;
        }
        if (position.turn.fullmove() >= kMaxOpeningMoves) break;  // Only index opening moves
    }
}

/** Merge local entries into global entries (with mutex), returns count of dropped games */
uint64_t mergeEntries(std::unordered_map<uint64_t, BookPosition>& localPositions,
                      std::unordered_map<uint64_t, BookPosition>& globalPositions,
                      std::mutex& mutex) {
    std::lock_guard<std::mutex> lock(mutex);
    uint64_t droppedGames = 0;

    for (const auto& [key, localPos] : localPositions) {
        auto& globalPos = globalPositions[key];
        const auto& localEntry = localPos.entry;
        auto& globalEntry = globalPos.entry;

        // Check if adding this local entry would overflow
        uint64_t globalTotal = globalEntry.total();
        uint64_t localTotal = localEntry.total();

        // If adding would exceed max, skip this entry to preserve ratios
        if (globalTotal + localTotal > BookEntry::kMaxResultCount) {
            droppedGames += localTotal;
            continue;
        }

        // Add counts - we already verified no overflow
        globalEntry.white += localEntry.white;
        globalEntry.draw += localEntry.draw;
        globalEntry.black += localEntry.black;

        // Store position reference if we have one and global doesn't yet
        if (localPos.hasRef && !globalPos.hasRef) {
            globalPos.ref = localPos.ref;
            globalPos.hasRef = true;
        }
    }

    return droppedGames;
}

/** Find the start of the next game after the given file position */
size_t findNextGameStart(std::ifstream& file, size_t startPos) {
    file.seekg(startPos);
    std::string line;
    bool foundEmptyLine = false;

    while (std::getline(file, line)) {
        if (line.empty() || line == "\r") {
            foundEmptyLine = true;
        } else if (foundEmptyLine && !line.empty() && line[0] == '[') {
            // Found empty line followed by tag - this is the start of a game
            size_t currentPos = file.tellg();
            return currentPos - line.length() - 1;
        } else if (!line.empty() && line[0] != '[') {
            foundEmptyLine = false;
        }
    }

    return static_cast<size_t>(file.tellg());
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

        if (!shouldIncludeGame(game)) continue;

        auto verifiedGame = pgn::verify(game);
        PositionRef gameRef = {fileIndex, gameStartOffset, 0};
        insertGame(verifiedGame, localPositions, gameRef);
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
    stats.gamesDropped = dropped;
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
    if (adaptiveChunkSize > kChunkSize) {
        std::cout << " (adaptive chunk size: " << adaptiveChunkSize / (1024 * 1024) << " MB)";
    }
    std::cout << " (~" << (chunks.size() + numThreads - 1) / numThreads << " chunks/thread)\n";

    // Distribute chunks round-robin to threads for load balancing
    std::vector<std::vector<size_t>> threadChunks(numThreads);
    for (size_t i = 0; i < chunks.size(); ++i) {
        threadChunks[i % numThreads].push_back(i);
    }

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
    for (auto& thread : threads) {
        thread.join();
    }

    // Print final 100% progress line if we printed any progress
    if (chunks.size() >= 100) {
        std::cout << chunks.size() << "/" << chunks.size() << " chunks processed (100.0%)\n";
    }

    // Aggregate stats
    BookStats totalStats;
    for (const auto& stats : threadStats) {
        totalStats.gamesProcessed += stats.gamesProcessed;
        totalStats.gamesAccepted += stats.gamesAccepted;
        totalStats.gamesDropped += stats.gamesDropped;
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
    for (uint16_t ply = 0; ply < ref.ply && ply < moves.size(); ++ply) {
        position = moves::applyMove(position, moves[ply]);
    }

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
    for (const auto& [key, pos] : positions) {
        if (pos.entry.total() >= kMinGames && pos.hasRef) {
            toReconstruct.push_back({key, pos.ref});
        }
    }

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
    for (auto& thread : threads) {
        thread.join();
    }

    // Merge FENs from all threads
    std::unordered_map<uint64_t, std::string> allFENs;
    for (const auto& threadMap : threadFENs) {
        allFENs.insert(threadMap.begin(), threadMap.end());
    }

    // Write to CSV
    out << "fen,white,draw,black\n";
    size_t writtenCount = 0;

    for (const auto& [key, pos] : positions) {
        const auto& entry = pos.entry;
        if (entry.total() >= kMinGames && pos.hasRef) {
            auto it = allFENs.find(key);
            if (it != allFENs.end()) {
                out << it->second << "," << entry.white << "," << entry.draw << "," << entry.black
                    << "\n";
                ++writtenCount;
            }
        }
    }

    return writtenCount;
}

/** Generate a book CSV file from one or more PGN files */
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input1.pgn> [input2.pgn ...] <output.csv>\n";
        return 1;
    }

    const char* csvFile = argv[argc - 1];
    std::unordered_map<uint64_t, BookPosition> positions;
    std::vector<std::string> pgnFiles;

    uint64_t totalGamesProcessed = 0;
    uint64_t totalGamesAccepted = 0;
    uint64_t totalGamesDropped = 0;

    // Process each PGN file
    for (int i = 1; i < argc - 1; ++i) {
        const char* pgnFile = argv[i];
        pgnFiles.push_back(pgnFile);
        std::cout << "Processing " << pgnFile << "...\n";

        BookStats stats = processPGNFile(pgnFile, i - 1, positions);
        totalGamesProcessed += stats.gamesProcessed;
        totalGamesAccepted += stats.gamesAccepted;
        totalGamesDropped += stats.gamesDropped;

        double acceptPercent =
            stats.gamesProcessed > 0 ? (100.0 * stats.gamesAccepted) / stats.gamesProcessed : 0.0;
        std::cout << "  " << stats.gamesAccepted << " games accepted out of "
                  << stats.gamesProcessed << " total (" << std::fixed << std::setprecision(3)
                  << acceptPercent << "%)\n";
    }

    double totalAcceptPercent =
        totalGamesProcessed > 0 ? (100.0 * totalGamesAccepted) / totalGamesProcessed : 0.0;
    std::cout << "\nTotal: " << totalGamesAccepted << " games out of " << totalGamesProcessed
              << " total (" << std::fixed << std::setprecision(3) << totalAcceptPercent << "%)\n";
    if (totalGamesDropped > 0) {
        std::cout << "Warning: " << totalGamesDropped
                  << " games dropped due to position count overflow\n";
    }
    std::cout << "Found " << positions.size() << " unique positions\n";

    size_t writtenCount = writeBookCSV(csvFile, positions, pgnFiles);
    std::cout << "Wrote " << writtenCount << " positions with at least " << kMinGames
              << " games to " << csvFile << "\n";

    return writtenCount > 0 ? 0 : 1;
}
