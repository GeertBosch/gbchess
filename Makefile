all: test perft-test puzzles


%.h: common.h

%-test: %_test.cpp %.cpp %.h common.h
	clang++ -fsanitize=address -std=c++17 -DDEBUG -g -O0 -o $@ $(filter-out %.h, $^)

.PHONY: perft-sse2 clean

clean:
	rm -f *.o *-debug *-test perft core *.core puzzles.actual perf.data perf.data.old

moves-test: moves_test.cpp moves.cpp moves.h common.h fen.h fen.cpp

eval-test: eval_test.cpp eval.cpp hash.cpp fen.cpp moves.cpp *.h
	g++ -std=c++17 -O2 -g -o $@ $(filter-out %.h,$^)
eval-debug: eval_test.cpp eval.cpp hash.cpp fen.cpp moves.cpp *.h
	clang++ -std=c++17 -DDEBUG -O0 -g -o $@ $(filter-out %h,$^)

# perft counst total leaf nodes in the search tree for a position, see the perft-test target
perft: perft.cpp eval.cpp hash.cpp moves.cpp fen.cpp *.h
	clang++ -O3 -std=c++17 -g -o $@ $(filter-out %.h,$^)

# Compare the perft tool with some different compilation options for speed comparison
perft-sse2: perft.cpp eval.cpp hash.cpp moves.cpp fen.cpp *.h
	clang++ -O3 -std=c++17 -g -o $@ $(filter-out %.h,$^) && ./$@ 5
	clang++ -O3 -DSSE2EMUL -std=c++17 -g -o $@ $(filter-out %.h,$^) && ./$@  5
	g++ -O3 -std=c++17 -g -o $@ $(filter-out %.h,$^) && ./$@  5
	g++ -O3 -DSSE2EMUL -std=c++17 -g -o $@ $(filter-out %.h,$^) && ./$@  5

# Solve some known puzzles, for correctness of the search methods
puzzles: eval-test puzzles.in puzzles.expected
	./eval-test 10 < puzzles.in > puzzles.actual
	@diff -uaB puzzles.expected puzzles.actual && echo "All puzzles solved correctly!"
	
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
perft-test: perft
	./perft 5 4865609
	./perft "rnbqkbnr/1ppppppp/B7/p7/4P3/8/PPPP1PPP/RNBQK1NR b KQkq - 1 2" 4 509448
	./perft ${kiwipete} 4 4085603
	./perft ${position3} 5 674624
	./perft ${position4} 4 422333
	./perft ${position4m} 4 422333
	./perft ${position5} 4 2103487
	./perft ${position6} 4 3894594

test: fen-test moves-test eval-test eval-debug perft
	./fen-test
	./moves-test
	./eval-test "6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1" 5
