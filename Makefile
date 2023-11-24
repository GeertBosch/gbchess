all: test puzzles

%-test: %_test.cpp %.cpp
	g++ -g -O0 -o $@ $^

clean:
	rm -f *.o *-debug *-test *.core puzzles.actual perf.data perf.data.old

eval-test: eval_test.cpp eval.cpp fen.cpp moves.cpp 
	g++ -O2 -g -o $@ $^
eval-debug: eval_test.cpp eval.cpp fen.cpp moves.cpp 
	clang++ -O0 -g -o $@ $^

puzzles: eval-test puzzles.in puzzles.expected
	./eval-test 4 < puzzles.in > puzzles.actual
	@diff -uaB puzzles.actual puzzles.expected && echo "All puzzles solved correctly!"
	
test: fen-test moves-test eval-test
	./fen-test
	./moves-test
	./eval-test "6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1" 5
