PUZZLES=lichess/lichess_db_puzzle.csv
FIXED_PUZZLES=lichess/fixed_puzzles.csv
CI_MATE123_PUZZLES=lichess/ci_mate123_4000.csv
CI_MATE45_PUZZLES=lichess/ci_mate45_100.csv
CI_NONMATE_PUZZLES=lichess/ci_nonmate_100.csv
PHASES=opening middlegame endgame
EVALS=$(foreach phase,${PHASES},lichess/lichess_${phase}_evals.csv)
CCFLAGS=-std=c++17 -Werror -Wall -Wextra
CLANGPP=clang++
GPP=g++
DEBUGFLAGS=-DDEBUG -O0 -g
OPTOBJ=build/opt
DBGOBJ=build/dbg
COMPILE_COMMANDS=build/compile_commands.json
NNUE_URL=https://raw.githubusercontent.com/official-stockfish/networks/refs/heads/master/nn-82215d0fd0df.nnue
NNUE_FILE=$(notdir ${NNUE_URL})

Q := @
VOPT :=
# Redirect output to log files and just report on overall pass / fail

ifndef V
	REDIR = > $@ && echo "  ✅ $@ passed" | tee -a $@ || { echo "  ❌ error in $@" | tee -a $@ ; mv $@ $(basename $@).log ; false; }
else
	Q :=
	REDIR = | tee -a $@ && echo "  ✅ $@ passed" | tee -a $@ || { echo "  ❌ error in $@" | tee -a $@ ; mv $@ $(basename $@).log ; false; }
	VOPT := -v
endif

RUNCMD = $(Q)$(1) || { echo "  ❌ error making $@"; false; }

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

all: .deps build debug test perft-bench perft-test build/mate123.out build/mate45.out build/puzzles.out
	@echo "✅ All tests passed!"

-include $(call calc_deps,${OPTOBJ},${ALLSRCS})
-include $(call calc_deps,${DBGOBJ},${ALLSRCS})

${OPTOBJ}/%.d: src/%.cpp
	$(Q)mkdir -p $(dir $@)
	$(call RUNCMD,${GPP} -MT $(subst .d,.o,$@) -MM ${CCFLAGS} -Isrc -o $@ $< > $@)

${OPTOBJ}/%.o: src/%.cpp ${OPTOBJ}/%.d
	$(Q)mkdir -p $(dir $@)
	$(call RUNCMD,${GPP} -c ${CCFLAGS} -Isrc -O2 -o $@ $<)

${DBGOBJ}/%.d: src/%.cpp
	$(Q)mkdir -p $(dir $@)
	$(call RUNCMD,${GPP} -MM ${CCFLAGS} -Isrc -o $@ $< > $@)

# By making debug objects depend on the optimized objects, most compiler errors will only be
# reported once, and final compilation speed appears effectively unchanged.
${DBGOBJ}/%.o: src/%.cpp # ${OPTOBJ}/%.o
	$(Q)mkdir -p $(dir $@)
	$(call RUNCMD,${CLANGPP} -MMD -c ${CCFLAGS} ${DEBUGFLAGS} -Isrc -o $@ $<)

${OPTOBJ}/%-test: ${OPTOBJ}/%_test.o
	$(Q)mkdir -p build
	$(call RUNCMD,${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^)
	$(Q)ln -sf $$(echo "$@" | sed 's|build/||') build/$(notdir $@)
	@echo "  ✅ build/$(notdir $@) built"

${DBGOBJ}/%-debug: ${DBGOBJ}/%_test.o
	$(Q)mkdir -p build
	$(call RUNCMD,${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $^)
	$(Q)ln -sf $$(echo "$@" | sed 's|build/||') build/$(notdir $@)
	@echo "  ✅ build/$(notdir $@) built"

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
	$(call RUNCMD,${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^)
build/engine-debug: $(call calc_objs,${DBGOBJ},$(call prefix_src,${ENGINE_SRCS}))
	$(call RUNCMD,${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $^)

BOOK_GEN_SRCS=book/book_gen.cpp book/pgn/pgn.cpp engine/fen/fen.cpp core/hash/hash.cpp ${MOVES_SRCS}
build/book-gen: $(call calc_objs,${OPTOBJ},$(call prefix_src,${BOOK_GEN_SRCS}))
	$(call RUNCMD,${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^)
build/book-gen-debug: $(call calc_objs,${DBGOBJ},$(call prefix_src,${BOOK_GEN_SRCS}))
	$(call RUNCMD,${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $^)

LAST_24_MONTHS := $(shell for i in $$(seq 1 24); do date -d "$$i month ago" +%Y-%m 2>/dev/null || date -v-$$im +%Y-%m 2>/dev/null; done | xargs)
BROADCAST_FILES := $(addprefix lichess/lichess_db_broadcast_,$(addsuffix .pgn,$(LAST_24_MONTHS)))

# Generate book.csv from all PGN files in lichess directory
LICHESS_PGNS=$(wildcard lichess/*.pgn) $(BROADCAST_FILES)

lichess/lichess_db_broadcast_%.pgn: lichess/lichess_db_broadcast_%.pgn.zst
	$(call RUNCMD,zstd -d -f -k "$<" -o "$@")
	$(Q)touch "$@"
lichess/lichess_db_broadcast_%.pgn.zst:
	$(Q)
	mkdir -p lichess
	cd lichess && wget -q https://database.lichess.org/broadcast/$(notdir $@)

book.csv build/book.out: build/book-gen ${LICHESS_PGNS}
	$(Q)echo "Generating book.csv from $(words ${LICHESS_PGNS}) PGN files..." > build/book.out
	$(Q)./build/book-gen ${LICHESS_PGNS} book.csv >> build/book.out 2>&1 \
		&& echo "  ✅ build/book.out passed" | tee -a build/book.out \
		|| { echo "  ❌ error in build/book.out" | tee -a build/book.out ; false; }
	$(Q)[ -n "$(V)" ] && cat build/book.out || true

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
	$(Q)./check-arch.sh
	@echo  "\n✅ All dependencies up to date"

.SUFFIXES: # Delete the default suffix rules

clean:
	rm -fr build
	rm -f *.log
	rm -f core *.core puzzles.actual perf.data* *.ii *.bc *.s
	rm -f game.??? log.??? players.dat # XBoard outputs
	rm -rf test/out *.dSYM .DS_Store
	rm -f $(COMPILE_COMMANDS)
	rm -f book.csv

realclean: clean
	rm -f lichess/*.csv

PERFT_SRCS=$(call prefix_src,engine/perft/perft.cpp engine/perft/perft_core.cpp ${MOVES_SRCS} engine/fen/fen.cpp core/hash/hash.cpp)
# perft counts the total leaf nodes in the search tree for a position, see the perft-test target
build/perft: $(call calc_objs,${OPTOBJ},${PERFT_SRCS})
	$(call RUNCMD,${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^)

PERFT_TEST_SRCS=$(call prefix_src,engine/perft/perft_test.cpp engine/perft/perft_core.cpp ${MOVES_SRCS} engine/fen/fen.cpp core/hash/hash.cpp)
build/perft-test: $(call calc_objs,${OPTOBJ},${PERFT_TEST_SRCS})
	$(call RUNCMD,${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^)
build/perft-debug: $(call calc_objs,${DBGOBJ},${PERFT_TEST_SRCS})
	$(call RUNCMD,${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $^)

PERFT_SIMPLE_SRCS=$(call prefix_src,engine/perft/perft_simple.cpp ${MOVES_SRCS} engine/fen/fen.cpp)
# perft_simple is a simplified version without caching or 128-bit ints
build/perft-simple: $(call calc_objs,${OPTOBJ},${PERFT_SIMPLE_SRCS})
	$(call RUNCMD,${GPP} ${CCFLAGS} -O2 ${LINKFLAGS} -o $@ $^)

# Build the perft tool with some different compilation options for speed comparison
build/perft-clang-sse2: ${PERFT_SRCS} src/core/*.h src/core/square_set/*.h src/engine/fen/*.h src/core/hash/*.h src/engine/perft/*.h src/move/*.h src/search/*.h
	$(Q)mkdir -p build
	$(call RUNCMD,${CLANGPP} -O3 ${CCFLAGS} -Isrc -g -o $@ $(filter-out %.h,$^))
build/perft-clang-emul:  ${PERFT_SRCS} src/core/*.h src/core/square_set/*.h src/engine/fen/*.h src/core/hash/*.h src/engine/perft/*.h src/move/*.h src/search/*.h
	$(Q)mkdir -p build
	$(call RUNCMD,${CLANGPP} -O3 -DSSE2EMUL ${CCFLAGS} -Isrc -g -o $@ $(filter-out %.h,$^))
build/perft-gcc-sse2:  ${PERFT_SRCS} src/core/*.h src/core/square_set/*.h src/engine/fen/*.h src/core/hash/*.h src/engine/perft/*.h src/move/*.h src/search/*.h
	$(Q)mkdir -p build
	$(call RUNCMD,${GPP} -O3 ${CCFLAGS} -Isrc -g -o $@ $(filter-out %.h,$^))
build/perft-gcc-emul:  ${PERFT_SRCS} src/core/*.h src/core/square_set/*.h src/engine/fen/*.h src/core/hash/*.h src/engine/perft/*.h src/move/*.h src/search/*.h
	$(Q)mkdir -p build
	$(call RUNCMD,${GPP} -O3 -DSSE2EMUL ${CCFLAGS} -Isrc -g -o $@ $(filter-out %.h,$^))

# Test the Kiwipete position at depth 4 as it exercises captures, en passant, castling,
# promotions, checks, discovered checks, double checks, checkmates, etc at low depth.
KIWIPETE=r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
build/perft-%.out: build/perft-%
	$(Q)./$< -q "$(KIWIPETE)" 5 | grep -q "Nodes searched: 193690690" $(REDIR)

build/perft.out: build/perft
	$(Q)./build/perft -q "$(KIWIPETE)" 5 | grep -q "Nodes searched: 193690690" $(REDIR)

# Aliases for perft test targets
perft-bench: build/perft-clang-emul.out build/perft-gcc-emul.out build/perft-clang-sse2.out build/perft-gcc-sse2.out

perft-test: build/perft.out build/perft-test.out build/perft-debug.out build/perft-simple.out

# Download the lichess puzzles database if not already present. As the puzzles change over time, and
# the file is large, we don't normally clean and refetch it.
${PUZZLES}: ${PUZZLES}.zst
	$(call RUNCMD,zstd -d -f -k "$<" -o "$@")
	$(Q)touch "$@"

${PUZZLES}.zst:
	$(Q)mkdir -p $(dir ${PUZZLES}) && cd $(dir ${PUZZLES}) \
		&& wget -q https://database.lichess.org/$(notdir ${PUZZLES}).zst

# Solve some known mate-in-n puzzles, for correctness of the search methods
build/mate123.out: build/search-test ${CI_MATE123_PUZZLES} ${NNUE_FILE}
	$(Q)./build/search-test $(VOPT) 7 ${CI_MATE123_PUZZLES} $(REDIR)

build/mate45.out: build/search-test ${CI_MATE45_PUZZLES} ${NNUE_FILE}
	$(Q)./build/search-test $(VOPT) 11 ${CI_MATE45_PUZZLES} $(REDIR)

.PHONY: build
build/puzzles.out: build/search-test ${CI_NONMATE_PUZZLES} ${NNUE_FILE}
	$(Q)./build/search-test $(VOPT) 7 ${CI_NONMATE_PUZZLES} $(REDIR)

${NNUE_FILE}:
	$(Q)wget -q ${NNUE_URL}

lichess/lichess_%_evals.csv: make-evals.sh ${PUZZLES}
	mkdir -p $(dir $@) && ./$< $(@:lichess/lichess_%_evals.csv=%) > $@

build/evals.out: build/eval-test ${EVALS}
	$(Q)./build/eval-test ${EVALS} $(REDIR)

# Some line count statistics, requires the cloc tool, see https://github.com/AlDanial/cloc
cloc:
	$(Q)echo "\n*** C++ Source Code ***\n"
	cloc --by-percent cmb `find src -name \*.cpp -o -name \*.h | egrep -v '_test[.]|debug'` 2>/dev/null || echo "Error combining source files"
	$(Q)echo "\n*** C++ Test Code ***\n"
	cloc --by-percent cmb `find src -name \*.cpp -o -name \*.h | egrep '_test[.]|debug'` 2>/dev/null || echo "Error combining source files"

# Verify well-known perft results. Great for checking correct move generation.
build/perft-test.out: build/perft-test
	$(Q)./build/perft-test $(REDIR)
build/perft-debug.out: build/perft-debug
	$(Q)./build/perft-debug $(REDIR)

debug: $(patsubst %-test,%-debug,$(CPP_TESTS)) build/perft-debug build/engine-debug build/book-gen-debug
build: .deps $(CPP_TESTS) $(COMPILE_COMMANDS) build/perft build/engine build/perft-simple build/book-gen
	$(Q)echo "\n✅ Build complete\n"

build/fixed-puzzles.out: build/search-test $(FIXED_PUZZLES) ${NNUE_FILE}
	$(Q)./build/search-test $(VOPT) 7 < $(FIXED_PUZZLES) $(REDIR)

build/searches.out: build/search-debug ${NNUE_FILE}
	$(Q){ \
		./build/search-debug "5r1k/pp4pp/5p2/1BbQp1r1/7K/7P/1PP3P1/3R3R b - - 3 26" 3; \
		./build/search-test $(VOPT) "6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1" 5; \
		./build/search-test $(VOPT) "8/8/8/8/8/4bk1p/2R2Np1/6K1 b - - 7 62" 3; \
		./build/search-test $(VOPT) "3r2k1/1p3ppp/pq1Q1b2/8/8/1P3N2/P4PPP/3R2K1 w - - 4 28" 3; \
		./build/search-test $(VOPT) "8/1N3k2/6p1/8/2P3P1/pr6/R7/5K2 b - - 2 56" 5; \
		./build/search-test $(VOPT) "r4rk1/p3ppbp/Pp3np1/3PpbB1/2q5/2N2P2/1PPQ2PP/3RR2K w - - 0 20" 1; \
	} $(REDIR)

searches: build/searches.out

build/uci-%.out: test/uci-%.in build/engine ${NNUE_FILE}
	$(Q)./build/engine $< 2>&1 | grep -wv "expect" $(REDIR)

build/uci.out: $(patsubst test/uci-%.in,build/uci-%.out,$(wildcard test/uci-*.in))
	$(Q)./test/check-uci.sh ${REDIR}

uci: build/uci.out

build/magic.out: build/magic-test
# To accept any changes on test failure, pipe the output to the `patch` command
	$(Q) { (./build/magic-test --verbose | diff -u src/move/magic/magic_gen.h -) 2>&1 \
	&& echo Magic tests passed \
	|| (echo "\n*** To accept these changes, pipe this output to the patch command ***" && false) \
	} $(REDIR)

build/test-cpp.out: ${CPP_TESTS} book.csv ${NNUE_FILE}
	$(Q){ \
		(cd build && echo Symlinking NNUE files && ln -fs ../*.nnue .); \
		(cd build && echo Symlinking book files && ln -fs ../book.csv .); \
		echo "Checking that all C++ unit tests have been built"; \
		find src -name "*_test.cpp" -exec basename {} _test.cpp \; | sort > .test_sources.tmp; \
		find build -depth 1 -a -name "*-test" -exec basename {} -test \; | sort > .test_executables.tmp; \
		diff -u .test_sources.tmp .test_executables.tmp \
			&& (echo "All C++ unit tests have been built" \
				&& rm -f .test_sources.tmp .test_executables.tmp) \
			|| (echo "\n*** Extra or missing C++ unit tests! ***\n"  && false); \
		echo ${CPP_TESTS}; \
		echo "Running C++ unit test executables..."; \
		(cd build && failed=0; for file in *-test; do \
			/bin/echo -n Run $$file ; \
			if ./$$file < /dev/null > /dev/null; then \
				echo " passed"; \
			else \
				echo " FAILED"; \
				./$$file < /dev/null || true; \
				failed=1; \
			fi; \
		done; \
		if [ $$failed -ne 0 ]; then \
			echo "\n❌ $$failed C++ unit tests failed!\n"; \
			exit 1; \
		fi); \
	} $(REDIR)

test: build/test-cpp.out build/fixed-puzzles.out build/searches.out build/evals.out build/uci.out build/magic.out

ci: build perft-test build/fixed-puzzles.out build/searches.out build/uci.out build/magic.out build/mate123.out build/mate45.out build/puzzles.out
	@echo "\n✅ CI checks passed\n"

# Generate compile_commands.json for clangd
$(COMPILE_COMMANDS):
	$(Q)mkdir -p build
	$(Q)echo '[' > $@
	$(Q)first=true; \
	for src in $(wildcard src/*.cpp src/*/*.cpp src/*/*/*.cpp); do \
		[ "$$first" = true ] && first=false || echo ',' >> $@; \
		echo '  {' >> $@; \
		echo "    \"directory\": \"$(PWD)\"," >> $@; \
		echo "    \"command\": \"$(CLANGPP) $(CCFLAGS) -Isrc -c $$src\"," >> $@; \
		echo "    \"file\": \"$$src\"" >> $@; \
		printf '  }' >> $@; \
	done
	$(Q)echo >> $@
	$(Q)echo ']' >> $@

.PHONY: ${COMPILE_COMMANDS} ci

SPRT_NEW ?= build/engine
SPRT_BASE ?= build/engine-prev
SPRT_STOCKFISH12 ?= stockfish-12
SPRT_ARGS ?= --tc 60+2

sprt-self: build/engine
	$(Q)./test/sprt.sh --new-cmd "$(SPRT_NEW)" --base-cmd "$(SPRT_BASE)" --new-name gbchess-new --base-name gbchess-base $(SPRT_ARGS)

sprt-sf12: build/engine
	$(Q)./test/sprt.sh --new-cmd "$(SPRT_NEW)" --base-cmd "$(SPRT_STOCKFISH12)" --new-name gbchess --base-name stockfish-12 $(SPRT_ARGS)

.PHONY: sprt-self sprt-sf12
