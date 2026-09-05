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

`src/eval/nnue/sf16.h` today ends at `Transformed transform(...)`. That boundary is an **exact
oracle**: seven positions are verified bit-for-bit against a patched Stockfish 16.1, across all
eight PSQT buckets, by `testGoldenTransforms` in `sf16_test.cpp`. Everything below is a pure
function of `Transformed` plus the loaded `LayerStack`s, so each phase below can be verified
against Stockfish independently of the ones before it.

Not yet implemented, in dependency order: layer stack propagation, bucket selection and score
scaling, engine integration, incremental accumulators, SIMD, strength validation.

The whole port keeps the file's **canonical row-major ordering**. Stockfish scrambles affine
weights at load time to suit its SIMD kernels; gbchess does not, so the scalar reference stays
debuggable. Phase 10 is the only place that may introduce a second, permuted layout, and only
alongside the canonical one.

---

## Phase 5 — Prerequisite: re-establish the oracle

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
If any differs, the plan's arithmetic changes, not its shape.

**Done when:** `nndump` prints all five groups for the seven phase-4 golden FENs, and the
`general-64` and `avx2` binaries agree on every number.

---

## Phase 6 — Layer stack propagation

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

## Phase 7 — Whole-network evaluation

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
  conversion `cp = v * 100 / NormalizeToPawnValue` (328 at the tag) and treat it as tunable,
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

## Phase 8 — Engine integration, correct but slow

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
  (lines ~795, ~872, ~938, ~1061). Collapse them into one `Score staticEval(const Position&)`
  helper before adding a second network. Phase 9 needs exactly one place to hook the
  accumulator stack, and four divergent call sites is how that goes wrong.
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

## Phase 9 — Incremental accumulators

The single largest speedup available, and the one with a clean correctness invariant.

- **Where state lives:** an accumulator stack indexed by search ply, pushed in `makeMove` and
  popped in `unmakeMove`. `makeMoveWithEval` already evaluates *after* every move, including in
  quiescence, so the stack must survive `unmakeMove` intact.
- **Update rule:** for each perspective, if that perspective's **own king moved**, refresh from
  scratch — HalfKAv2_hm indices all depend on the king bucket and the mirroring flip, and
  Stockfish itself refreshes on any own-king move. Otherwise apply deltas: subtract the moved
  piece's old feature row, add its new one, subtract a captured piece's row. Castling moves the
  king, so it always takes the refresh path.
- **What describes the delta:** `moves::BoardChange` / `prepareMove` already carry what changed;
  check whether they name the captured piece and its square, or whether a small dirty-piece
  record needs adding alongside them.
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
- `AGENTS.md` asks for well-known x86 SSE2 APIs implemented in standard C++ where possible.
  The obvious targets, in expected payoff order: the feature-transformer delta adds (16-wide
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
- `book.csv` is tracked as of `c405f31`, so start positions vary; but note that `sprt-self`
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
