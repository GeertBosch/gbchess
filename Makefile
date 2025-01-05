PUZZLES=puzzles/lichess_db_puzzle.csv
CCFLAGS=-std=c++17
CLANGPP=clang++
GPP=g++
# DEBUGFLAGS=-fsanitize=address -DDEBUG -O0 -g --coverage
DEBUGFLAGS=-DDEBUG -O0 -g
# DEBUGFLAGS=-O2
# -fprofile-instr-generate -fcoverage-mapping

export LLVM_PROFILE_FILE=coverage-%m.profraw
LLVM-MERGE=llvm-profdata merge -sparse coverage-*.profraw -o coverage.profdata

# MacOS specific stuff - why can't thinks just work by default?
ifeq ($(_system_type),Darwin)
    export MallocNanoZone=0
    sdk=$(shell xcrun --sdk macosx --show-sdk-path)
    arch=$(shell uname -m)
    CLANGPP:=/usr/bin/clang++
    CCFLAGS:=${CCFLAGS} -isysroot ${sdk} -mmacosx-version-min=11.0 -target darwin17.0.0 -arch ${arch} -stdlib=libc++ -Wl,-syslibroot,${sdk} -mmacosx-version-min=11.0 -target darwin17.0.0 -arch ${arch}
endif

all: test perft-test mate123

%.h: common.h

%-test: %_test.cpp %.cpp %.h common.h
	${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $(filter-out %.h, $^)

.PHONY:
.PHONY.h: .PHONY

clean: .PHONY
	rm -f *.o *-debug *-test perft core *.core puzzles.actual perf.data* *.ii *.bc *.s
	rm -f *.profraw *.profdata *.gcda *.gcno lcov.info
	rm -rf *.dSYM

moves-test: moves_test.cpp moves.cpp moves.h common.h fen.h fen.cpp

elo-test: elo_test.cpp elo.h
	${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} ${LINKFLAGS} -o $@ $(filter-out %.h, $^)

EVAL_SRCS=eval.cpp hash.cpp fen.cpp moves.cpp

eval-test: eval_test.cpp ${EVAL_SRCS} *.h
	${GPP} ${CCFLAGS} -g -O2 -o $@ $(filter-out %.h,$^)
eval-debug: eval_test.cpp ${EVAL_SRCS} *.h
	${GPP} ${CCFLAGS} ${DEBUGFLAGS} -o $@ $(filter-out %.h,$^)

SEARCH_SRCS=${EVAL_SRCS} search.cpp single_runner.cpp

search-test: search_test.cpp ${SEARCH_SRCS} *.h
	${GPP} ${CCFLAGS} -g -O2 -o $@ $(filter-out %.h,$^)
search-debug: search_test.cpp ${SEARCH_SRCS} *.h
	${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} -o $@ $(filter-out %.h,$^)

uci-test: uci_test.cpp uci.cpp ${SEARCH_SRCS}


# perft counts the total leaf nodes in the search tree for a position, see the perft-test target
perft: perft.cpp ${EVAL_SRCS} *.h
	${CLANGPP} -O3 ${CCFLAGS} -g -o $@ $(filter-out %.h,$^)
perft-debug: perft.cpp ${EVAL_SRCS} *.h
	${CLANGPP} ${CCFLAGS} ${DEBUGFLAGS} -o $@ $(filter-out %.h,$^)

# Compare the perft tool with some different compilation options for speed comparison
perft-sse2: perft.cpp hash.cpp moves.cpp fen.cpp *.h .PHONY.h
	${CLANGPP} -O3 ${CCFLAGS} -g -o $@ $(filter-out %.h,$^) && ./$@ 5 4865609
	${CLANGPP} -O3 -DSSE2EMUL ${CCFLAGS} -g -o $@ $(filter-out %.h,$^) && ./$@  5 4865609
	${GPP} -O3 ${CCFLAGS} -g -o $@ $(filter-out %.h,$^) && ./$@  5 4865609
	${GPP} -O3 -DSSE2EMUL ${CCFLAGS} -g -o $@ $(filter-out %.h,$^) && ./$@  5 4865609

# Solve some known mate-in-n puzzles, for correctness of the search methods
mate123: search-test ${PUZZLES} .PHONY
	egrep "FEN,Moves|mateIn[123]" ${PUZZLES} | head -101 | ./search-test 5
# There are still some failures in these
mate45: search-test ${PUZZLES} .PHONY
	egrep "FEN,Moves|mateIn[45]" ${PUZZLES} | head -31 | ./search-test 9

puzzles: search-test ${PUZZLES} .PHONY
	egrep -v "mateIn[12345]" ${PUZZLES} | head -31| ./search-test 5

# Some line count statistics, requires the cloc tool, see https://github.com/AlDanial/cloc
cloc:
	@echo Excluding Tests
	cloc --by-percent cmb `find . -name \*.cpp -o -name \*.h | egrep -v '_test[.]|debug'`
	@echo Just Tests
	cloc --by-percent cmb `find . -name \*.cpp -o -name \*.h | egrep '_test[.]|debug'`

# The following are well known perft positions, see https://www.chessprogramming.org/Perft_Results
kiwipete="r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
position3="8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
position4="r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"
position4m="r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1"
position5="rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
position6="r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"

# Verify well-known perft results. Great for checking correct move generation.
perft-test: perft perft-debug perft-sse2
	./perft-debug 4 197281
	./perft 5 4865609
	./perft "rnbqkbnr/1ppppppp/B7/p7/4P3/8/PPPP1PPP/RNBQK1NR b KQkq - 1 2" 4 509448
	./perft ${kiwipete} 4 4085603
	./perft ${position3} 5 674624
	./perft ${position4} 4 422333
	./perft ${position4m} 4 422333
	./perft ${position5} 4 2103487
	./perft ${position6} 4 3894594

${PUZZLES}:
	mkdir -p $(dir ${PUZZLES}) && cd $(dir ${PUZZLES}) && wget https://database.lichess.org/$(notdir ${PUZZLES}).zst
	zstd -d ${PUZZLES}.zst

test: fen-test moves-test elo-test eval-test search-debug single_runner-test uci-test
	rm -f coverage-*.profraw
	./fen-test
	./moves-test
	./elo-test
	./eval-test "6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1"
	./single_runner-test
	./search-debug "6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1" 5

coverage: test
	${LLVM-MERGE}
	llvm-cov report ./fen-test -instr-profile=coverage.profdata
	llvm-cov report ./moves-test -instr-profile=coverage.profdata
	llvm-cov report ./elo-test -instr-profile=coverage.profdata
	llvm-cov report ./eval-test -instr-profile=coverage.profdata
	llvm-cov report ./single_runner-test -instr-profile=coverage.profdata
	llvm-cov report ./search-debug -instr-profile=coverage.profdata
