src/hashring.o: src/hashring.c include/hashring.h
	make -C src hashring.o

test/t_hashring.o: test/t_hashring.c test/t_bias.h include/hashring.h
	make -C test t_hashring.o

test/t_bias.o: test/t_bias.c include/hashring.h
	make -C test t_bias.o

test/isi_hash.o: test/isi_hash.c test/isi_hash.h
	make -C test isi_hash.o

dep/mh3/MurmurHash3.o: dep/mh3/MurmurHash3.cpp
	make -C dep mh3/MurmurHash3.o

test/run_tests: test/t_bias.o test/t_hashring.o dep/mh3/MurmurHash3.o src/hashring.o test/isi_hash.o
	make -C test run_tests

check: test/run_tests
	test/run_tests
