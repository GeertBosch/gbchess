all: test

%-test: %_test.cpp %.cpp
	g++ -o $@ $^

eval-test: eval.cpp moves.cpp

clean:
	rm -f *.o *-test *.core
	
test: fen-test moves-test
	$(foreach t,$^,echo $(t) ; ./$(t) ; )
