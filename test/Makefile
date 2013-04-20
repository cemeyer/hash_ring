run_tests: t_hashring.o isi_hash.o ../src/hashring.o ../dep/mh3/MurmurHash3.o
	$(CC) -g -pthread -Os $^ -lcheck -lm -lcrypto -lz -o $@

t_hashring.o: t_hashring.c ../include/hashring.h
	$(CC) -Wall -Wextra -pthread -std=gnu99 -Os -c $< \
		-g -Wno-unused-function -I../include -I../dep/mh3

isi_hash.o: isi_hash.c isi_hash.h
	$(CC) -Wall -Wextra -std=gnu99 -Os -g -c $<
