PUZZLES=lichess/lichess_db_puzzle.csv
FIXED_PUZZLES=lichess/fixed_puzzles.csv
PHASES=opening middlegame endgame
EVALS=$(foreach phase,${PHASES},lichess/lichess_${phase}_evals.csv)
CCFLAGS=-std=c++17 -Werror -Wall -Wextra
CLANGPP=clang++
GPP=g++
DEBUGFLAGS=-DDEBUG -O0 -g
OPTOBJ=build/opt
DBGOBJ=build/dbg
COMPILE_COMMANDS=build/compile_commands.json

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
CPP_TEST_SRCS=$(wildcard src/*_test.cpp src/*/*_test.cpp src/*/*/*_test.cpp)
CPP_TESTS=$(patsubst %_test.cpp,build/%-test,$(foreach test, $(CPP_TEST_SRCS),$(notdir $(test))))

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

ALLSRCS=$(wildcard src/*.cpp src/*/*.cpp src/*/*/*.cpp)

all: build debug test perft-bench perft-test mate123 mate45 puzzles
	@echo "\n✅ All tests passed!\n"

-include $(call calc_deps,${OPTOBJ},${ALLSRCS})
-include $(call calc_deps,${DBGOBJ},${ALLSRCS})

${OPTOBJ}/%.d: src/%.cpp
	@mkdir -p $(dir $@)
	@${GPP} -MT $(subst .d,.o,$@) -MM ${CCFLAGS} -Isrc -o $@ $< > $@

${OPTOBJ}/%.o: src/%.cpp ${OPTOBJ}/%.d
	@mkdir -p $(dir $@)
	@${GPP} -c ${CCFLAGS} -Isrc -O2 -o $@ $<

${DBGOBJ}/%.d: src/%.cpp
	@mkdir -p $(dir $@)
	@${GPP} -MM ${CCFLAGS} -Isrc -o $@ $< > $@

${DBGOBJ}/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	@${CLANGPP} -MMD -c ${CCFLAGS} ${DEBUGFLAGS} -Isrc -o $@ $<

${OPTOBJ}/%-test: ${OPTOBJ}/%_test.o
	@mkdir -p build
	@${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^
	@ln -sf $$(echo "$@" | sed 's|build/||') build/$(notdir $@)

${DBGOBJ}/%-debug: ${DBGOBJ}/%_test.o
	@mkdir -p build
	@${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $^
	@ln -sf $$(echo "$@" | sed 's|build/||') build/$(notdir $@)

# Test dependency definitions
NNUE_SRCS=eval/nnue/nnue.cpp eval/nnue/nnue_stats.cpp eval/nnue/nnue_incremental.cpp core/square_set/square_set.cpp
MOVES_SRCS=move/move.cpp move/move_table.cpp move/move_gen.cpp move/magic/magic.cpp core/square_set/square_set.cpp
EVAL_SRCS=eval/eval.cpp core/hash/hash.cpp ${NNUE_SRCS} ${MOVES_SRCS}
BOOK_SRCS=book/book.cpp book/pgn/pgn.cpp engine/fen/fen.cpp
SEARCH_SRCS=${EVAL_SRCS} search/search.cpp
ENGINE_SRCS=engine/engine.cpp engine/perft/perft_core.cpp ${BOOK_SRCS} ${SEARCH_SRCS}

define test_rules
${OPTOBJ}/$(1)-test: $$(call test_objs,$(1)-test,$(2))
${DBGOBJ}/$(1)-debug: $$(call test_objs,$(1)-debug,$(2))
build/$(notdir $1)-test: ${OPTOBJ}/$(1)-test
build/$(notdir $1)-debug: ${DBGOBJ}/$(1)-debug
endef

build/engine: $(call calc_objs,${OPTOBJ},$(call prefix_src,${ENGINE_SRCS}))
	@${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^
build/engine-debug: $(call calc_objs,${DBGOBJ},$(call prefix_src,${ENGINE_SRCS}))
	@${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $^

BOOK_GEN_SRCS=book/book_gen.cpp book/pgn/pgn.cpp engine/fen/fen.cpp core/hash/hash.cpp ${MOVES_SRCS}
build/book-gen: $(call calc_objs,${OPTOBJ},$(call prefix_src,${BOOK_GEN_SRCS}))
	@${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^

# Generate book.csv from all PGN files in lichess directory
LICHESS_PGNS=$(wildcard lichess/*.pgn)
book.csv: build/book-gen ${LICHESS_PGNS}
	@echo "Generating book.csv from $(words ${LICHESS_PGNS}) PGN files..."
	@./build/book-gen ${LICHESS_PGNS} book.csv

$(eval $(call test_rules,core/core))
$(eval $(call test_rules,engine/fen/fen,engine/fen/fen.cpp))
$(eval $(call test_rules,core/hash/hash,core/hash/hash.cpp ${MOVES_SRCS} engine/fen/fen.cpp))
$(eval $(call test_rules,engine/uci/uci,engine/uci/uci.cpp ${SEARCH_SRCS} engine/fen/fen.cpp))
$(eval $(call test_rules,eval/eval,eval/eval.cpp ${EVAL_SRCS} engine/fen/fen.cpp))
$(eval $(call test_rules,core/square_set/square_set,core/square_set/square_set.cpp engine/fen/fen.cpp))
$(eval $(call test_rules,search/search,search/search.cpp ${SEARCH_SRCS} book/pgn/pgn.cpp engine/fen/fen.cpp))
$(eval $(call test_rules,move/move,move/move.cpp ${MOVES_SRCS} engine/fen/fen.cpp))
$(eval $(call test_rules,move/move_gen,move/move_gen.cpp ${MOVES_SRCS} engine/fen/fen.cpp))
$(eval $(call test_rules,move/move_table,move/move_table.cpp engine/fen/fen.cpp))
$(eval $(call test_rules,move/magic/magic,move/magic/magic.cpp ${MOVES_SRCS} engine/fen/fen.cpp))
$(eval $(call test_rules,eval/nnue/nnue,${NNUE_SRCS} engine/fen/fen.cpp))
$(eval $(call test_rules,search/elo,))
$(eval $(call test_rules,book/pgn/pgn,${MOVES_SRCS} book/pgn/pgn.cpp))
$(eval $(call test_rules,book/book,${BOOK_SRCS} ${MOVES_SRCS} core/hash/hash.cpp))

.deps: $(call calc_deps,${OPTOBJ},${ALLSRCS}) $(call calc_deps,${DBGOBJ},${ALLSRCS})

.SUFFIXES: # Delete the default suffix rules

clean:
	rm -fr build
	rm -f *.log
	rm -f core *.core puzzles.actual perf.data* *.ii *.bc *.s
	rm -f game.??? log.??? players.dat # XBoard outputs
	rm -rf test/out *.dSYM .DS_Store
	rm -f $(COMPILE_COMMANDS)

realclean: clean
	rm -f lichess/*.csv

PERFT_SRCS=$(call prefix_src,engine/perft/perft.cpp engine/perft/perft_core.cpp ${MOVES_SRCS} engine/fen/fen.cpp core/hash/hash.cpp)
# perft counts the total leaf nodes in the search tree for a position, see the perft-test target
build/perft: $(call calc_objs,${OPTOBJ},${PERFT_SRCS})
	@${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^

PERFT_TEST_SRCS=$(call prefix_src,engine/perft/perft_test.cpp engine/perft/perft_core.cpp ${MOVES_SRCS} engine/fen/fen.cpp core/hash/hash.cpp)
build/perft-test: $(call calc_objs,${OPTOBJ},${PERFT_TEST_SRCS})
	@${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^
build/perft-debug: $(call calc_objs,${DBGOBJ},${PERFT_TEST_SRCS})
	@${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $^

PERFT_SIMPLE_SRCS=$(call prefix_src,engine/perft/perft_simple.cpp ${MOVES_SRCS} engine/fen/fen.cpp)
# perft_simple is a simplified version without caching or 128-bit ints
build/perft-simple: $(call calc_objs,${OPTOBJ},${PERFT_SIMPLE_SRCS})
	@${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^

# Build the perft tool with some different compilation options for speed comparison
build/perft-clang-sse2: ${PERFT_SRCS} src/core/*.h src/core/square_set/*.h src/engine/fen/*.h src/core/hash/*.h src/engine/perft/*.h src/move/*.h src/search/*.h
	@mkdir -p build
	@${CLANGPP} -O3 ${CCFLAGS} -Isrc -g -o $@ $(filter-out %.h,$^)
build/perft-clang-emul:  ${PERFT_SRCS} src/core/*.h src/core/square_set/*.h src/engine/fen/*.h src/core/hash/*.h src/engine/perft/*.h src/move/*.h src/search/*.h
	@mkdir -p build
	@${CLANGPP} -O3 -DSSE2EMUL ${CCFLAGS} -Isrc -g -o $@ $(filter-out %.h,$^)
build/perft-gcc-sse2:  ${PERFT_SRCS} src/core/*.h src/core/square_set/*.h src/engine/fen/*.h src/core/hash/*.h src/engine/perft/*.h src/move/*.h src/search/*.h
	@mkdir -p build
	@${GPP} -O3 ${CCFLAGS} -Isrc -g -o $@ $(filter-out %.h,$^)
build/perft-gcc-emul:  ${PERFT_SRCS} src/core/*.h src/core/square_set/*.h src/engine/fen/*.h src/core/hash/*.h src/engine/perft/*.h src/move/*.h src/search/*.h
	@mkdir -p build
	@${GPP} -O3 -DSSE2EMUL ${CCFLAGS} -Isrc -g -o $@ $(filter-out %.h,$^)

# Test the Kiwipete position at depth 4 as it exercises captures, en passant, castling,
# promotions, checks, discovered checks, double checks, checkmates, etc at low depth.
KIWIPETE=r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
build/perft-%.ok: build/perft-%
	@./$< "$(KIWIPETE)" 5 > $(basename $@).out
	@grep -q "Nodes searched: 193690690" $(basename $@).out && touch $@
	@grep -H nodes/sec $(basename $@).out || true

build/perft.ok: build/perft
	@./build/perft "$(KIWIPETE)" 5 | grep -q "Nodes searched: 193690690" && touch $@

# Aliases for perft test targets
perft-bench: build/perft-clang-emul.ok build/perft-gcc-emul.ok build/perft-clang-sse2.ok build/perft-gcc-sse2.ok

perft-test: build/perft.ok build/perft-test.ok build/perft-debug.ok build/perft-simple.ok

# Download the lichess puzzles database if not already present. As the puzzles change over time, and
# the file is large, we don't normally clean and refetch it.
${PUZZLES}: ${PUZZLES}.zst
	zstd -d $<

${PUZZLES}.zst:
	mkdir -p $(dir ${PUZZLES}) && cd $(dir ${PUZZLES}) \
		&& wget https://database.lichess.org/$(notdir ${PUZZLES}).zst

# Create derived puzzle files for different test targets
build/mate123.csv: ${PUZZLES}
	@mkdir -p build
	@egrep "FEN,Moves|mateIn[123]" ${PUZZLES} | head -4001 > $@

build/mate45.csv: ${PUZZLES}
	@mkdir -p build
	@egrep "FEN,Moves|mateIn[45]" ${PUZZLES} | head -101 > $@

build/puzzles.csv: ${PUZZLES}
	@mkdir -p build
	@egrep -v "mateIn[12345]" ${PUZZLES} | head -101 > $@

# Solve some known mate-in-n puzzles, for correctness of the search methods
mate123: build/search-test build/mate123.csv
	@./build/search-test 7 build/mate123.csv

mate45: build/search-test build/mate45.csv
	@./build/search-test 11 build/mate45.csv

.PHONY: puzzles build
puzzles: build/search-test build/puzzles.csv
	@./build/search-test 7 build/puzzles.csv

lichess/lichess_%_evals.csv: make-evals.sh ${PUZZLES}
	mkdir -p $(dir $@) && ./$< $(@:lichess/lichess_%_evals.csv=%) > $@

evals: build/eval-test ${EVALS}
	@./build/eval-test ${EVALS}

# Some line count statistics, requires the cloc tool, see https://github.com/AlDanial/cloc
cloc:
	@echo "\n*** C++ Source Code ***\n"
	cloc --by-percent cmb `find src -name \*.cpp -o -name \*.h | egrep -v '_test[.]|debug'` 2>/dev/null || echo "Error combining source files"
	@echo "\n*** C++ Test Code ***\n"
	cloc --by-percent cmb `find src -name \*.cpp -o -name \*.h | egrep '_test[.]|debug'` 2>/dev/null || echo "Error combining source files"

# Verify well-known perft results. Great for checking correct move generation.
build/perft-test.ok: build/perft-test
	./build/perft-test && touch $@
build/perft-debug.ok: build/perft-debug
	./build/perft-debug && touch $@

debug: $(patsubst %-test,%-debug,$(CPP_TESTS)) build/perft-debug build/engine-debug
build: $(CPP_TESTS) $(COMPILE_COMMANDS) build/perft build/engine build/perft-simple
	@echo "\n✅ Build complete\n"
	@./check-arch.sh

fixed-puzzles: build/search-test
	./build/search-test 7 < ${FIXED_PUZZLES}

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

test/out/uci-%.out: test/uci-%.in build/engine
	@mkdir -p test/out
	@./build/engine $< 2>&1 | grep -wv "expect" > "$@"

.PHONY: uci
uci: $(patsubst test/uci-%.in,test/out/uci-%.out,$(wildcard test/uci-*.in))
	@./test/check-uci.sh

magic: build/magic-test
# To accept any changes on test failure, pipe the output to the `patch` command
	@(./build/magic-test --verbose | diff -u src/move/magic/magic_gen.h -) \
	&& echo Magic tests passed \
	|| (echo "\n*** To accept these changes, pipe this output to the patch command ***" && false)

test-cpp: build ${CPP_TESTS} book.csv
	@(cd build && echo Symlinking NNUE files && ln -fs ../*.nnue .)
	@(cd build && echo Symlinking book files && ln -fs ../book.csv .)
	@echo "Checking that all C++ unit tests have been built"
	@find src -name "*_test.cpp" -exec basename {} _test.cpp \; | sort > .test_sources.tmp
	@find build -depth 1 -a -name "*-test" -exec basename {} -test \; | sort > .test_executables.tmp
	@diff -u .test_sources.tmp .test_executables.tmp \
		&& (echo "All C++ unit tests have been built" \
			&& rm -f .test_sources.tmp .test_executables.tmp) \
		|| (echo "\n*** Extra or missing C++ unit tests! ***\n"  && false)
	@echo ${CPP_TESTS}
	@echo "Running C++ unit test executables..."
	@(cd build && failed=0; for file in *-test; do \
		/bin/echo -n Run $$file ; \
		if ./$$file < /dev/null > /dev/null; then \
			echo " passed"; \
		else \
			echo " FAILED"; \
			./$$file < /dev/null || true; \
			failed=1; \
		fi; \
	done; \
	if [ $$failed -eq 0 ]; then \
		echo "\n✅ All C++ unit tests passed!\n"; \
	else \
		echo "\n❌ Some C++ unit tests failed!\n"; \
		exit 1; \
	fi)

test: test-cpp fixed-puzzles searches evals uci magic

# Generate compile_commands.json for clangd
$(COMPILE_COMMANDS):
	@mkdir -p build
	@echo '[' > $@
	@first=true; \
	for src in $(wildcard src/*.cpp src/*/*.cpp src/*/*/*.cpp); do \
		[ "$$first" = true ] && first=false || echo ',' >> $@; \
		echo '  {' >> $@; \
		echo "    \"directory\": \"$(PWD)\"," >> $@; \
		echo "    \"command\": \"$(CLANGPP) $(CCFLAGS) -Isrc -c $$src\"," >> $@; \
		echo "    \"file\": \"$$src\"" >> $@; \
		printf '  }' >> $@; \
	done
	@echo >> $@
	@echo ']' >> $@

.PHONY: ${COMPILE_COMMANDS}
