run_tests: t_hashring.o ../src/hashring.o ../dep/mh3/MurmurHash3.o
	gcc -g -pthread -Os $^ -lcheck -lm -lcrypto -o $@

t_hashring.o: t_hashring.c
	gcc -Wall -Wextra -pthread -std=gnu99 -Os -c $< \
		-g -Wno-unused-function -I../include -I../dep/mh3
