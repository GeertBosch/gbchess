PUZZLES=puzzles/lichess_db_puzzle.csv
PHASES=opening middlegame endgame
EVALS=$(foreach phase,${PHASES},evals/lichess_${phase}_evals.csv)
CCFLAGS=-std=c++17 -Werror -Wall -Wextra
CLANGPP=clang++
GPP=g++
DEBUGFLAGS=-DDEBUG -O0 -g
# -fprofile-instr-generate -fcoverage-mapping
OPTOBJ=build/opt
DBGOBJ=build/dbg

# First argument is DBG or OPT, second is list of source files
calc_objs=$(patsubst %.cpp,${$(1)OBJ}/%.o,$(2))
calc_deps=${calc_objs:.o=.d}

all: debug test build perft-bench perft-test mate123 mate45 puzzles evals
	@echo "\n*** All tests passed! ***\n"

-include $(call calc_deps,OPT,$(wildcard *.cpp))
-include $(call calc_deps,DBG,$(wildcard *.cpp))

export LLVM_PROFILE_FILE=coverage-%m.profraw
LLVM-MERGE=llvm-profdata merge -sparse coverage-*.profraw -o coverage.profdata

# MacOS specific stuff - why can't thinks just work  by default?
ifeq ($(_system_type),Darwin)
    export MallocNanoZone=0
    sdk=$(shell xcrun --sdk macosx --show-sdk-path)
    arch=$(shell uname -m)
    CLANGPP:=/usr/bin/clang++
    CCFLAGS:=${CCFLAGS} -isysroot ${sdk} -mmacosx-version-min=11.0 -target darwin17.0.0 -arch ${arch} -stdlib=libc++
 .  DEBUGFLAGS:=${DEBUGFLAGS} fsanitize=address
    LINKFLAGS:=${LINKFLAGS} -stdlib=libc++ -Wl,-syslibroot,${sdk} -mmacosx-version-min=11.0 -target darwin17.0.0 -arch ${arch}
endif

${OPTOBJ}/%.d: %.cpp
	@mkdir -p ${OPTOBJ}
	${GPP} -MT $(subst .d,.o,$@) -MM ${CCFLAGS} -o $@ $< > $@
${OPTOBJ}/%.o: %.cpp ${OPTOBJ}/%.d
	@mkdir -p ${OPTOBJ}
	${GPP} -c ${CCFLAGS} -O2 -o $@ $<

${DBGOBJ}/%.d: %.cpp
	@mkdir -p ${DBGOBJ}
	${GPP} -MM ${CCFLAGS} -o $@ $< > $@

${DBGOBJ}/%.o: %.cpp
	@mkdir -p ${DBGOBJ}
	${CLANGPP} -MMD -c ${CCFLAGS} ${DEBUGFLAGS} -o $@ $<

%-test: ${OPTOBJ}/%_test.o
	${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^

%-debug: ${DBGOBJ}/%_test.o
	${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $^

ALLSRCS=$(wildcard *.cpp)

.deps: $(call calc_deps,OPT,${ALLSRCS}) $(call calc_deps,DBG,${ALLSRCS})

.SUFFIXES: # Delete the default suffix rules
.PHONY:

clean: .PHONY
	rm -fr build
	rm -f *-debug *-test perft
	rm -f *.log
	rm -f core *.core puzzles.actual perf.data* *.ii *.bc *.s
	rm -f perft-{clang,gcc}-{sse2,emul}
	rm -f *.profraw *.profdata *.gcda *.gcno lcov.info
	rm -f game.??? log.??? players.dat # XBoard outputs
	rm -f ut*.out
	rm -f puzzles.csv
	rm -rf *.dSYM

fen-test: ${OPTOBJ}/fen.o
fen-debug: ${DBGOBJ}/fen.o

MOVES_SRCS=moves.cpp magic.cpp

moves-test: $(call calc_objs,OPT,${MOVES_SRCS} fen.cpp) 
moves-debug: $(call calc_objs,DBG,${MOVES_SRCS} fen.cpp)

hash-test: $(call calc_objs,OPT,${MOVES_SRCS} hash.cpp fen.cpp)
hash-debug: $(call calc_objs,DBG,${MOVES_SRCS} hash.cpp fen.cpp)

magic-test: $(call calc_objs,OPT,${MOVES_SRCS})
magic-debug: $(call calc_objs,DBG,${MOVES_SRCS})

EVAL_SRCS=eval.cpp hash.cpp fen.cpp ${MOVES_SRCS}

eval-test: $(call calc_objs,OPT,${EVAL_SRCS})
eval-debug: $(call calc_objs,DBG,${EVAL_SRCS})

SEARCH_SRCS=${EVAL_SRCS} search.cpp

search-test: $(call calc_objs,OPT,${SEARCH_SRCS})
search-debug: $(call calc_objs,DBG,${SEARCH_SRCS})

uci-test: $(call calc_objs,OPT,uci.cpp ${SEARCH_SRCS})
uci-debug: $(call calc_objs,DBG,uci.cpp ${SEARCH_SRCS})

PERFT_SRCS=perft.cpp ${MOVES_SRCS} fen.cpp
# perft counts the total leaf nodes in the search tree for a position, see the perft-test target
perft: $(call calc_objs,OPT,${PERFT_SRCS})
	${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^
perft-debug: $(call calc_objs,DBG,${PERFT_SRCS})
	${GPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $^

# Compare the perft tool with some different compilation options for speed comparison
perft-clang-sse2: perft.cpp ${MOVES_SRCS} fen.cpp *.h
	${CLANGPP} -O3 ${CCFLAGS} -g -o $@ $(filter-out %.h,$^)
perft-clang-emul: perft.cpp  ${MOVES_SRCS} fen.cpp *.h
	${CLANGPP} -O3 -DSSE2EMUL ${CCFLAGS} -g -o $@ $(filter-out %.h,$^)
perft-gcc-sse2: perft.cpp  ${MOVES_SRCS} fen.cpp *.h
	${GPP} -O3 ${CCFLAGS} -g -o $@ $(filter-out %.h,$^)
perft-gcc-emul: perft.cpp  ${MOVES_SRCS} fen.cpp *.h
	${GPP} -O3 -DSSE2EMUL ${CCFLAGS} -g -o $@ $(filter-out %.h,$^)

perft-emul: perft-clang-emul perft-gcc-emul .PHONY
	./perft-clang-emul -q 5 4865609
	./perft-gcc-emul -q 5 4865609

perft-sse2: perft-clang-sse2 perft-gcc-sse2 .PHONY
	./perft-clang-sse2 -q 5 4865609
	./perft-gcc-sse2 -q 5 4865609

perft-bench: perft-emul perft-sse2

# Solve some known mate-in-n puzzles, for correctness of the search methods
mate123: search-test ${PUZZLES} .PHONY
	egrep "FEN,Moves|mateIn[123]" ${PUZZLES} | head -1001 | ./search-test 5

mate45: search-test ${PUZZLES} .PHONY
	egrep "FEN,Moves|mateIn[45]" ${PUZZLES} | head -101 | ./search-test 9

puzzles.csv: ${PUZZLES} Makefile
	egrep -v "mateIn[12345]" ${PUZZLES} | head -101 > $@

puzzles: puzzles.csv search-test .PHONY
	./search-test 6 < $<

evals: eval-test ${EVALS} .PHONY
	./eval-test ${EVALS}

# Some line count statistics, requires the cloc tool, see https://github.com/AlDanial/cloc
cloc:
	@echo "\n*** Excluding Tests ***\n"
	cloc --by-percent cmb `find . -name \*.cpp -o -name \*.h | egrep -v '_test[.]|debug'`
	@echo "\n*** Just Tests ***\n"
	cloc --by-percent cmb `find . -name \*.cpp -o -name \*.h | egrep '_test[.]|debug'`

# The following are well known perft positions, see https://www.chessprogramming.org/Perft_Results
kiwipete="r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
position3="8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
position4="r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"
position4m="r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1"
position5="rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
position6="r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"

# Verify well-known perft results. Great for checking correct move generation.
perft-test: perft perft-debug
	./perft-debug 3
	./perft-debug 4 197281
	./perft -q 5 4865609
	./perft -q "rnbqkbnr/1ppppppp/B7/p7/4P3/8/PPPP1PPP/RNBQK1NR b KQkq - 1 2" 4 509448
	./perft -q ${kiwipete} 4 4085603
	./perft -q ${position3} 5 674624
	./perft -q ${position4} 4 422333
	./perft -q ${position4m} 4 422333
	./perft -q ${position5} 4 2103487
	./perft -q ${position6} 4 3894594

${PUZZLES}:
	mkdir -p $(dir ${PUZZLES}) && cd $(dir ${PUZZLES}) && wget https://database.lichess.org/$(notdir ${PUZZLES}).zst
	zstd -d ${PUZZLES}.zst

evals/lichess_%_evals.csv: make-evals.sh ${PUZZLES}
	mkdir -p $(dir $@) && ./$< $(@:evals/lichess_%_evals.csv=%) > $@

# Automatically generate debug targets from *_test.cpp files
TEST_SRCS=$(wildcard *_test.cpp)
DEBUG_TARGETS=$(patsubst %_test.cpp,%-debug,$(TEST_SRCS))
BUILD_TARGETS=$(patsubst %_test.cpp,%-test,$(TEST_SRCS))

debug: $(DEBUG_TARGETS) perft-debug
build: $(BUILD_TARGETS) perft

searches1: search-debug
	./search-debug "5r1k/pp4pp/5p2/1BbQp1r1/7K/7P/1PP3P1/3R3R b - - 3 26" 3

searches2: search-test
	./search-test "6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1" 5

searches3: search-test
	./search-test "8/8/8/8/8/4bk1p/2R2Np1/6K1 b - - 7 62" 3

searches4: search-test
	./search-test "3r2k1/1p3ppp/pq1Q1b2/8/8/1P3N2/P4PPP/3R2K1 w - - 4 28" 3

searches5: search-test
	./search-test "8/1N3k2/6p1/8/2P3P1/pr6/R7/5K2 b - - 2 56" 5

searches6: search-test
	./search-test "r4rk1/p3ppbp/Pp3np1/3PpbB1/2q5/2N2P2/1PPQ2PP/3RR2K w - - 0 20" 1

searches: searches1 searches2 searches3 searches4 searches5 searches6

ut%.out: ut%.in uci-debug
	@./uci-debug $< 2>&1 | grep -wv "expect" > "$@"
	@grep -w "^expect" $< | \
	while read expect pattern ; do \
		grep -He "$$pattern" "$@" || \
			(echo "$@: no match for \"$$pattern\"" && cat "$@" && rm -f "$@" && false) ; \
	done

uci: $(patsubst ut%.in,ut%.out,$(wildcard ut*.in))

magic: magic-test
# To accept any changes on test failure, pipe the output to the `patch` command
	@(./magic-test | diff -u magic_gen.h -) && echo Magic tests passed || \
	(echo "\n*** To accept these changes, pipe this output to the patch command ***" && false)

test: build debug searches evals uci magic
	rm -f coverage-*.profraw
	./fen-test
	./moves-test
	./elo-test
	./eval-test "6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1"

coverage: test
	${LLVM-MERGE}
	llvm-cov report ./fen-test -instr-profile=coverage.profdata
	llvm-cov report ./moves-test -instr-profile=coverage.profdata
	llvm-cov report ./elo-test -instr-profile=coverage.profdata
	llvm-cov report ./eval-test -instr-profile=coverage.profdata
	llvm-cov report ./search-debug -instr-profile=coverage.profdata
