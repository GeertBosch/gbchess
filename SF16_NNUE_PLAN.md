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
| 9 | `a018b22` | Incremental accumulators, an accumulator stack under make/unmake |
| 10 | `42ae86a` | Sparse fc0 over transposed weights, a fused update, no allocation |
| 11 | | Strength validation, cutover, and the removal of the SF12 evaluation |

`src/eval/nnue/sf16.h` ends at `evaluate(...)`, and everything down to it is an **exact
oracle**: seven positions are verified bit-for-bit against a patched Stockfish 16.1, across all
eight buckets, from the transformed bytes through every layer intermediate to the final `Value`
and centipawn score. `sf16.cpp` is a complete, standalone SF16.1 evaluation -- scalar
through phase 9, with the sparse and SSE2 kernels of phase 10 checked against that scalar path --
and since phase 8 the engine plays with it when `UseSF16` is set.

Not yet implemented: strength validation. Phase 11 is the last one, and it ends with the SF12
evaluation deleted and `sf16.*` renamed to `nnue.*`, so nothing after it refers to two networks.

The whole port keeps the file's **canonical row-major ordering**. Stockfish scrambles affine
weights at load time to suit its SIMD kernels; gbchess does not, so the scalar reference stays
debuggable. Phase 10 introduced the one permuted layout in the tree, the transposed `fc_0` weights,
built at load time alongside the canonical ones and held against them by test.

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

### Found by defaulting `UseSF16` on: the network load was on the clock

Flipping the default to `true` and running `make -j` fails four UCI tests, all with the same
signature — `nodes 0`, an elapsed time of 200 ms to 2 s, and `bestmove 0000`. The cause is not the
evaluation but *when* it is loaded: phase 8 made both networks function-local statics read on first
use, and first use is inside the first search of a process. Reading 116 MB takes ~250 ms on an idle
machine and ~2 s under a parallel build, and all of it is charged to that move's budget, so the
first `go` under any real time control spends its whole clock loading, searches nothing, and has no
move to return. SF12 hid this: its network loads in 15 ms.

`search::warmEvaluation()` now reads whichever network the options select, and the UCI layer calls
it at startup — before any command can start a clock, since a GUI may send `go` first — and after
any `setoption`, which is where the selection can change. Loading stays lazy in the *option*: with
`UseNNUE=false` nothing is read at all. A network that fails to read is not reported there; the
function-local static is left uninitialized, so the search retries and throws where the engine
already turns that into an `info string error in search` rather than a terminated process. With the
fix, `make -j` passes with `UseSF16` defaulted on, over three consecutive runs.

**One latent robustness gap remains, and it is in the root loop rather than in the evaluation.**
`toplevelAlphaBeta` runs the soft time check *before* searching each root move, the first included,
and on abort returns an empty `PrincipalVariation`, which the engine prints as `bestmove 0000` — an
illegal UCI response. Any budget that expires before root move one completes reaches it, whatever
the evaluation; SF16 makes a node ~1.7x dearer and so widens the window. It shows up as an
occasional `uci-move-overhead` failure on the 1 ms-budget case under a loaded machine (once in four
`make -j` runs here, never on an idle one, and never reproducible under synthetic CPU load). The
fix belongs to time management, not to this port: the root must complete at least one move before
it may honour an abort. Worth closing before phase 11 puts the network on a clock in anger.

---

## Phase 10 — Performance and SIMD — **done**

The evaluation is 5.3x cheaper per node and the search now runs **faster with SF16.1 than with
SF12** on both middlegame positions, having been 3.0x slower at phase 8 and 1.7x at phase 9. Every
golden value is unchanged: all 56 propagations, all 56 evaluations, the seven transforms and the
node counts at depth 8 are bit for bit what phases 6 to 9 recorded. `sf16.h` gained
`ColumnMajorLayer`, `transpose`, `affineForwardSparse`, a pointer form of `affineForward` and one
of `transform`; `LayerStack` gained `fc0Columns`. What the plan below asked for is what was built,
with six notes.

- **The compiler was already vectorizing everything, and that was the problem.** The first move was
  to read the generated code rather than to reach for intrinsics, and at `-O2` Apple clang had
  already turned every hot loop into SSE: `affineForward` widened bytes to 32 bit lanes with
  `pmovsxbd`/`pmaddwd`, `transform` ran 8 values per iteration, the accumulator adds ran 32 int16 at
  a time. So there was nothing to *start* vectorizing. What the kernels needed was a better
  algorithm and a better shape, after which the compiler's own output is fine. Only one SSE2 call
  is written by hand in the whole phase.
- **The one intrinsic is the nonzero scan, and it goes through `core/sse2.h`.** `nonzeroMask`
  compares 16 bytes against zero and takes a `_mm_movemask_epi8`, both of which `core/sse2emul.h`
  already implements. It compares rather than reading the sign bits directly because a transformed
  feature never has bit 7 set - the pairwise product tops out at 126 - and because comparing is the
  form that reads the same under the emulation, whose `_mm_movemask_epi8` reports "byte is not zero"
  where the hardware reports "byte is negative". The two agree on exactly the all-ones and all-zeros
  bytes a comparison produces, which is all this uses.
- **fc0 skips the zeros, and 90% of its input is zero.** The transform's pairwise product vanishes
  whenever either half clipped to zero, and the test now reports the density it measures rather than
  taking it on faith. Skipping those inputs needs the weights transposed, which is the second,
  permuted layout the port's design allows: `ColumnMajorLayer` holds fc0's 16 weights per input
  contiguously and is built at load time *beside* the canonical row major layer, which stays what
  `affineForward` reads and what the golden tables pin. 327KB more memory across the eight stacks.
  The kernel's inner loop is a fixed 16 iterations, which is the whole reason for fixing
  `ColumnMajorLayer::kOutputs` at compile time: a compiler turns that into straight line vector code
  on any target, and a layer of any other width simply gets no twin and keeps the canonical path.
- **The other three wins are pure C++ restructuring.** The transform's clip-and-multiply now stays
  in int16 - both operands clip into `[0, 127]`, so the product needs 14 bits - where writing it in
  `int` made clang widen to 4 lanes per register; that alone is 2.6x on that loop. The incremental
  accumulator update was four passes over 5KB (a copy, then one per row) and is now one fused pass,
  `dst = src + added - removed`, dispatched on the shape of the move so the row counts are compile
  time constants. And `propagate` and `evaluateValue` no longer allocate: they keep their buffers in
  `thread_local` scratch, since a search evaluates millions of nodes and the engine runs its search
  on its own thread.
- **What is left is memory bandwidth, not arithmetic.** A refresh reads 30 feature rows of 5KB each
  out of a 65MB weight table and takes 8 us; an incremental update reads at most six and takes 600
  ns. Both work out to 36-51 GB/s on this machine, so the rows themselves are the floor and there is
  no more to win there without caching accumulators across positions the way Stockfish's finny
  tables do.
- **Stockfish's sparse trick, but not Stockfish's kernel.** SF16.1 finds nonzero *4-byte chunks* and
  spends them through `maddubs`, which is SSSE3. This finds nonzero bytes and spends each on all 16
  outputs at once, which needs nothing beyond SSE2 and no permutation of the input.

### Measured

`sf16-test`, `-O2`, i9-9900K. The two right hand columns are the same binary built with
`-DSSE2EMUL`, so the portable emulation is exercised on real data rather than only compiled.

| | phase 9 | phase 10 | ratio | emulated |
|---|---|---|---|---|
| propagation through one layer stack | 5334 ns | 873 ns | 6.1x | 1122 ns |
| both perspectives of a capture, incrementally | 1040 ns | 600 ns | 1.7x | 585 ns |
| a whole `evaluate()` from accumulators | 6013 ns | 1142 ns | 5.3x | |
| of which fc0 | 4661 ns | 616 ns | 7.6x | |
| of which the transform | 598 ns | 326 ns | 1.8x | |

`build/gbchess`, `go depth 8`, `OwnBook=false`, real time, no nodestime. The node counts are the
ones phases 8 and 9 recorded, to the node, so nothing about the evaluation moved.

| Position | SF12 nps | SF16 nps, phase 9 | SF16 nps, phase 10 | SF16 against SF12 |
|---|---|---|---|---|
| startpos | 330117 | 197000 | 437867 | **1.33x faster** |
| kiwipete | 230927 | 145000 | 308130 | **1.33x faster** |
| `8/2k5/2p5/8/1P6/8/3K4/6R1 b` | 498000 | 186000 | 409881 | 1.21x slower |

The phase 9 column is that phase's own figures with its one-off network load excluded, which is how
it reported them; phase 10's numbers need no such adjustment, the load having moved off the clock in
`d836299`. **The endgame is the one position where SF12 still leads**, and for the reason phase 9
gave: five pieces make a refresh cheap and an incremental update no cheaper, and two of the five are
kings whose every move forces a refresh anyway. It is also the position where the evaluation matters
least.

### Tests

- `sf16_test.cpp` gains `testSparseAffineForward`, which checks the transposition weight by weight
  across all eight stacks, then holds the sparse kernel against the canonical one on all 56 golden
  position-bucket pairs - both as `affineForwardSparse` against `affineForward`, and as a whole
  `propagate` through a stack stripped of its transposed weights against one that has them. It then
  runs synthetic layers 1, 15, 16, 17, 31, 33 and 64 inputs wide, which are the widths that exercise
  the kernel's tail, plus an all-zero input that must leave the biases untouched. The canonical path
  stays the one the golden tables pin, so this is a comparison against something already exact.
- `make test` and `make ci` gain `build/sf16-sse2emul.out`: the same test binary built with
  `-DSSE2EMUL`, which is how the emulated path stays exact rather than merely compiling. It is
  deliberately not named `*-test`, because `build/test-cpp.out` insists those correspond one to one
  with `*_test.cpp` files.
- The invariant phase 9 put inside `push` does the rest of the work for the fused accumulator
  update: a debug build still compares every pushed position against a fresh `refreshBoth`, over the
  4114 positions of the make/unmake walk and the two searches in `search_test.cpp`.
- Checked under real GCC as well as Apple clang, with and without `-DSSE2EMUL`, per
  `macos-gpp-is-clang-ci-blind-spot`.

---

## Phase 11 — Strength validation and cutover

The last phase: five steps in three movements, in this order. Measure (steps 1-2), because the
measurement is what licenses everything after it. Delete (step 3), so the tree stops carrying two
evaluations and stops talking about the one that lost. Then make what remains honest (steps 4-5):
docs that describe the engine as it is, and a puzzle suite that cannot improve in silence.

### Step 1 — SPRT prerequisites

- `sprt-self` starts from `build/book-openings.epd`: book-exit positions generated from `book.csv`
  by `book-gen --sprt-suite`, passed as `--openings-file` with `OwnBook=false` on both sides so
  the book does not play on top of the suite position. That is a wide enough sample for an
  evaluation change; run `make book-openings` and go.
- `book.csv` is tracked and must not be regenerated: `make generate-book` downloads tens of GB.

### Step 2 — The strength measurement

- SPRT the SF16.1 evaluation against the current default **in real time, not nodestime**. A
  network that is 400 Elo better per node and several times slower per node is not obviously an
  improvement, and nodestime reports it as a triumph. This is the one measurement in the ladder
  that nodestime cannot stand in for.
- Watch for the known first-mover time-forfeit artifact (`first-mover-time-forfeits`): a more
  expensive evaluation makes it likelier, and a forfeit is not an evaluation result. Use
  `test/sprt_summary.py` to separate forfeits from losses before reading any Elo number.
- The endgame position from phase 10 is the one place the old evaluation still leads on speed,
  so a book-driven run (which starts from book exits, not endgames) is the honest sample.

**Non-goal, deliberately deferred:** retuning what the evaluation feeds. The aspiration windows
and futility / reverse-futility margins in `options.h` are in centipawns and were tuned against
the old scale, so the new network is being judged with somebody else's margins. Measure it that
way anyway. If it passes untuned it passes, and retuning becomes a separate later phase with its
own SPRT — a tuning pass folded into the cutover would make it impossible to say which of the
two changes bought the Elo.

### Step 3 — Cutover: delete SF12, root and branch

Once the bar is met, there is one evaluation. No option, no fallback path, no second network
file, no `sf16` in an identifier. Everything below comes out in the same commit; git history is
where the old evaluation lives from then on.

- **Delete** `src/eval/nnue/nnue.{h,cpp}` and `src/eval/nnue/nnue_test.cpp` (the SF12
  implementation), and drop `nnue.cpp` from `NNUE_SRCS`.
- **Rename** `sf16.h` → `nnue.h`, `sf16.cpp` → `nnue.cpp`, `sf16_test.cpp` → `nnue_test.cpp`,
  and flatten `namespace nnue::sf16` to `namespace nnue`. Use `git mv` so the history follows
  the file. Update the `test_rules` registration and every include.
- **Remove the switch:** the `UseSF16` option in `options.h`, `sf12Network()` in `search.cpp`,
  and the branch in `staticEval` that chooses between them. One evaluation call remains.
- **Makefile:** `NNUE_URL`/`NNUE_FILE` become the SF16.1 net (`nn-b1a57edbea57.nnue`); the
  separate `SF16_NNUE_URL`/`SF16_NNUE_FILE` pair and the `nn-82215d0fd0df.nnue` download rule go
  away; `build/sf16-sse2emul` becomes `build/nnue-sse2emul` in both `test` and `ci`. Delete the
  local `nn-82215d0fd0df.nnue`.
- **Two callers that are not just renames, and are the likeliest way this step breaks:**
  - `eval_test.cpp` hardcodes `loadNNUE("nn-82215d0fd0df.nnue", nnue::kNormal)` and drives
    `build/evals.out` against the lichess eval CSVs. It has to be repointed at the new network,
    and its accepted error bands re-derived, because the two networks do not share a centipawn
    scale. If the comparison is not worth re-deriving, say so and delete the target rather than
    leaving it aimed at a deleted network.
  - `nnue_stats.{h,cpp}` counters are only ever incremented from the SF12 `nnue.cpp`; its
    callers in `eval_test.cpp` and `search_test.cpp` would print zeros forever. Either move the
    instrumentation into the surviving evaluation or delete the module with its callers. Do not
    leave dead counters.
- **Done check for this step:** `grep -ri 'sf12\|sf16' src Makefile test` returns nothing except
  `sprt-sf12`, which is an SPRT against the external `stockfish-12` binary and unrelated to the
  evaluation. `make -j` and `make ci` pass.

### Step 4 — Documentation pass

Every doc in the tree gets read, not just the ones that mention NNUE. The rule is that a comment
describing how the code used to behave is a bug unless it is warning about a pitfall that is
still reachable.

- Sweep at least: `README.md`, `AGENTS.md`, `USING_SPRT.md`, `USING_PUZZLES.md`, `MakeMove.md`,
  `QSTT.md`, `SEARCH_OPTIMIZATION_ANALYSIS.md`, and the file-header comments in `search.h`,
  `search.cpp`, `options.h` and `eval.h`.
- Delete stale statements outright — "the SF12 network gbchess itself evaluates with", "only read
  by the SF16 format unit test so far", the `search.h` sentence about which network is used when
  `UseSF16` is on. Keep only what still warns about a live pitfall (nodestime hiding per-node
  cost, first-mover forfeits, `make generate-book` downloading tens of GB, `make clean` reaching
  into worktrees).
- This plan file keeps its name and its phase 1-10 text: phases 5-10 are a record of porting one
  network against another, and the contrast is the point there. Nothing *after* phase 10 should
  mention SF12 at all.
- `dairy.md` is a dated log; append, do not rewrite.
- Update the `sf16-nnue-port-ladder` memory: the ladder is complete and the files are `nnue.*`.

### Step 5 — Make the puzzle suites ratchet

The `ci_nonmate_100` suite went from 93/100 to 98/100 solved and nothing said so. A test that can
only silently improve can also silently regress back, and the improvement is exactly the moment the
bar should have been raised.

- `puzzle_test.cpp` reports `<stats>, <N> rating` and exits nonzero only when the *engine* fails;
  the solved count and the derived rating are printed and then thrown away.
  `kExpectedPuzzleRating = 2000` is only the ELO seed, not an expectation.
- Add `--expect-rating N` (and a `--rating-tolerance` defaulting to something wider than observed
  run-to-run noise). Below the band: fail as a regression -- the per-puzzle failure output the
  test already prints is the diagnostic.
  **Above the band: also fail**, with a diagnostic that says the suite got better, gives the
  measured rating, and names the exact `Makefile` line to raise. A ratchet that only catches
  regressions is the bug being fixed here.
- Expectations are per suite and per depth (`ci_mate123_4000` at depth 7, `ci_mate45_100` at
  depth 11, `ci_nonmate_100` at depth 7), so each of the three Makefile rules carries its own
  number.
- Before picking the band, establish how repeatable the number is: fixed depth with a fixed
  binary should be deterministic, but the worker pool and any time-based cutoff can make it not
  so. Measure a handful of runs. If it is deterministic, the band can be tight (a few rating
  points); if not, the band has to cover the spread, and a too-wide band is worth less than
  gating on the solved count instead.
- Land the expectation numbers from the SF16.1 engine, i.e. after step 3, so the ratchet records
  the new strength rather than immediately failing on it.

**Done when:** a passing real-time SPRT is recorded here with its time control, node rates and
forfeit count; no `sf12`/`sf16` identifier survives outside `sprt-sf12`; `make -j` and `make ci`
are green; the docs describe the engine as it is; and the puzzle suites fail in both directions
against a recorded expectation.

---

## Cross-cutting notes

**Working conventions.** One squashed commit per phase on master, single-line message, no PR.
Each phase carries its own tests and states its own non-goals, as phases 1-4 did.

**Nothing is kept for reference.** No commented-out code, no `#if 0`, no superseded copy of a
function left alongside its replacement, no doc paragraph preserved as "the old plan". History is
the archive, and it is a better one: a `log -S` search finds a deleted implementation faster than
reading past it in the working tree does. When something is replaced, delete it.

**Worktrees.** A fresh worktree has no `*.nnue`; symlink them
(`ln -sf /Users/bosch/gbchess/nn-*.nnue <worktree>/`) or `make` re-downloads 65 MB.
`EnterWorktree` branches from a possibly-stale `origin/master` — check and `git reset --hard
master`. A `make clean` in the main checkout deletes matching files inside `.claude/worktrees/*`
too; suspect it if tracked files vanish mid-session.

**Test wiring.** `sf16_test.cpp` is registered via `$(eval $(call test_rules,eval/nnue/sf16,...))`;
`test-cpp` and `test-debug` depend on `${SF16_NNUE_FILE}` and symlink the nets into `build/`, and
`build/sf16-sse2emul.out` rebuilds the same test with `-DSSE2EMUL`. All three names change in
phase 11's rename.

**Sequencing risk.** Phases 8 through 10 confirmed the order: the measurement at phase 8 is
what identified the real bottleneck, and phase 9's exactness invariant would have been much harder
to establish on top of a vectorized kernel than under one. The same order applies to phase 11 --
the SPRT decides, and only then does anything get deleted.

**The oracle is the whole method.** Approximate agreement on a 116 MB parameter file hides
layout and indexing errors indefinitely. Every phase above lands with exact golden values or it
does not land.
