all: test puzzles

%-test: %_test.cpp %.cpp
	g++ -g -O0 -o $@ $^

clean:
	rm -f *.o *-test *.core puzzles.actual perf.data

eval-test: eval_test.cpp eval.cpp fen.cpp moves.cpp 
	g++ -O2 -o $@ $^

puzzles: eval-test puzzles.in puzzles.expected
	./eval-test 3 < puzzles.in > puzzles.actual
	@diff -aB puzzles.actual puzzles.expected && echo "All puzzles solved correctly!"
	
test: fen-test moves-test eval-test
	./fen-test
	./moves-test
	./eval-test "6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1" 4
