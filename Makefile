PUZZLES=puzzles/lichess_db_puzzle.csv
PHASES=opening middlegame endgame
EVALS=$(foreach phase,${PHASES},evals/lichess_${phase}_evals.csv)
CCFLAGS=-std=c++17 -Werror -Wall -Wextra
CLANGPP=clang++
GPP=g++
DEBUGFLAGS=-DDEBUG -O0 -g
OPTOBJ=build/opt
DBGOBJ=build/dbg

# First argument is DBG or OPT, second is list of source files
calc_objs=$(patsubst src/%.cpp,${$(1)OBJ}/%.o,$(2))
calc_deps=${calc_objs:.o=.d}

all: debug test perft-bench perft-test perft-debug-test mate123 mate45 puzzles evals
	@echo "\n*** All tests passed! ***\n"

-include $(call calc_deps,OPT,${ALLSRCS})
-include $(call calc_deps,DBG,${ALLSRCS})

# MacOS specific stuff - why can't thinks just work  by default?
ifeq ($(_system_type),Darwin)
    export MallocNanoZone=0
    sdk=$(shell xcrun --sdk macosx --show-sdk-path)
    arch=$(shell uname -m)
    CLANGPP:=/usr/bin/clang++
    CCFLAGS:=${CCFLAGS} -isysroot ${sdk} -mmacosx-version-min=11.0 -target darwin17.0.0 -arch ${arch} -stdlib=libc++
    DEBUGFLAGS:=${DEBUGFLAGS} -fsanitize=address
    LINKFLAGS:=${LINKFLAGS} -Wl,-syslibroot,${sdk},-dead_strip_dylibs -mmacosx-version-min=11.0 -target darwin17.0.0 -arch ${arch}
endif

${OPTOBJ}/%.d: src/%.cpp
	@mkdir -p ${OPTOBJ}
	${GPP} -MT $(subst .d,.o,$@) -MM ${CCFLAGS} -Isrc -o $@ $< > $@
${OPTOBJ}/%.o: src/%.cpp ${OPTOBJ}/%.d
	@mkdir -p ${OPTOBJ}
	${GPP} -c ${CCFLAGS} -Isrc -O2 -o $@ $<

${DBGOBJ}/%.d: src/%.cpp
	@mkdir -p ${DBGOBJ}
	${GPP} -MM ${CCFLAGS} -Isrc -o $@ $< > $@

${DBGOBJ}/%.o: src/%.cpp
	@mkdir -p ${DBGOBJ}
	${CLANGPP} -MMD -c ${CCFLAGS} ${DEBUGFLAGS} -Isrc -o $@ $<

build/%-test: ${OPTOBJ}/%_test.o
	@mkdir -p build
	${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^

build/%-debug: ${DBGOBJ}/%_test.o
	@mkdir -p build
	${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $^

ALLSRCS=$(wildcard src/*.cpp)

.deps: $(call calc_deps,OPT,${ALLSRCS}) $(call calc_deps,DBG,${ALLSRCS})

.SUFFIXES: # Delete the default suffix rules
.PHONY:

clean: .PHONY
	rm -fr build
	rm -f *.log
	rm -f core *.core puzzles.actual perf.data* *.ii *.bc *.s
	rm -f game.??? log.??? players.dat # XBoard outputs
	rm -f ut*.out
	rm -f puzzles.csv
	rm -rf *.dSYM .DS_Store

build/fen-test: ${OPTOBJ}/fen.o
build/fen-debug: ${DBGOBJ}/fen.o

MOVES_SRCS=src/moves.cpp src/magic.cpp

build/nnue-test: $(call calc_objs,OPT,src/nnue.cpp src/nnue_stats.cpp src/nnue_incremental.cpp ${MOVES_SRCS} src/fen.cpp)
build/nnue-debug: $(call calc_objs,DBG,src/nnue.cpp src/nnue_stats.cpp src/nnue_incremental.cpp ${MOVES_SRCS} src/fen.cpp)

build/moves-test: $(call calc_objs,OPT,${MOVES_SRCS} src/fen.cpp) 
build/moves-debug: $(call calc_objs,DBG,${MOVES_SRCS} src/fen.cpp)

build/hash-test: $(call calc_objs,OPT,${MOVES_SRCS} src/hash.cpp src/fen.cpp)
build/hash-debug: $(call calc_objs,DBG,${MOVES_SRCS} src/hash.cpp src/fen.cpp)

build/magic-test: $(call calc_objs,OPT,${MOVES_SRCS})
build/magic-debug: $(call calc_objs,DBG,${MOVES_SRCS})

EVAL_SRCS=src/eval.cpp src/hash.cpp src/fen.cpp src/nnue.cpp src/nnue_stats.cpp src/nnue_incremental.cpp ${MOVES_SRCS}

build/eval-test: $(call calc_objs,OPT,${EVAL_SRCS})
build/eval-debug: $(call calc_objs,DBG,${EVAL_SRCS})

SEARCH_SRCS=${EVAL_SRCS} src/search.cpp

build/search-test: $(call calc_objs,OPT,${SEARCH_SRCS})
build/search-debug: $(call calc_objs,DBG,${SEARCH_SRCS})

build/uci-test: $(call calc_objs,OPT,src/uci.cpp ${SEARCH_SRCS})
build/uci-debug: $(call calc_objs,DBG,src/uci.cpp ${SEARCH_SRCS})

PERFT_SRCS=src/perft.cpp ${MOVES_SRCS} src/fen.cpp src/hash.cpp
# perft counts the total leaf nodes in the search tree for a position, see the perft-test target
build/perft: $(call calc_objs,OPT,${PERFT_SRCS})
	${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^
build/perft-debug: $(call calc_objs,DBG,${PERFT_SRCS})
	${GPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $^

# Compare the perft tool with some different compilation options for speed comparison
build/perft-clang-sse2: ${PERFT_SRCS} src/*.h 
	@mkdir -p build
	${CLANGPP} -O3 ${CCFLAGS} -Isrc -g -o $@ $(filter-out %.h,$^)
build/perft-clang-emul:  ${PERFT_SRCS} src/*.h
	@mkdir -p build
	${CLANGPP} -O3 -DSSE2EMUL ${CCFLAGS} -Isrc -g -o $@ $(filter-out %.h,$^)
build/perft-gcc-sse2:  ${PERFT_SRCS} src/*.h
	@mkdir -p build
	${GPP} -O3 ${CCFLAGS} -Isrc -g -o $@ $(filter-out %.h,$^)
build/perft-gcc-emul:  ${PERFT_SRCS} src/*.h
	@mkdir -p build
	${GPP} -O3 -DSSE2EMUL ${CCFLAGS} -Isrc -g -o $@ $(filter-out %.h,$^)

build/perft-%.ok: build/perft-%
	./$< -q 5 4865609

# Aliases for perft test targets
perft-bench: build/perft-clang-emul.ok build/perft-gcc-emul.ok build/perft-clang-sse2.ok build/perft-gcc-sse2.ok

perft-test: build/perft-test

perft-debug-test: build/perft-debug-test

# Solve some known mate-in-n puzzles, for correctness of the search methods
mate123: build/search-test ${PUZZLES} .PHONY
	egrep "FEN,Moves|mateIn[123]" ${PUZZLES} | head -1001 | ./build/search-test 5

mate45: build/search-test ${PUZZLES} .PHONY
	egrep "FEN,Moves|mateIn[45]" ${PUZZLES} | head -101 | ./build/search-test 9

puzzles.csv: ${PUZZLES} Makefile
	egrep -v "mateIn[12345]" ${PUZZLES} | head -101 > $@

puzzles: puzzles.csv build/search-test .PHONY
	./build/search-test 6 < $<

evals: build/eval-test ${EVALS} .PHONY
	./build/eval-test ${EVALS}

# Some line count statistics, requires the cloc tool, see https://github.com/AlDanial/cloc
cloc:
	@echo "\n*** Excluding Tests ***\n"
	cloc --by-percent cmb `find src -name \*.cpp -o -name \*.h | egrep -v '_test[.]|debug'`
	@echo "\n*** Just Tests ***\n"
	cloc --by-percent cmb `find src -name \*.cpp -o -name \*.h | egrep '_test[.]|debug'`

# The following are well known perft positions, see https://www.chessprogramming.org/Perft_Results
kiwipete="r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
position3="8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
position4="r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"
position4m="r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1"
position5="rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
position6="r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"

build/perft-debug-test: build/perft-debug
	./build/perft-debug 4 197281

# Verify well-known perft results. Great for checking correct move generation.
build/perft-test: build/perft
	./build/perft -q 5 4865609
	./build/perft

${PUZZLES}:
	mkdir -p $(dir ${PUZZLES}) && cd $(dir ${PUZZLES}) && wget https://database.lichess.org/$(notdir ${PUZZLES}).zst
	zstd -d ${PUZZLES}.zst

evals/lichess_%_evals.csv: make-evals.sh ${PUZZLES}
	mkdir -p $(dir $@) && ./$< $(@:evals/lichess_%_evals.csv=%) > $@

# Automatically generate debug targets from *_test.cpp files
TEST_SRCS=$(wildcard src/*_test.cpp)
DEBUG_TARGETS=$(patsubst src/%_test.cpp,build/%-debug,$(TEST_SRCS))
BUILD_TARGETS=$(patsubst src/%_test.cpp,build/%-test,$(TEST_SRCS))

debug: $(DEBUG_TARGETS) build/perft-debug
build: $(BUILD_TARGETS) build/perft

searches1: build/search-debug
	./build/search-debug "5r1k/pp4pp/5p2/1BbQp1r1/7K/7P/1PP3P1/3R3R b - - 3 26" 3

searches2: build/search-test
	./build/search-test "6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1" 5

searches3: build/search-test
	./build/search-test "8/8/8/8/8/4bk1p/2R2Np1/6K1 b - - 7 62" 3

searches4: build/search-test
	./build/search-test "3r2k1/1p3ppp/pq1Q1b2/8/8/1P3N2/P4PPP/3R2K1 w - - 4 28" 3

searches5: build/search-test
	./build/search-test "8/1N3k2/6p1/8/2P3P1/pr6/R7/5K2 b - - 2 56" 5

searches6: build/search-test
	./build/search-test "r4rk1/p3ppbp/Pp3np1/3PpbB1/2q5/2N2P2/1PPQ2PP/3RR2K w - - 0 20" 1

searches: searches1 searches2 searches3 searches4 searches5 searches6

ut%.out: ut%.in build/uci-debug
	@./build/uci-debug $< 2>&1 | grep -wv "expect" > "$@"
	@grep -w "^expect" $< | \
	while read expect pattern ; do \
		grep -He "$$pattern" "$@" || \
			(echo "$@: no match for \"$$pattern\"" && cat "$@" && rm -f "$@" && false) ; \
	done

uci: $(patsubst ut%.in,ut%.out,$(wildcard ut*.in))

magic: build/magic-test
# To accept any changes on test failure, pipe the output to the `patch` command
	@(./build/magic-test | diff -u src/magic_gen.h -) && echo Magic tests passed || \
	(echo "\n*** To accept these changes, pipe this output to the patch command ***" && false)

test: build debug searches evals uci magic
	./build/fen-test
	./build/moves-test
	./build/elo-test
	./build/nnue-test
	./build/hash-test
	./build/eval-test "6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1"
