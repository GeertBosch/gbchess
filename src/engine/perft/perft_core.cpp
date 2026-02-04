#include "perft_core.h"
#include "core/options.h"
#include "core/hash/hash.h"
#include "move/move.h"
#include "move/move_gen.h"
#include "move/move_table.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

static constexpr size_t MB = 1ull << 20;

/**
 * Hash table for caching perft results to speed up computation
 */
struct HashTable {
    using Key = HashValue;
    using Value = NodeCount;

    static Key makeKey(HashValue hash, int level) {
        constexpr HashValue mixer = 0xd989bcacc137dcd5ull;
        return static_cast<Key>(hash ^ (mixer * level));
    }

    class Entry {
    public:
        Entry() = default;
        constexpr Entry(Key key, Value value) : _key(key), _value(value) {}
        HashValue key() const { return _key; }
        HashValue value() const { return _value; }

    private:
        Key _key = 0;
        Value _value = 0;
    };

    static constexpr size_t kHashTableSize =
        options::cachePerft ? options::cachePerftMB * MB / 2 / sizeof(Entry) : 1;

    // This becomes super hot with very deep perfts with close to 100% cache hits
    static constexpr size_t kNumMutexShards = 2048;  // Reduce contention
    mutable std::atomic_flag tableMutexes[kNumMutexShards] = {};

    HashTable() {
        table.clear();
        table.resize(kHashTableSize, Entry{});
    }

    const Entry* lookup(HashValue hash, int level) const {
        Key key = makeKey(hash, level);
        size_t idx = static_cast<size_t>(key % kHashTableSize);

        while (tableMutexes[idx % kNumMutexShards].test_and_set(std::memory_order_acquire)) {
            // Spin until lock is available
        }

        auto& entry = table[idx];
        bool found = entry.key() == key;
        const Entry* result = found ? &entry : nullptr;

        // Release the spinlock
        tableMutexes[idx % kNumMutexShards].clear(std::memory_order_release);

        return result;
    }

    void enter(HashValue hash, int level, Value value) {
        Key key = makeKey(hash, level);
        size_t idx = static_cast<size_t>(key % kHashTableSize);

        while (tableMutexes[idx % kNumMutexShards].test_and_set(std::memory_order_acquire)) {
            // Spin until lock is available
        }

        auto& entry = table[idx];
        dassert(entry.key() != key || entry.value() == value);
        entry = {key, value};

        tableMutexes[idx % kNumMutexShards].clear(std::memory_order_release);
    }

    std::vector<Entry> table;
};

static HashTable perftHashTable;
static std::atomic<NodeCount> perftCached{0};

// For perft depth 2 caching, we don't need many bits for the counts, as there can at most be
// about 218 * 218 = 47,524 moves. If the table is at least 64K entries, we only need
// 128 - 16 = 112 bits for the key and 16 bits for the counts.
static constexpr size_t kPerft2CacheSize = options::cachePerftMB * MB / 2 / sizeof(uint128_t);
static std::vector<std::atomic<uint128_t>> perft2cache(kPerft2CacheSize);

uint32_t lookup2(HashValue hash) {
    size_t idx = static_cast<size_t>(hash % kPerft2CacheSize);
    uint128_t key = hash >> 16;
    uint128_t entry = perft2cache[idx].load(std::memory_order_relaxed);
    return static_cast<HashValue>(entry >> 16) == key ? static_cast<uint32_t>(entry & 0xffff) : 0;
}

void enter2(HashValue hash, uint32_t count) {
    size_t idx = static_cast<size_t>(hash % kPerft2CacheSize);
    uint128_t entry = ((hash >> 16) << 16) | count;
    perft2cache[idx].store(entry, std::memory_order_relaxed);
}

/**
 * Specialize perft with 2 levels left. The bulk of the work is done here, so optimize this.
 */
NodeCount perft2(Board& board, Hash hash, moves::SearchState state) {
    dassert(!options::cachePerft || hash == Hash(Position{board, state.turn}));

    if constexpr (options::cachePerft)
        if (auto count = lookup2(hash())) return count;
    NodeCount nodes = 0;
    auto newState = state;
    auto pawn = addColor(PieceType::PAWN, !state.active());
    auto king = addColor(PieceType::KING, !state.active());
    auto initialPawns = find(board, pawn);
    newState.kingSquare = *find(board, king).begin();

    forAllLegalMovesAndCaptures(board, state, [&](Board& board, MoveWithPieces mwp) {
        auto delta = MovesTable::occupancyDelta(mwp.move);
        newState.occupancy = !(state.occupancy ^ delta);
        newState.turn = moves::applyMove(state.turn, mwp);

        newState.pawns = initialPawns - SquareSet{mwp.move.to};
        if (mwp.move.kind == MoveKind::En_Passant) newState.pawns = find(board, pawn);

        newState.inCheck = moves::isAttacked(board, newState.kingSquare, newState.occupancy);
        newState.pinned = moves::pinnedPieces(board, newState.occupancy, newState.kingSquare);

        auto moveNodes = countLegalMovesAndCaptures(board, newState);
        nodes += moveNodes;
    });
    if (options::cachePerft && nodes > 100) enter2(hash(), nodes);

    return nodes;
}

NodeCount perft(Board& board, Hash hash, moves::SearchState state, int depth) {
    assert(depth > 1);
    if (depth == 2) return perft2(board, hash, state);

    dassert(!options::cachePerft || hash == Hash(Position{board, state.turn}));

    // Unlike normal Zobrist hashing, we need to include the level.

    if constexpr (options::cachePerft)
        if (auto entry = perftHashTable.lookup(hash(), depth)) return entry->value();

    NodeCount nodes = 0;
    auto newState = state;
    forAllLegalMovesAndCaptures(board, state, [&](Board& board, MoveWithPieces mwp) {
        auto delta = MovesTable::occupancyDelta(mwp.move);
        auto mask = moves::castlingMask(mwp.move.from, mwp.move.to);

        auto newHash = options::cachePerft ? applyMove(hash, state.turn, mwp, mask) : Hash();
        newState.occupancy = !(state.occupancy ^ delta);
        newState.pawns = find(board, addColor(PieceType::PAWN, !state.active()));
        newState.turn = moves::applyMove(state.turn, mwp);
        newState.kingSquare = *find(board, addColor(PieceType::KING, !state.active())).begin();
        newState.inCheck = moves::isAttacked(board, newState.kingSquare, newState.occupancy);
        newState.pinned = moves::pinnedPieces(board, newState.occupancy, newState.kingSquare);

        auto moveNodes = perft(board, newHash, newState, depth - 1);
        auto newNodes = nodes + moveNodes;
        assert(newNodes >= nodes);  // Check for node count overflow
        nodes = newNodes;
    });
    if (options::cachePerft) perftHashTable.enter(hash(), depth, nodes);
    return nodes;
}

/**
 * Perft with progress callback at the end of the computation
 */
NodeCount perft(Board& board,
                Hash hash,
                moves::SearchState state,
                int depth,
                const ProgressCallback& callback) {
    auto nodes = perft(board, hash, state, depth);
    if (callback) callback(nodes);
    return nodes;
}

struct PerftTask {
    Position position;
    int depth = 0;

    PerftTask() = default;
    PerftTask(Position pos, int d) : position(pos), depth(d) {}
};

using TaskList = std::vector<PerftTask>;

void expandTask(TaskList& expanded, PerftTask task, MoveVector moves) {
    if (moves.size() < 2 || task.depth < 5)
        expanded.push_back(task);
    else
        for (auto move : moves)
            expanded.emplace_back(moves::applyMove(task.position, move), task.depth - 1);
}
TaskList expandTasks(TaskList tasks, size_t number) {
    while (tasks.size() < number) {
        TaskList expanded;

        for (auto task : tasks)
            expandTask(expanded,
                       task,
                       moves::allLegalMovesAndCaptures(task.position.turn, task.position.board));


        if (expanded.size() == tasks.size()) break;  // No further expansion possible
        tasks = expanded;
    };
    return tasks;
}

NodeCount threadedPerft(Position position, int depth, const ProgressCallback& callback) {
    std::atomic<NodeCount> nodes{0};
    TaskList tasks;
    tasks.emplace_back(PerftTask{position, depth});
    tasks = expandTasks(tasks, depth > 4 ? depth * depth * depth : 100);

    std::atomic<size_t> taskIndex{0};

    // Create a bounded number of threads to process the tasks
    const auto numThreads = std::max<unsigned int>(4, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    std::mutex progressMutex;
    std::condition_variable progressCondition;
    std::atomic<int> runningThreads{0};
    std::atomic<size_t> completedTasks{0};

    std::thread progressThread([&]() {
        struct Progress {
            uint128_t nodes;
            uint128_t cached;
        };
        Progress last = {nodes.load(), perftCached.load()};
        // Remember the time of the last update
        auto lastUpdate = std::chrono::high_resolution_clock::now();
        auto interval = std::chrono::milliseconds(options::perftProgressMillis);

        while (completedTasks.load() < tasks.size() && callback) {
            std::unique_lock<std::mutex> lock(progressMutex);
            Progress current = {nodes.load(), perftCached.load()};
            auto reportNodes = current.nodes;

            if (current.nodes == last.nodes)
                reportNodes += current.cached - last.cached;
            else
                last = current;

            reportNodes = std::max(reportNodes, last.nodes);  // Ensure monotonic increase

            callback(reportNodes);
            std::chrono::time_point currentTime = lastUpdate;
            do {
                progressCondition.wait_for(lock, interval);
                currentTime = std::chrono::high_resolution_clock::now();
            } while (runningThreads.load() && currentTime - lastUpdate < interval / 2);
            lastUpdate = currentTime;
        }
        // Ensure that the final update reflects all nodes
        if (callback) callback(nodes.load());
    });

    for (unsigned int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&]() {
            runningThreads.fetch_add(1);
            while (true) {
                size_t idx = taskIndex.fetch_add(1, std::memory_order_relaxed);
                if (idx >= tasks.size()) break;
                PerftTask task = tasks[idx];
                auto state = moves::SearchState(task.position.board, task.position.turn);
                nodes.fetch_add(perft(task.position.board, Hash(task.position), state, task.depth));
                completedTasks.fetch_add(1);
                progressCondition.notify_one();
            }
            runningThreads.fetch_sub(1);
        });
    }

    progressThread.join();
    for (auto& thread : threads) thread.join();
    return nodes;
}

NodeCount perft(Position position, int depth, const ProgressCallback& callback) {
    auto state = moves::SearchState(position.board, position.turn);
    if (depth <= 1) {
        NodeCount result = depth ? moves::countLegalMovesAndCaptures(position.board, state) : 1;
        if (callback) callback(result);
        return result;
    }

    if (depth <= 5) return perft(position.board, Hash{position}, state, depth, callback);

    // For deeper perfts, see if the first few plies have sufficient cardinality to merit threading.
    // For that we take the perft at depth 4 and estimate the apparent depth assuming an average of
    // 20 moves per ply.
    auto perft4 = std::max<NodeCount>(perft(position.board, Hash{position}, state, 4), 1);
    int apparentDepth = depth - 4 + static_cast<int>(std::log(perft4) / std::log(20.0));
    if (apparentDepth <= 5) return perft(position.board, Hash{position}, state, depth, callback);
    return threadedPerft(position, depth, callback);
}

NodeCount getPerftCached() {
    return perftCached;
}