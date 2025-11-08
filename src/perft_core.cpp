#include "perft_core.h"
#include "hash.h"
#include "moves.h"
#include "moves_gen.h"
#include "moves_table.h"
#include "options.h"
#include <vector>

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

    static constexpr size_t MB = 1ull << 20;
    static constexpr size_t hashTableMemory = options::cachePerftMB * MB;
    static constexpr size_t kHashTableSize =
        options::cachePerft ? hashTableMemory / sizeof(Entry) : 1;

    HashTable() {
        table.clear();
        table.resize(kHashTableSize, Entry{});
    }

    const Entry* lookup(HashValue hash, int level) const {
        Key key = makeKey(hash, level);
        size_t idx = static_cast<size_t>(key % kHashTableSize);
        auto& entry = table[idx];
        return entry.key() == key ? &entry : nullptr;
    }

    void enter(HashValue hash, int level, Value value) {
        Key key = makeKey(hash, level);
        size_t idx = static_cast<size_t>(key % kHashTableSize);
        auto& entry = table[idx];
        dassert(entry.key() != key || entry.value() == value);
        entry = {key, value};
    }

    std::vector<Entry> table;
};

static HashTable perftHashTable;
static NodeCount perftCached = 0;

NodeCount perft(Board& board, Hash hash, moves::SearchState& state, int depth) {
    if (depth <= 1) return depth == 1 ? countLegalMovesAndCaptures(board, state) : 1;

    dassert(!options::cachePerft || hash == Hash(Position{board, state.turn}));

    // Unlike normal Zobrist hashing, we need to include the level.

    if constexpr (options::cachePerft)
        if (auto entry = perftHashTable.lookup(hash(), depth))
            return perftCached += entry->value(), entry->value();

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

NodeCount perft(Position position, int depth) {
    auto state = moves::SearchState(position.board, position.turn);
    return perft(position.board, Hash{position}, state, depth);
}

NodeCount getPerftCached() {
    return perftCached;
}