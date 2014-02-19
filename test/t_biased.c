/*
 * Copyright (c) 2013 Conrad Meyer <cse.cem@gmail.com>
 *
 * This is available for use under the terms of the MIT license, see the
 * LICENSE file.
 *
 * This is derived from the MIT-licensed Go-language implementation,
 * 'consistent,' available here:
 *         https://github.com/stathat/consistent
 */

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "hashring.h"

#include "t_bias.h"

#define hash_ring_init(r, h, n) hash_ring_init(r, h, NULL, n)

#define NBYTES (16*1024)

#define hash_ring_add(r, m, w) \
fail_if(hash_ring_add(r, m, w, malloc(NBYTES), NBYTES))

#define hash_ring_remove(r, m) \
fail_if(hash_ring_remove(r, m, malloc(NBYTES), NBYTES))

#define min(x, y) ({ \
	typeof(x) _min1 = (x); \
	typeof(y) _min2 = (y); \
	(void) (&_min1 == &_min2); \
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({ \
	typeof(x) _max1 = (x); \
	typeof(y) _max2 = (y); \
	(void) (&_max1 == &_max2); \
	_max1 > _max2 ? _max1 : _max2; })

static void
_hash_ring_dump(struct hash_ring *h)
{

	printf("%p -> {\n", h);
	printf("\thash_fn: %p\n", h->hr_hash_fn);
	printf("\tcount: %"PRIu32"\n", h->hr_count);
	printf("\treplicas: %"PRIu32"\n", h->hr_nreplicas);
	printf("\tring:\n");
	for (size_t i = 0; i < h->hr_ring_used; i++)
		printf("\t\t{ hash: %#"PRIx32", bin: %#"PRIx32" }\n",
		    h->hr_ring[i].kv_hash, h->hr_ring[i].kv_value);
	printf("\tring capacity: %zu\n", h->hr_ring_capacity);
}

static unsigned
hash_ring_weight(struct hash_ring *h, uint32_t nreplicas, uint32_t member)
{
	unsigned tot = 0;

	for (size_t i = 0; i < h->hr_ring_used; i++)
		if (h->hr_ring[i].kv_value == member)
			tot++;

	return 100 * tot / nreplicas;
}

static bool
eps_equals(unsigned a, unsigned b, unsigned eps)
{
	unsigned lower, higher;

	lower = b - eps;
	if (lower > b)
		lower = 0;
	higher = b + eps;
	if (higher < b)
		higher = UINT_MAX;

	if (a >= lower && a <= higher)
		return true;
	return false;
}

START_TEST(wht_basic)
{
	struct hash_ring ring;
	uint32_t nreps = 16;
	unsigned wt;

	hash_ring_init(&ring, isi_hasher64, nreps);

	hash_ring_add(&ring, 0xdeadbeef, 100);
	hash_ring_add(&ring, 0xc0ffee, 50);

	fail_unless(ring.hr_count == 2);

	wt = hash_ring_weight(&ring, nreps, 0xdeadbeef);
	fail_unless(eps_equals(wt, 100, 1), "wt: %u", wt);

	wt = hash_ring_weight(&ring, nreps, 0xc0ffee);
	fail_unless(eps_equals(wt, 50, 1), "wt: %u", wt);

	hash_ring_clean(&ring);
}
END_TEST

START_TEST(wht_bigger)
{
	struct hash_ring ring;
	uint32_t nreps = 32;
	unsigned wt;

	hash_ring_init(&ring, isi_hasher64, nreps);

	hash_ring_add(&ring, 0xdeadbeef, 100);
	hash_ring_add(&ring, 0xc0ffee, 50);
	hash_ring_add(&ring, 0xdeadb4dd, 21);
	hash_ring_add(&ring, 0x11220033, 5);
	hash_ring_add(&ring, 0x71220433, 75);

	fail_unless(ring.hr_count == 5);

	wt = hash_ring_weight(&ring, nreps, 0xdeadbeef);
	fail_unless(eps_equals(wt, 100, 3), "wt: %u", wt);

	wt = hash_ring_weight(&ring, nreps, 0xc0ffee);
	fail_unless(eps_equals(wt, 50, 3), "wt: %u", wt);

	wt = hash_ring_weight(&ring, nreps, 0xdeadb4dd);
	fail_unless(eps_equals(wt, 21, 3), "wt: %u", wt);

	wt = hash_ring_weight(&ring, nreps, 0x11220033);
	fail_unless(eps_equals(wt, 5, 3), "wt: %u", wt);

	wt = hash_ring_weight(&ring, nreps, 0x71220433);
	fail_unless(eps_equals(wt, 75, 3), "wt: %u", wt);

	hash_ring_clean(&ring);
}
END_TEST

START_TEST(wht_bounds1)
{

	// Silence assert
	stderr = fopen("/dev/null", "w+b");
	fail_unless((intptr_t)stderr);

	hash_ring_add(NULL, 0xdeadbeef, 101);
}
END_TEST

START_TEST(wht_bounds2)
{

	// Silence assert
	stderr = fopen("/dev/null", "w+b");
	fail_unless((intptr_t)stderr);

	hash_ring_add(NULL, 0xdeadbeef, 0);
}
END_TEST

void
suite_add_t_biased(Suite *s)
{
	struct TCase *t;

	t = tcase_create("weighted_hashing");
	tcase_add_test(t, wht_basic);
	tcase_add_test(t, wht_bigger);
	tcase_add_test_raise_signal(t, wht_bounds1, SIGABRT);
	tcase_add_test_raise_signal(t, wht_bounds2, SIGABRT);
	suite_add_tcase(s, t);
}
