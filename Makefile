all: test

%-test: %_test.cpp %.cpp
	g++ -o $@ $^

clean:
	rm -f *.o *-test *.core

eval-test: eval_test.cpp eval.cpp fen.cpp moves.cpp
	g++ -o $@ $^

	
test: fen-test moves-test eval-test
	./fen-test
	./moves-test
	./eval-test "6k1/4Q3/5K2/8/8/8/8/8 w - - 0 1" 4
