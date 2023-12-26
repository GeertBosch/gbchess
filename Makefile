all: test puzzles

%.h: common.h

%-test: %_test.cpp %.cpp %.h common.h
	clang++ -fsanitize=address -std=c++17 -g -O0 -o $@ $(filter-out %.h, $^)

clean:
	rm -f *.o *-debug *-test perft *.core puzzles.actual perf.data perf.data.old

moves-test: moves_test.cpp moves.cpp moves.h common.h fen.h fen.cpp

eval-test: eval_test.cpp eval.cpp fen.cpp moves.cpp *.h
	g++ -O2 -g -o $@ $(filter-out %.h,$^)
eval-debug: eval_test.cpp eval.cpp fen.cpp moves.cpp *.h
	clang++ -std=c++17 -O0 -g -o $@ $(filter-out %h,$^)

perft: perft.cpp eval.cpp moves.cpp fen.cpp *.h
	g++ -O2 -g -o $@ $(filter-out %.h,$^)

puzzles: eval-test puzzles.in puzzles.expected
	./eval-test 4 < puzzles.in > puzzles.actual
	@diff -uaB puzzles.expected puzzles.actual && echo "All puzzles solved correctly!"
	
test: fen-test moves-test eval-test perft
	./fen-test
	./moves-test
	./perft 5 4865609
	./eval-test "6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1" 5
