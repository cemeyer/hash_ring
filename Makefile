src/hashring.o: src/hashring.c include/hashring.h
	make -C src hashring.o

test/t_hashring.o: test/t_hashring.c include/hashring.h
	make -C test t_hashring.o

dep/mh3/MurmurHash3.o: dep/mh3/MurmurHash3.cpp
	make -C dep mh3/MurmurHash3.o

test/run_tests: test/t_hashring.o dep/mh3/MurmurHash3.o src/hashring.o
	make -C test run_tests

check: test/run_tests
	test/run_tests
