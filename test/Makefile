run_tests: t_hashring.o siphash24.o t_bias.o isi_hash.o ../src/hashring.o ../dep/mh3/MurmurHash3.o
	$(CC) -g -pthread -Os $^ -lcheck -lm -lcrypto -lz -o $@

t_hashring.o: t_hashring.c t_bias.h ../include/hashring.h
	$(CC) -Wall -Wextra -pthread -std=gnu99 -Os -c $< \
		-g -Wno-unused-function -I../include -I../dep/mh3

t_bias.o: t_bias.c siphash24.h t_bias.h ../include/hashring.h
	$(CC) -Wall -Wextra -pthread -std=gnu99 -Os -c $< \
		-g -Wno-unused-function -I../include -I../dep/mh3

siphash24.o: siphash24.c siphash24.h
	$(CC) -Wall -Wextra -pthread -std=gnu99 -Os -g -c $<

isi_hash.o: isi_hash.c isi_hash.h
	$(CC) -Wall -Wextra -std=gnu99 -Os -g -c $<
