all: test

%-test: %.cpp
	g++ -D$(@:-test=_TEST) -o $@ $^

eval-test: eval.cpp moves.cpp

clean:
	rm -f *.o *-test *.core
	
test: fen-test moves-test
	$(foreach t,$^,echo $(t) ; ./$(t) ; )
