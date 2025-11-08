#include "perft_core.h"
#include "hash.h"
#include "moves.h"
#include "moves_gen.h"
#include "moves_table.h"
#include "options.h"
#include <atomic>
#include <iostream>
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
    mutable std::mutex tableMutex;

    HashTable() {
        table.clear();
        table.resize(kHashTableSize, Entry{});
    }

    const Entry* lookup(HashValue hash, int level) const {
        std::lock_guard<std::mutex> lock(tableMutex);
        Key key = makeKey(hash, level);
        size_t idx = static_cast<size_t>(key % kHashTableSize);
        auto& entry = table[idx];
        return entry.key() == key ? &entry : nullptr;
    }

    void enter(HashValue hash, int level, Value value) {
        std::lock_guard<std::mutex> lock(tableMutex);
        Key key = makeKey(hash, level);
        size_t idx = static_cast<size_t>(key % kHashTableSize);
        auto& entry = table[idx];
        dassert(entry.key() != key || entry.value() == value);
        entry = {key, value};
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
        if (auto count = lookup2(hash())) return perftCached.fetch_add(count), count;

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

        nodes += countLegalMovesAndCaptures(board, newState);
    });
    if (options::cachePerft && nodes > 100) {
        enter2(hash(), nodes);
        assert(lookup2(hash()) == nodes);
    }

    return nodes;
}

NodeCount perft(Board& board, Hash hash, moves::SearchState state, int depth) {
    // if (depth <= 1) return depth == 1 ? countLegalMovesAndCaptures(board, state) : 1;
    if (depth == 2) return perft2(board, hash, state);

    dassert(!options::cachePerft || hash == Hash(Position{board, state.turn}));

    // Unlike normal Zobrist hashing, we need to include the level.

    if constexpr (options::cachePerft)
        if (auto entry = perftHashTable.lookup(hash(), depth))
            return perftCached.fetch_add(entry->value()), entry->value();

    NodeCount nodes = 0;
    auto newState = state;
    forAllLegalMovesAndCaptures(board, state, [&](Board& board, MoveWithPieces mwp) {
        auto delta = MovesTable::occupancyDelta(mwp.move);
        auto newHash = options::cachePerft ? applyMove(hash, state.turn, mwp) : Hash();
        newState.occupancy = !(state.occupancy ^ delta);
        newState.pawns = find(board, addColor(PieceType::PAWN, !state.active()));
        newState.turn = moves::applyMove(state.turn, mwp);
        newState.kingSquare = *find(board, addColor(PieceType::KING, !state.active())).begin();
        newState.inCheck = moves::isAttacked(board, newState.kingSquare, newState.occupancy);
        newState.pinned = moves::pinnedPieces(board, newState.occupancy, newState.kingSquare);

        auto newNodes = nodes + perft(board, newHash, newState, depth - 1);
        dassert(newNodes >= nodes);  // Check for node count overflow
        nodes = newNodes;
    });
    if (options::cachePerft && nodes > 200) perftHashTable.enter(hash(), depth, nodes);
    return nodes;
}

struct PerftTask {
    Position position;
    int depth;

    PerftTask() = default;
    PerftTask(Position pos, int d) : position(pos), depth(d) {}
};

using TaskList = std::vector<PerftTask>;

TaskList expandTasks(TaskList tasks) {
    while (tasks.size() < 100) {
        TaskList expanded;

        for (auto task : tasks) {
            auto moves = moves::allLegalMovesAndCaptures(task.position.turn, task.position.board);
            if (moves.size() < 2 || task.depth < 5)
                expanded.push_back(task);
            else
                for (auto move : moves)
                    expanded.emplace_back(moves::applyMove(task.position, move), task.depth - 1);
        }

        if (expanded.size() == tasks.size()) break;
        tasks = expanded;
    };
    return tasks;
}

NodeCount threaded_perft(Position position, int depth) {
    NodeCount nodes = 0;
    TaskList tasks;
    tasks.emplace_back(PerftTask{position, depth});
    tasks = expandTasks(tasks);

    std::mutex nodesMutex;
    std::atomic<size_t> taskIndex{0};

    // Create a bounded number of threads to process the tasks
    const auto numThreads = std::max<unsigned int>(4, std::thread::hardware_concurrency());
    std::cout << "Starting " << tasks.size() << " tasks for depth " << depth;
    std::cout << " using " << numThreads << " threads\n";
    std::vector<std::thread> threads;

    for (unsigned int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&]() {
            NodeCount localNodes = 0;

            while (true) {
                size_t idx = taskIndex.fetch_add(1, std::memory_order_relaxed);
                if (idx >= tasks.size()) break;
                PerftTask task = tasks[idx];
                auto state = moves::SearchState(task.position.board, task.position.turn);
                localNodes += perft(task.position.board, Hash(task.position), state, task.depth);
            }

            std::lock_guard<std::mutex> lock(nodesMutex);
            nodes += localNodes;
        });
    }

    for (auto& thread : threads) thread.join();
    return nodes;
}

NodeCount perft(Position position, int depth) {
    if (depth <= 1)
        return depth == 1 ? moves::allLegalMovesAndCaptures(position.turn, position.board).size()
                          : 1;
    if (depth > 5) return threaded_perft(position, depth);
    auto state = moves::SearchState(position.board, position.turn);
    return perft(position.board, Hash{position}, state, depth);
}

NodeCount getPerftCached() {
    return perftCached;
}