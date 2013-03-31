hashring.o: hashring.c hashring.h
	gcc -g -Wall -Wextra -pthread -std=gnu99 -Os -c $<

check: t_hashring.c hashring.o
	gcc -Wall -Wextra -pthread -std=gnu99 -Os $^ -lcheck \
		-g \
		-Wno-unused-function \
		-lcrypto \
		-o run_tests
	./run_tests
