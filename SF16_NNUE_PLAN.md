# Finishing the SF16.1 NNUE integration: phases 5 and up

Companion to the phase ladder recorded in `sf16-nnue-port-ladder`. Phases 1-4 are on master;
this plan covers everything from the layer stacks down to a network that actually plays.

## Where the ladder stands

| Phase | Commit | What landed |
|---|---|---|
| 1 | `bd4f91d` | File header + architecture hash |
| 2 | `be454eb` | Full parameter load of `nn-b1a57edbea57.nnue`, landing exactly at EOF |
| 3 | `3ca380a` | HalfKAv2_hm feature indexing, king buckets, horizontal mirroring |
| 4 | `fab95fd` | Fresh accumulation, the 2560-byte feature transform, PSQT |
| 5 | `8e5bd76` | The `nndump` oracle re-established at Stockfish tag `sf_16.1` |
| 6 | `6111394` | Layer stack propagation, every intermediate golden |
| 7 | `f0405d8` | Bucket selection, whole-network evaluation, centipawn scale |
| 8 | `d44864f` | Engine integration behind `UseSF16`, one `staticEval`, measured |
| 9 | this commit | Incremental accumulators, an accumulator stack under make/unmake |

`src/eval/nnue/sf16.h` today ends at `evaluate(...)`, and everything down to it is an **exact
oracle**: seven positions are verified bit-for-bit against a patched Stockfish 16.1, across all
eight buckets, from the transformed bytes through every layer intermediate to the final `Value`
and centipawn score. `sf16.cpp` is a complete, standalone, scalar SF16.1 evaluation, and as of
phase 8 the engine plays with it when `UseSF16` is set.

Not yet implemented, in dependency order: SIMD, strength validation.

The whole port keeps the file's **canonical row-major ordering**. Stockfish scrambles affine
weights at load time to suit its SIMD kernels; gbchess does not, so the scalar reference stays
debuggable. Phase 10 is the only place that may introduce a second, permuted layout, and only
alongside the canonical one.

---

## Phase 5 — Prerequisite: re-establish the oracle — **done**

Landed at `/Users/bosch/Stockfish-16.1` (tag `sf_16.1`, `e67cc97`), outside the gbchess tree so
`make clean` cannot reach it. `gbchess-oracle/README.md` there records how to rebuild and
regenerate; `gbchess-oracle/nndump.patch` is the uncommitted patch, and `dump-avx2.txt` /
`dump-scalar.txt` are the seven golden FENs dumped by both binaries — byte identical, so the
SIMD and scalar paths agree on every number. The feature transform half of those dumps
reproduces all seven committed phase-4 hashes, sums, boundary bytes and PSQT values across all
eight buckets (`gbchess-oracle/check.py` re-verifies this against `sf16_test.cpp`), and the
per-bucket psqt/positional pairs match Stockfish's own `eval` trace table. Phase 6 can start.

**One constant differs from what this plan assumed:** `NormalizeToPawnValue` is **356**, not 328,
and it lives in `uci.cpp` (not `uci.h`) at line 48. Everything else below is confirmed at the tag:
`WeightScaleBits = 6`, `OutputScale = 16`, ClippedReLU `clamp(x >> 6, 0, 127)`, SqrClippedReLU
`min(127, (x*x) >> 19)` (written `>> (2 * WeightScaleBits + 7)`), the forward-skip term
`fc_0_out[15] * (600 * 16) / (127 * 64)`, and the bucket `(pieceCount - 1) / 4`.

One detail the dump made concrete: `ac_sqr_0` and `ac_0` are both 16 wide, but only their first
15 outputs are used — `ac_0_out[0]` is memcpy'd over `ac_sqr_0_out[15]`, so `fc_1`'s input is
exactly the 30 values the plan describes, padded to 32 with zeros.

*Original notes, kept for the record:*

**The Stockfish 16.1 clone is gone from this machine.** `/Users/bosch/Stockfish` is the *SF12*
checkout (`42cea7e7`, "Instrumentation fo gbchess purposes"); there is no `sqr_clipped_relu.h`
or `affine_transform_sparse_input.h` anywhere under `~`. Every phase below needs golden data
that only a patched SF16.1 can produce, so this comes first, not when it is first missed.

Follow `sf161-golden-reference-recipe` verbatim; its gotchas each cost real time:

- clone at tag `sf_16.1` (commit `e67cc97`) into a directory that will not be reaped;
- build `ARCH=x86-64-avx2` (this Mac is an Intel i9-9900K, not Apple Silicon), plus a second
  `ARCH=general-64` binary to confirm the scalar and SIMD paths agree before trusting anything;
- SF16.1 loads *both* nets and exits if either is missing: symlink the Big net and download the
  Small net `nn-baff1ede1f90.nnue`;
- insert new functions **above** the `template<NetSize Net_Size>` line preceding
  `Value evaluate(...)`, not between it and the signature.

Extend the existing `ftdump` UCI command into an `nndump` that prints, for a given FEN and
bucket, every intermediate the phases below need:

1. `fc_0` output, all `l2 + 1 == 16` int32s (including the forward-skip output);
2. the 30-byte concatenation fed to `fc_1` (sqr-clipped half, then clipped half);
3. `fc_1` output (32 int32s) and its clipped bytes;
4. `fc_2` output (1 int32) and the assembled `positional` value;
5. the selected bucket, the PSQT term, and the final `Value`.

Keep the patch uncommitted and out of CI. Only the small expected values are committed, into
`sf16_test.cpp`, exactly as phase 4 did.

**Also confirm the constants at the tag rather than from memory.** The expected values, to be
checked in `nnue_common.h`, `layers/*.h`, `nnue_architecture.h` and `evaluate_nnue.cpp`:
`WeightScaleBits = 6`, `OutputScale = 16`, ClippedReLU `clamp(x >> 6, 0, 127)`, SqrClippedReLU
`min(127, (x*x) >> 19)`, the forward-skip term `fc_0_out[15] * (600 * 16) / (127 * 64)`, the
bucket `(pieceCount - 1) / 4`, and `NormalizeToPawnValue` in `uci.h` for the centipawn scale.
(It is in `uci.cpp`, and it is 356 — see above.)
If any differs, the plan's arithmetic changes, not its shape.

**Done when:** `nndump` prints all five groups for the seven phase-4 golden FENs, and the
`general-64` and `avx2` binaries agree on every number.

---

## Phase 6 — Layer stack propagation — **done**

All seven golden positions propagate bit-identically through all eight layer stacks, every
intermediate checked, not just the final int32. `sf16.h` gained `affineForward`, `clippedReLU`,
`sqrClippedReLU`, the `Propagation` trace and `propagate`; `kWeightScaleBits` and `kOutputScale`
are the two constants they need. The plan below is what was built, with three notes:

- **`propagate` takes an optional `Propagation*` trace** rather than being one function per
  intermediate. The traced and untraced calls are the same code — the trace only decides where
  the intermediates are written — so the golden test can pin every layer without a second
  implementation to keep in step.
- **The golden table is digests, not five thousand numbers.** Per position and bucket it commits
  an FNV-1a-64 hash and a sum of each of `fc0`, `fc1Input`, `fc1` and `fc2Input`, `fc0[0]` and
  `fc0[15]` exactly, and `fc2`, the forward skip and the output exactly. The hash catches a
  changed value and the sum catches a reordering, which is the idiom phase 4 established.
- **The forward-skip rescale is done in 64 bits.** Stockfish scales `fc_0_out[15]` in `int32`,
  where a value above ~223k would overflow; trained networks stay four orders of magnitude
  below that, so widening the product changes nothing this can see and keeps UBSan quiet in the
  debug builds.

Confirmed against the SF16.1 source at the tag rather than from the notes below: `>> 6` clipped
ReLU clamped to `[0, 127]`, `min(127, (x*x) >> 19)` with the sign discarded by the squaring, and
row-major `i * PaddedInputDimensions + j` weights — Stockfish's scrambling is behind
`USE_SSSE3`, so the file order and the `general-64` order are the same thing. The oracle dump
re-verifies all of it: a script recomputed all 56 blocks' activations, rescales and sums from
`fc_0_out` alone and reproduced the dump exactly before a line of C++ was written.

**Measured, as the baseline phases 9 and 10 must beat** (`sf16-test`, `-O2`, i9-9900K): a fresh
transform of a 30-piece position is 9 us, and propagation through one layer stack is 5.3 us. So
a full scalar evaluation is ~14 us per node today, which is what phase 8 will measure honestly
in real time.

*Original plan follows.*

The first phase that computes something new. Purely arithmetic: `Transformed::features` in,
one int32 out. No position, no search, no engine.

**API added to `sf16.h`:**

```cpp
/** Propagate the transformed features through one layer stack, yielding its raw output. */
int32_t propagate(const LayerStack& stack, const std::vector<uint8_t>& features);
```

Internally three small steps worth naming and testing separately: an affine forward over an
`AffineLayer` in canonical row-major order (`layer.weight(i, j)`, summing over `inputs`, not
`paddedInputs` — the padding is zero in practice but reading it is how a layout bug hides), the
clipped ReLU, and the squared clipped ReLU.

Two details that are easy to get subtly wrong and that the golden data will catch:

- `fc_0` has `l2 + 1 = 16` outputs. Outputs `0..14` feed the activations; output `15` bypasses
  the stack entirely and is rescaled into the result. Its scaling exists because `1.0` is
  `127 << WeightScaleBits` there but `600 * OutputScale` in the output.
- `fc_1`'s input is the concatenation **sqr-clipped first, then plain-clipped**, both derived
  from the same 15 `fc_0` outputs, giving 30 values padded to 32.

**Non-goals:** bucket selection, PSQT, centipawns, SIMD, incremental anything.

**Tests** (`sf16_test.cpp`, extending the existing synthetic + golden split):

- synthetic hand-computed affine forward, clipped ReLU and sqr-clipped ReLU on tiny layers,
  including the saturation boundaries (`>= 127 << 6` clips, and the sqr branch's `min(127, ...)`);
- a synthetic stack whose weights make every intermediate hand-checkable;
- golden: for each of the seven FENs and all eight buckets, every intermediate from `nndump`
  must match exactly — not just the final int32. Matching only the last value can hide two
  compensating errors.

**Done when:** all seven golden positions propagate bit-identically through all eight stacks.

---

## Phase 7 — Whole-network evaluation — **done**

All seven golden positions produce Stockfish 16.1's exact `Value` and centipawn score, in all
eight buckets rather than only the one each position selects. `sf16.h` gained `materialBucket`
(over a piece count and over a `Position`), the `Evaluation` trace, `evaluateValue` and
`evaluate`, plus `kNormalizeToPawnValue = 356` and `kMaxEvaluation = 9000`. What the plan below
described is what was built, with four notes:

- **`evaluateValue` takes an optional `Evaluation*` trace**, the same idiom as phase 6's
  `Propagation*`: one implementation, traced or not, so the golden test pins the bucket, the
  PSQT term and the positional term without a second code path to keep in step.
- **The oracle already had the numbers.** `nndump` prints `pieces N  bucket B` per position and
  `value` / `cp` per bucket, so the whole golden table came out of the existing
  `dump-avx2.txt`; no new Stockfish run was needed. Every one of the 56 values matched on the
  first run of the new test.
- **The golden table is all eight buckets, not just the selected one.** The combination
  `(psqt + positional) / OutputScale` and the centipawn division are the same arithmetic
  everywhere, and a rounding of our own or a sign error would show first in the buckets a
  position never picks. Both columns are side-to-move relative, as Stockfish's own are.
- **Color symmetry is two claims, and the test makes both.** A color-swapped position keeps its
  `Value` — the value belongs to the side to move, and the swap swaps who that is — and negates
  its centipawn score. An implementation that negated in the wrong place satisfies one and not
  the other. The golden pair at indices 2 and 3 is exactly such a swap and Stockfish reports
  identical values for it, which is where the convention was read off rather than assumed.

The clamp is not decorative: `4k3/8/8/8/8/QQQQQQQQ/QQQQQQQQ/RRRRKRRR w` evaluates to 19573 cp,
which `Score` cannot hold, and the test asserts it clamps to 9000 and stays below
`Score::mateIn(99)`.

`--verbose <fen>` on `sf16-test` prints the breakdown for an arbitrary position, repeatable, and
loads the network once:

    build/sf16-test --verbose "8/2k5/2p5/8/1P6/8/3K4/6R1 b - - 0 1"
    8/2k5/2p5/8/1P6/8/3K4/6R1 b - - 0 1
      5 pieces, bucket 1: psqt -17386 + positional -15331 = value -2044, 574 cp for white

**One warning for later phases:** the checked-in sources are *not* formatted by clang-format 20,
which is what is on this machine; running it over `sf16_test.cpp` rewrites 1300 lines it should
not touch. Format new code by hand to match its neighbours.

*Original plan follows.*

Wires the transform to the stacks and produces a score.

**API added:**

```cpp
/** The layer stack Stockfish selects for a position, by total piece count. */
uint32_t materialBucket(const Position& position);

/** Evaluate `position` with `network`, in Stockfish's internal Value units. */
int32_t evaluateValue(const Network& network, const Position& position);

/** Evaluate `position` in centipawns, from White's perspective. */
int32_t evaluate(const Network& network, const Position& position);
```

- **Bucket:** `(pieceCount - 1) / 4`, which is also the PSQT bucket — one bucket selects both.
  `ActiveFeatures::size` is already exactly the piece count (HalfKAv2_hm counts kings), so no
  new popcount is needed; `SquareSet::size()` is an O(n) `std::distance` and should not be used
  on this path.
- **Combination:** `(psqt + positional) / OutputScale`, where `psqt` is `Transformed::psqt` for
  that bucket and `positional` is phase 6's output.
- **Sign convention:** the result is relative to the side to move (the transform puts the side
  to move's perspective first). `nnue::evaluate` for SF12 returns a *White-relative* score by
  negating when Black is to move; match that so the four call sites in `search.cpp` need no
  sign changes.
- **Scale:** Stockfish `Value` units are not gbchess centipawns. Start from the principled
  conversion `cp = v * 100 / NormalizeToPawnValue` (**356** at the tag) and treat it as tunable,
  the way SF12's `kScale = 0.0300682` in `nnue.cpp:307` is.
- **Clamp before `Score`:** `Score` is an `int16_t` that *asserts* `-9999 <= cp <= 9999`, and
  mate scores live in the top band via `Score::mateIn`. An NNUE evaluation of a won position
  can exceed that. Clamp to something like `±9000` inside `evaluate` so a legitimate blowout
  cannot abort a debug build or collide with mate scores.

**Explicit non-goals**, all of which are Stockfish *search* heuristics rather than network
evaluation, and none of which gbchess has the inputs for: the `adjusted` psqt/positional blend
in `evaluate_nnue.cpp`, and `evaluate.cpp`'s optimism, complexity, non-pawn-material and
rule-50 damping. Port them later as separate, individually SPRT'd search changes, if at all.

**Tests:** golden final `Value` and centipawn score for the seven FENs; bucket selection over a
table of piece counts including the boundaries (2 pieces → bucket 0, 32 pieces → bucket 7);
symmetry — a position and its color-swapped mirror must evaluate to opposite scores.

**Done when:** the seven golden positions produce Stockfish 16.1's exact `Value`, and a
`--verbose` mode prints the score for an arbitrary FEN.

---

## Phase 8 — Engine integration, correct but slow — **done**

`UseSF16=true` plays. The search's four evaluation expressions collapsed into one exported
`search::staticEval(const Position&)`, both networks now load on first use rather than at static
initialization, `eval/nnue/sf16.cpp` joined `NNUE_SRCS`, and `make -j` is green. What the plan
below described is what was built, with five notes:

- **The collapsed helper honours `UseNNUE` everywhere, which is a behavior change.** Two of the
  four sites ignored the option before, so `UseNNUE=false` did not turn the network off; it does
  now, falling back to `evaluateBoard` at all four. The default is unchanged (`true`), so nothing
  regresses by default, but a `UseNNUE=false` search is a different search than it was.
- **`staticEval` is exported from `search.h`, not file-local.** Phase 9 needs exactly one place to
  hang the accumulator stack, and a test needs to reach it; the doc comment on the declaration is
  where the sign convention and the option precedence are written down. `makeMoveWithEval`'s NNUE
  branch is the one caller that negates the result, because it evaluates *after* the move and
  returns a score belonging to the side that just moved.
- **Both loaders are function-local statics.** The SF16.1 Big net is ~116 MB, so an engine that
  never selects it never reads it; and a missing file now throws where a caller can see it rather
  than out of a static initializer that runs before `main`. The SF12 filename was already
  hardcoded, so the SF16 one is too — a string-valued `UCIOption` remains a separate change.
- **The golden test is Stockfish's own centipawns, through the option.** `testStaticEval` in
  `search_test.cpp` pins three positions to the values `sf16-test --verbose` prints (9, 9 and 574
  cp for White), covering both a White-to-move and a Black-to-move position, plus the piece-square
  fallback and the color-swap symmetry. The failure this guards against is a sign error, not an
  arithmetic one. Note the symmetry claim holds for SF16 only: HalfKAv2_hm mirrors its features, so
  its perspectives are the same computation, while SF12's HalfKP evaluation of a color-swapped pair
  differs by tens of centipawns (−1269 vs −1228 on the endgame FEN).
- **Debug builds are fine.** `UseSF16` defaults off, but `testStaticEval` switches it on for three
  positions, so `search-debug` now loads the Big net under ASan+UBSan. That costs a few seconds;
  `sf16-debug` already paid the same price. `make -j` from clean is ~1m27s.

### Measured, in real time, no nodestime

`build/gbchess`, `-O2`, i9-9900K, `go depth 8`, `OwnBook=false`, one process per position. Node
counts differ between the two because a different evaluation prunes differently, so nodes/second
is the comparison, not time:

| Position | SF12 nodes | SF12 nps | SF16 nodes | SF16 nps | ratio |
|---|---|---|---|---|---|
| startpos | 104317 | 322962 | 112094 | 83714 | 3.9x |
| kiwipete | 549377 | 244602 | 264068 | 75685 | 3.2x |
| `8/2k5/2p5/8/1P6/8/3K4/6R1 b` | 132468 | 481701 | 113947 | 131275 | 3.7x |

**SF16.1 is ~3.5x slower per node than SF12 today.** About 0.24 s of each SF16 time column is the
one-off network load; excluding it the ratio is ~3.0x. This is the number phases 9 and 10 must
beat, and it is the reason `UseSF16` ships off: nothing about the port is worth SPRT-ing until a
node costs something like what an SF12 node costs. The phase-6 microbenchmarks decompose it — a
fresh transform of a 30-piece position is 9 us and one layer stack is 5.3 us, so ~14 us of the
budget per evaluated node is the network, and two thirds of that is the full refresh phase 9
removes.

Self-play at depth 6 from the start position, `UseSF16=true`, plays a normal game — 1.e4 c5 2.Nf3
Nc6 3.Nc3 e5 4.Bc4 d6 5.O-O h6 6.Nd5 Nf6 7.Bb5 a6 8.Bxc6+ bxc6 9.Nxf6+ Qxf6 10.c3 Bg4 11.Qa4 Kd7
12.d4 Bxf3 — with castling, captures and evaluations that stay in a sane band.

*Original plan follows.*

Deliberately before any optimization: get the network playing, exactly, and *measure* it. This
is the phase that turns an oracle-verified library into an engine feature.

- **Loading.** `search.cpp:92` eagerly constructs a global SF12 network at static-init time.
  The SF16.1 Big network is ~116 MB of heap; load it lazily, only when selected, and only once.
- **Selection.** `options::useNNUE` is a bool. Add `inline UCIOption useSF16{"UseSF16", false};`
  next to it in `options.h`, defaulting **off** so nothing regresses while this phase lands.
  A string-valued `EvalFile` option would be nicer but `UCIOption` has no string type today;
  that is a separate, optional change, not a blocker — hardcode the filename next to the SF12
  one for now.
- **Refactor first.** `search.cpp` calls `nnue::evaluate(position, *network)` at four sites
  (798, 872, 938, 1061 as of phase 7). Collapse them into one `Score staticEval(const Position&)`
  helper before adding a second network. Phase 9 needs exactly one place to hook the
  accumulator stack, and four divergent call sites is how that goes wrong.
- **The four sites are not interchangeable, which makes that refactor a decision and not a
  rename.** Checked at phase 7: 798 and 872 are gated on `options::useNNUE` and fall back to
  `evaluateBoard`, while 938 and 1061 call the network unconditionally, so `UseNNUE=false` does
  not actually turn the network off today. All four then negate for the side to move, because
  `nnue::evaluate` hands back a White-relative score and the search wants a side-to-move one.
  Decide deliberately what the collapsed helper does when the option is off; do not preserve
  the current inconsistency by accident.
- **The sign convention needs no changes at those sites.** Phase 7's `sf16::evaluate` returns
  White-relative centipawns exactly as SF12's does, which was the point of matching it.
- **Measure honestly.** Expect this to be *slower per node* than SF12: a full refresh is
  2560 int16 adds per active feature, ~32 features per position, versus SF12's 256-wide
  accumulator. The phase-4 test already prints "fresh transform of a 30 piece position: N us" —
  that is the baseline to beat, and it should be reported before and after every later phase.
  **Do not evaluate this phase under nodestime**: nodestime hides cost-per-node regressions
  entirely, and cost per node is the entire subject of phases 9 and 10.
- **Keep debug builds sane.** Debug builds run with ASan+UBSan and `make test-debug` runs them.
  A full-refresh-per-node SF16 search under ASan will be brutally slow; keep `UseSF16` off in
  the debug search tests, or exercise it there only through a small synthetic network.

**Done when:** `UseSF16=true` plays legal, sensible games at full strength-per-node; `make -j`
is green; a documented nodes/second figure exists for both networks in real time.

---

## Phase 9 — Incremental accumulators — **done**

The search now carries both perspectives' accumulators down the tree instead of rebuilding them
at every evaluated node. Node counts at depth 8 are **identical to phase 8's** on all three
measured positions, so nothing about the evaluation changed; only what it costs did. `sf16.h`
gained `Accumulators`, `refreshBoth`, `PieceChanges`, `pieceChanges` and `AccumulatorStack`, plus
`evaluateValue`/`evaluate` overloads that read accumulators a caller already has; `search.cpp`
gained `makeMoveTracked`, `unmakeMoveTracked` and one file-local stack. What the plan below
described is what was built, with six notes:

- **The update rule needs no dirty-piece record, exactly as phase 7 predicted.** `pieceChanges`
  derives what a move removes from the board and what it adds from a `BoardChange` and the
  pre-move board alone, and it branches on the *shape* of the change rather than on `MoveKind`:
  a no-op second half is a plain move, a second half starting where the first half landed is a
  promotion or an en passant capture, and a second half starting anywhere else is castling. There
  is no `switch` over move kinds to keep in step with `MovesTable::compoundMove`.
- **`refreshBoth` is not an overload of `refresh`.** A `refresh(transformer, position)` returning
  `Accumulators` and the existing `refresh(transformer, features)` returning `Accumulator` are
  ambiguous for any call written `refresh(t, {})`, which an existing test was, and they differ in
  return type by one letter. The name says which one it is.
- **The invariant lives in `push`, not in the tests.** A debug build asserts at every pushed
  position that the incrementally updated pair equals `refreshBoth` of that position, so it holds
  for every caller rather than for the ones a test remembered to check. That is what makes the
  two tests below cheap to write: they only have to *reach* positions.
- **An empty stack is an inactive stack**, and `push`/`pop` are no-ops on one. That is the whole
  of how a search with `UseSF16` off, a test calling `staticEval` directly, and the UCI layer pay
  nothing: `evaluateSF16` falls back to a fresh evaluation when the stack is inactive, and
  `computeBestMove` activates it, under an RAII scope that clears it however the search ends.
- **The root pushes a fresh accumulation rather than a delta**, because `toplevelAlphaBeta` builds
  each child with `applyMove` into a new `Position` and has no `BoardChange` to derive one from.
  There are only as many root moves as the position has, so this is not worth restructuring.
- **The null move needs no work at all**, which is worth stating because it looks like it should.
  It changes `position.turn` and not the board, and the side to move enters the evaluation at
  `transform`, which reads it from the position when the accumulators are read rather than when
  they are built.

### Measured, in real time, no nodestime

`build/gbchess`, `-O2`, i9-9900K, `go depth 8`, `OwnBook=false`, one process per position. Node
counts differ between the two networks because a different evaluation prunes differently, and the
SF16 counts are bit-for-bit the ones phase 8 recorded.

| Position | SF12 nodes | SF12 nps | SF16 nodes | SF16 nps, phase 8 | SF16 nps, phase 9 | ratio |
|---|---|---|---|---|---|---|
| startpos | 104317 | 316112 | 112094 | 83714 | 137201 | 2.3x |
| kiwipete | 549377 | 244276 | 264068 | 75685 | 128001 | 1.9x |
| `8/2k5/2p5/8/1P6/8/3K4/6R1 b` | 132468 | 474795 | 113947 | 131275 | 132189 | 3.6x |

Each SF16 time column still carries a 248 ms one-off network load and each SF12 one a 15 ms load,
both measured at `go depth 1`. Excluding them, SF16 runs at 197k, 145k and 186k nps against SF12's
331k, 246k and 502k, so **the ratio is 1.7x on both middlegame positions, down from phase 8's
3.0x**, and the network now costs about 5 us of the budget per evaluated node rather than 14.

**The endgame barely moved, and that is the shape of the result rather than a disappointment.**
That position has five pieces. A fresh refresh of it copies 2560 biases and adds five feature
rows; an incremental update copies 2560 accumulator values and adds at most three. The copy is
the floor, so the saving is proportional to how many pieces a refresh would have had to add, and
two of the five pieces are kings whose every move forces a refresh anyway. The win is a
middlegame win, which is where the nodes are.

`sf16-test` reports what one move costs against building the same position from scratch, both
perspectives together: **922 ns incrementally against 8344 ns fresh** on the kiwipete capture
`Nxg6`, a 9x reduction in the accumulator half of a node.

### Tests

Two, at different altitudes, plus the assertion inside `push` that both of them trip over:

- `sf16_test.cpp` names the pieces every kind of move moves — quiet, double push, capture, both
  castlings for both colors, en passant for both colors, promotion and promotion capture — as
  sorted sets like `{"Pe7", "rd8"} -> {"Rd8"}`, so a compound move that is decomposed wrongly
  fails on the decomposition rather than on a number 2560 additions later. It then walks every
  legal move to depth 2 from four positions, 4114 in all, checking at each that the stack equals
  a fresh refresh, that `evaluate` through the stack equals `evaluate` from scratch, and — after
  the moves made from a position have been unmade — that its accumulators are still intact.
- `search_test.cpp` runs two shallow searches with `UseSF16` on. Those assert nothing about
  accumulators themselves; their point is to reach positions through the *search's* own make and
  unmake wiring — the root, the main loop, quiescence, the null move — so that the assertion in
  `push` sees them. That costs about six seconds of `search-debug`, where every push refreshes
  both perspectives to compare against.

The Makefile change is one line: `sf16-test` now links `${MOVES_SRCS}`, because a walk over legal
moves needs a move generator. `sf16.cpp` itself still depends on nothing but `core/core.h`.

*Original plan follows.*

The single largest speedup available, and the one with a clean correctness invariant.

- **Where state lives:** an accumulator stack indexed by search ply, pushed in `makeMove` and
  popped in `unmakeMove`, and it must survive `unmakeMove` intact.
- **Correction, checked at phase 7:** `makeMoveWithEval` (search.cpp:789) is *not* on every move.
  It has a single caller, in quiescence at line 858; the main search makes moves with plain
  `makeMove` and evaluates lazily through `getStaticEval`. So the accumulator stack must hang off
  every `makeMove`/`unmakeMove` pair, not off `makeMoveWithEval`, or the quiescence path will be
  the only one that is ever correct.
- **Update rule:** for each perspective, if that perspective's **own king moved**, refresh from
  scratch — HalfKAv2_hm indices all depend on the king bucket and the mirroring flip, and
  Stockfish itself refreshes on any own-king move. Otherwise apply deltas: subtract the moved
  piece's old feature row, add its new one, subtract a captured piece's row. Castling moves the
  king, so it always takes the refresh path.
- **What describes the delta: `BoardChange` is already sufficient, checked at phase 7.** No
  dirty-piece record needs adding. `prepareMove` (move.cpp:78) fills in `captured` (the piece)
  and `first = {move.from, compound.to}`, and because `captured = board[compound.to]`, `first.to`
  *is* the square the captured piece stood on — including for en passant, where `compoundMove`
  sets `to` to the captured pawn's own square and uses `second` to walk the capturing pawn on to
  its real destination. `second` likewise carries the rook in castling and the square in a
  promotion, and `promo` is an index delta on the piece.
  The one thing `BoardChange` does *not* name is the **moving** piece: `makeMove` recovers it as
  `board[change.first.from]` before the board is touched. Compute the feature delta there, on the
  pre-move board, or reconstruct it after the fact from `first.to` and `promo`.
- **The invariant, as a test:** in debug builds, assert at every node that the incrementally
  maintained accumulator equals a fresh `refresh()` of the same position, for both perspectives.
  This single assertion catches essentially every class of bug this phase can introduce, and it
  costs nothing in optimized builds. Run it over a perft-style walk of a few thousand positions
  and over the fixed puzzle suite.

**Done when:** the equality assertion holds across the debug test suite, evaluations are
unchanged bit-for-bit from phase 8, and nodes/second in real time is materially higher.

---

## Phase 10 — Performance and SIMD

Only after phases 8 and 9 have produced real measurements; this is where the port stops being
obviously correct and needs the scalar path kept as an oracle.

- Keep the canonical row-major layout as the reference implementation and the thing tests check.
  If a permuted layout is needed for the affine kernels, build it at load time into a *separate*
  runtime structure, and keep a test asserting that both paths produce identical output for the
  golden positions. This preserves the ladder's founding design choice.
- `AGENTS.md` (line 80) asks for well-known x86 SSE2 APIs implemented in standard C++ where
  possible, and that abstraction already exists in the tree: `core/sse2.h` selects `<emmintrin.h>`
  or the portable `core/sse2emul.h` and declares `constexpr bool haveSSE2 = true` either way.
  Only `square_set.cpp` uses it today and neither NNUE does. Use it rather than raw intrinsics,
  and run the golden tests under `-DSSE2EMUL` as well, so the emulated path stays exact.
- The obvious targets, in expected payoff order: the feature-transformer delta adds (16-wide
  int16), the clip-and-pairwise-multiply in `transform`, and the `fc_0` affine forward (2560
  int8 MACs per output). `fc_1` and `fc_2` are tiny and not worth vectorizing.
- Stockfish's sparse-input affine trick for `fc_0` exploits the many zero bytes in the
  transformed features. Worth considering, but it is an optimization *of a verified kernel*,
  so it belongs at the end, not the start.
- Remember `make -j` builds with Apple clang locally but real GCC on CI; syntax-check anything
  intrinsic-adjacent with `/usr/local/bin/g++-15 -fsyntax-only` before pushing.

**Done when:** nodes/second with SF16.1 is within a stated factor of the SF12 path, and the
scalar and optimized paths agree exactly on the golden positions.

---

## Phase 11 — Strength validation and cutover

- SPRT SF16.1 against the SF12 evaluation **in real time, not nodestime**. A network that is
  400 Elo better per node and 6x slower per node is not obviously an improvement, and nodestime
  will report it as a triumph.
- Watch for the known first-mover time-forfeit artifact in real-time SPRT runs
  (`first-mover-time-forfeits`); a slower evaluation makes that failure mode more likely, and a
  forfeit is not an evaluation result. Use `test/sprt_summary.py` to separate the two.
- `book.csv` is tracked as of `c405f31` (confirmed at phase 7: it is in `git ls-files`, and
  `.gitignore` un-ignores it with `!book.csv`), so start positions vary; but note that `sprt-self`
  replays the fixed 10-puzzle endgame fixture, which is a narrow sample for an evaluation
  change. Prefer a book-driven run for the cutover decision.
- Retune what the evaluation feeds: the aspiration windows, futility margins and reverse
  futility margins in `options.h` are all in centipawns and were tuned against SF12's scale.
  A different evaluation scale silently changes all of them. Re-tune, or at minimum re-SPRT the
  margins, before concluding the network itself is neutral.
- Cutover: flip `UseSF16` on by default only after a passing SPRT. Keep the SF12 path and its
  network file; two evaluations that can be switched between are worth more than a deleted one,
  and it matches the project's "turn algorithms on and off" goal.

**Done when:** a passing SPRT in real time at a sane time control, with the option defaulted on
and the result recorded.

---

## Cross-cutting notes

**Working conventions.** One squashed commit per phase on master, single-line message, no PR.
Each phase carries its own tests and states its own non-goals, as phases 1-4 did.

**Worktrees.** A fresh worktree has no `*.nnue`; symlink them
(`ln -sf /Users/bosch/gbchess/nn-*.nnue <worktree>/`) or `make` re-downloads 65 MB.
`EnterWorktree` branches from a possibly-stale `origin/master` — check and `git reset --hard
master`. A `make clean` in the main checkout deletes matching files inside `.claude/worktrees/*`
too; suspect it if tracked files vanish mid-session.

**Test wiring.** `sf16_test.cpp` is already registered
(`$(eval $(call test_rules,eval/nnue/sf16,...))`) and both `test-cpp` and `test-debug` depend on
`${SF16_NNUE_FILE}` and symlink the nets into `build/`. New phases need no Makefile changes
until phase 8 adds `sf16.cpp` to `NNUE_SRCS`/`SEARCH_SRCS`.

**Sequencing risk.** The temptation is to jump from phase 6 to SIMD, because the scalar full
refresh is visibly slow. Resist it: phase 8's measurement is what tells you whether phase 9 or
phase 10 is the real bottleneck, and phase 9's exactness invariant is much harder to establish
on top of a vectorized kernel than under one.

**The oracle is the whole method.** Approximate agreement on a 116 MB parameter file hides
layout and indexing errors indefinitely. Every phase above lands with exact golden values or it
does not land.
