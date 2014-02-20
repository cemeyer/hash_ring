CFLAGS?=-g -pipe -Wall -Wextra -Werror -Os
CFLAGS+=-pthread -std=c99 -D_BSD_SOURCE
CXXFLAGS?=-g -pipe -Wall -Wextra -Werror -Os
CXXFLAGS+=-pthread -std=c++98

all: hashring.o run_tests

hashring.o: hashring.c hashring.h
	$(CC) $(CFLAGS) -c $<

T_DEPS = hashring.o MurmurHash3.o siphash24.o isi_hash.o
T_OBJS = t_bias.o t_biased.o t_hashring.o $(T_DEPS)
T_HDRS = t_bias.h siphash24.h hashring.h isi_hash.h MurmurHash3.h

run_tests: $(T_OBJS) $(T_HDRS)
	$(CC) $(CFLAGS) -o $@ $(T_OBJS) -lcheck -lm -lcrypto -lz

%.o: %.c t_bias.h hashring.h siphash24.h isi_hash.h
	$(CC) $(CFLAGS) -c $<

%.o: %.cpp MurmurHash3.h
	$(CXX) $(CXXFLAGS) -c $<

check: run_tests
	./run_tests

clean:
	rm -f $(T_OBJS)
