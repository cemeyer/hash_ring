hashring.o: hashring.c hashring.h
	gcc -g -Wall -Wextra -pthread -std=gnu99 -Os -c $<

check: t_hashring.c hashring.o MurmurHash3.cpp
	gcc -Wall -Wextra -pthread -std=gnu99 -Os $^ -lcheck \
		-g \
		-Wno-unused-function \
		-lm -lcrypto \
		-o run_tests
	./run_tests
