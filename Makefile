PUZZLES=lichess/lichess_db_puzzle.csv
PHASES=opening middlegame endgame
EVALS=$(foreach phase,${PHASES},lichess/lichess_${phase}_evals.csv)
CCFLAGS=-std=c++17 -Werror -Wall -Wextra
CLANGPP=clang++
GPP=g++
DEBUGFLAGS=-DDEBUG -O0 -g
OPTOBJ=build/opt
DBGOBJ=build/dbg

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

# CPP Tests
CPP_TEST_SRCS=$(wildcard src/*_test.cpp)
CPP_TESTS=$(patsubst src/%_test.cpp,build/%-test,$(CPP_TEST_SRCS))

# First argument is the object dir, second is list of source files
calc_objs=$(patsubst src/%.cpp,$(1)/%.o,$(2))
calc_deps=${calc_objs:.o=.d}

# Given a list of move source files, return these prefixed with src/
prefix_src=$(patsubst %.cpp,src/%.cpp,$(1))
# Given a test target name, return the corresponding source file name
test_src=$(patsubst %-test,%_test.cpp,$(patsubst %-debug,%_test.cpp,$(1)))
# Given a test target name, return the corresponding object directory
test_dir=$(patsubst %-test,${OPTOBJ},$(patsubst %-debug,${DBGOBJ},$(1)))
# Given a test target name and additional source files, return the list of object files
test_objs=$(call calc_objs,$(call test_dir,$(1)),$(call prefix_src,$(call test_src,$(1)) $(2)))

ALLSRCS=$(wildcard src/*.cpp)

all: debug test perft-bench perft-test perft-debug-test mate123 mate45 puzzles
	@echo "\n*** All tests passed! ***\n"

-include $(call calc_deps,${OPTOBJ},${ALLSRCS})
-include $(call calc_deps,${DBGOBJ},${ALLSRCS})

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

.deps: $(call calc_deps,${OPTOBJ},${ALLSRCS}) $(call calc_deps,${DBGOBJ},${ALLSRCS})

.SUFFIXES: # Delete the default suffix rules

clean:
	rm -fr build
	rm -f *.log
	rm -f core *.core puzzles.actual perf.data* *.ii *.bc *.s
	rm -f game.??? log.??? players.dat # XBoard outputs
	rm -f test/ut*.out
	rm -f lichess/puzzles.csv
	rm -rf *.dSYM .DS_Store

build/fen-test: $(call test_objs,fen-test,fen.cpp)
build/fen-debug: $(call test_objs,fen-debug,fen.cpp)

build/square_set-test: $(call test_objs,square_set-test,square_set.cpp fen.cpp)
build/square_set-debug: $(call test_objs,square_set-debug,square_set.cpp fen.cpp)

NNUE_SRCS=nnue.cpp nnue_stats.cpp nnue_incremental.cpp square_set.cpp fen.cpp

build/nnue-test: $(call test_objs,nnue-test,${NNUE_SRCS})
build/nnue-debug: $(call test_objs,nnue-debug,${NNUE_SRCS})

build/moves_table-test: $(call test_objs,moves_table-test,moves_table.cpp fen.cpp)
build/moves_table-debug: $(call test_objs,moves_table-debug,moves_table.cpp fen.cpp)

MOVES_SRCS=moves.cpp moves_table.cpp moves_gen.cpp magic.cpp square_set.cpp

build/moves_gen-test: $(call test_objs,moves_gen-test,${MOVES_SRCS} fen.cpp)
build/moves_gen-debug: $(call test_objs,moves_gen-debug,${MOVES_SRCS} fen.cpp)

build/moves-test: $(call test_objs,moves-test,${MOVES_SRCS} fen.cpp)
build/moves-debug: $(call test_objs,moves-debug,${MOVES_SRCS} fen.cpp)

build/hash-test: $(call test_objs,hash-test,${MOVES_SRCS} hash.cpp fen.cpp)
build/hash-debug: $(call test_objs,hash-debug,${MOVES_SRCS} hash.cpp fen.cpp)

build/magic-test: $(call test_objs,magic-test,${MOVES_SRCS})
build/magic-debug: $(call test_objs,magic-debug,${MOVES_SRCS})

EVAL_SRCS=eval.cpp hash.cpp ${NNUE_SRCS} ${MOVES_SRCS}

build/eval-test: $(call test_objs,eval-test,${EVAL_SRCS})
build/eval-debug: $(call test_objs,eval-debug,${EVAL_SRCS})

SEARCH_SRCS=${EVAL_SRCS} search.cpp

build/search-test: $(call test_objs,search-test,${SEARCH_SRCS})
build/search-debug: $(call test_objs,search-debug,${SEARCH_SRCS})

build/uci-test: $(call test_objs,uci-test,uci.cpp ${SEARCH_SRCS})
build/uci-debug: $(call test_objs,uci-debug,uci.cpp ${SEARCH_SRCS})

PERFT_SRCS=$(call prefix_src,perft.cpp perft_core.cpp ${MOVES_SRCS} fen.cpp hash.cpp)
# perft counts the total leaf nodes in the search tree for a position, see the perft-test target
build/perft: $(call calc_objs,${OPTOBJ},${PERFT_SRCS})
	${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^

PERFT_TEST_SRCS=$(call prefix_src,perft_test.cpp perft_core.cpp ${MOVES_SRCS} fen.cpp hash.cpp)
build/perft-test: $(call calc_objs,${OPTOBJ},${PERFT_TEST_SRCS})
	${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^
build/perft-debug: $(call calc_objs,${DBGOBJ},${PERFT_TEST_SRCS})
	${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $^

PERFT_SIMPLE_SRCS=$(call prefix_src,perft_simple.cpp ${MOVES_SRCS} fen.cpp)
# perft_simple is a simplified version without caching or 128-bit ints
build/perft-simple: $(call calc_objs,${OPTOBJ},${PERFT_SIMPLE_SRCS})
	${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^

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
	./$< 5 4865609 | grep nodes/sec

# Aliases for perft test targets
perft-bench: build/perft-clang-emul.ok build/perft-gcc-emul.ok build/perft-clang-sse2.ok build/perft-gcc-sse2.ok

perft-test: build/perft-test.ok

perft-debug-test: build/perft-debug.ok

# Download the lichess puzzles database if not already present. As the puzzles change over time, and
# the file is large, we don't normally clean and refetch it.
${PUZZLES}:
	mkdir -p $(dir ${PUZZLES}) && cd $(dir ${PUZZLES}) \
		&& wget https://database.lichess.org/$(notdir ${PUZZLES}).zst
	zstd -d ${PUZZLES}.zst

# Solve some known mate-in-n puzzles, for correctness of the search methods
mate123: build/search-test ${PUZZLES}
	egrep "FEN,Moves|mateIn[123]" ${PUZZLES} | head -1001 | ./build/search-test 5

mate45: build/search-test ${PUZZLES}
	egrep "FEN,Moves|mateIn[45]" ${PUZZLES} | head -101 | ./build/search-test 9

lichess/puzzles.csv: ${PUZZLES} Makefile
	egrep -v "mateIn[12345]" ${PUZZLES} | head -101 > $@

.PHONY: puzzles
puzzles: lichess/puzzles.csv build/search-test
	./build/search-test 6 < $<

lichess/lichess_%_evals.csv: make-evals.sh ${PUZZLES}
	mkdir -p $(dir $@) && ./$< $(@:lichess/lichess_%_evals.csv=%) > $@

evals: build/eval-test ${EVALS}
	./build/eval-test ${EVALS}

# Some line count statistics, requires the cloc tool, see https://github.com/AlDanial/cloc
cloc:
	@echo "\n*** C++ Source Code ***\n"
	cloc --by-percent cmb `find src -name \*.cpp -o -name \*.h | egrep -v '_test[.]|debug'` 2>/dev/null || echo "Error combining source files"
	@echo "\n*** C++ Test Code ***\n"
	cloc --by-percent cmb `find src -name \*.cpp -o -name \*.h | egrep '_test[.]|debug'` 2>/dev/null || echo "Error combining source files"

build/perft-debug.ok: build/perft-debug
	./build/perft-debug && touch $@

# Verify well-known perft results. Great for checking correct move generation.
build/perft-test.ok: build/perft build/perft-test
	./build/perft -q 5 4865609 && ./build/perft-test && touch $@

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

test/ut%.out: test/ut%.in build/uci-debug
	@./build/uci-debug $< 2>&1 | grep -wv "expect" > "$@"
	@grep -w "^expect" $< | \
	while read expect pattern ; do \
		grep -He "$$pattern" "$@" || \
			(echo "$@: no match for \"$$pattern\"" && cat "$@" && rm -f "$@" && false) ; \
	done

uci: $(patsubst test/ut%.in,test/ut%.out,$(wildcard test/ut*.in))

magic: build/magic-test
# To accept any changes on test failure, pipe the output to the `patch` command
	@(./build/magic-test --verbose | diff -u src/magic_gen.h -) && echo Magic tests passed || \
	(echo "\n*** To accept these changes, pipe this output to the patch command ***" && false)

test-cpp: build debug ${CPP_TESTS}
	@echo "Running C++ test executables..."
	@for file in ${CPP_TESTS}; do \
		/bin/echo -n Run $$file ; \
		(./$$file < /dev/null > /dev/null && echo " passed") || ./$$file </dev/null; \
	done

test: test-cpp searches evals uci magic
