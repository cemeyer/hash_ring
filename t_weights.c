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
#include <limits.h>
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

#define hash_ring_remove(r, m, w) \
fail_if(hash_ring_remove(r, m, w, malloc(NBYTES), NBYTES))

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

#define silence_hash_ring_dump 1

#if !silence_hash_ring_dump
static void
_hash_ring_dump(struct hash_ring *h)
{

	printf("%p -> {\n", h);
	printf("\thash_fn: %p\n", h->hr_hash_fn);
	printf("\treplicas: %"PRIu32"\n", h->hr_nreplicas);
	printf("\tring:\n");
	for (size_t i = 0; i < h->hr_ring_used; i++)
		printf("\t\t{ hash: %#"PRIx32", bin: %#"PRIx32" }\n",
		    h->hr_ring[i].kv_hash, h->hr_ring[i].kv_value);
	printf("\tring capacity: %zu\n", h->hr_ring_capacity);
}
#endif

static unsigned
hash_ring_weight(struct hash_ring *h, uint32_t nreplicas, uint32_t member)
{
	const uint32_t MASK = (1<<24)-1;
	unsigned tot = 0;

	for (size_t i = 0; i < h->hr_ring_used; i++) {
		if ((h->hr_ring[i].kv_value & MASK) == member)
			tot++;
	}

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

	hash_ring_add(&ring, 0xdeadbf, 100);
	hash_ring_add(&ring, 0xc0ffee, 50);

#if !silence_hash_ring_dump
	_hash_ring_dump(&ring);
#endif

	fail_unless(ring.hr_ring_used == 3*nreps/2);

	wt = hash_ring_weight(&ring, nreps, 0xdeadbf);
	fail_unless(eps_equals(wt, 100, 1), "wt: %u", wt);

	wt = hash_ring_weight(&ring, nreps, 0xc0ffee);
	fail_unless(eps_equals(wt, 50, 1), "wt: %u", wt);

	hash_ring_clean(&ring);
}
END_TEST

START_TEST(wht_basic_remove)
{
	struct hash_ring ring;
	uint32_t nreps = 64;
	unsigned wt;

	hash_ring_init(&ring, isi_hasher64, nreps);

	hash_ring_add(&ring, 0xdeadbf, 100);
	hash_ring_add(&ring, 0xc0ffee, 50);

	fail_unless(ring.hr_ring_used == 3*nreps/2);

	wt = hash_ring_weight(&ring, nreps, 0xdeadbf);
	fail_unless(eps_equals(wt, 100, 1), "wt: %u", wt);

	wt = hash_ring_weight(&ring, nreps, 0xc0ffee);
	fail_unless(eps_equals(wt, 50, 1), "wt: %u", wt);

	hash_ring_remove(&ring, 0xdeadbf, 55);
	hash_ring_remove(&ring, 0xc0ffee, 23);

	wt = hash_ring_weight(&ring, nreps, 0xdeadbf);
	fail_unless(eps_equals(wt, 55, 3), "wt: %u", wt);
	wt = hash_ring_weight(&ring, nreps, 0xc0ffee);
	fail_unless(eps_equals(wt, 23, 3), "wt: %u", wt);

	hash_ring_add(&ring, 0xc0ffee, 100);

	wt = hash_ring_weight(&ring, nreps, 0xc0ffee);
	fail_unless(eps_equals(wt, 100, 1), "wt: %u", wt);

	hash_ring_clean(&ring);
}
END_TEST

START_TEST(wht_bigger)
{
	struct hash_ring ring;
	uint32_t nreps = 32;
	unsigned wt;

	hash_ring_init(&ring, isi_hasher64, nreps);

	hash_ring_add(&ring, 0xdeadbf, 100);
	hash_ring_add(&ring, 0xc0ffee, 50);
	hash_ring_add(&ring, 0xd5adb4, 21);
	hash_ring_add(&ring, 0x112200, 5);
	hash_ring_add(&ring, 0x712204, 75);

	fail_unless(eps_equals(ring.hr_ring_used, (100+50+21+5+75)*nreps/100,
		2));

	wt = hash_ring_weight(&ring, nreps, 0xdeadbf);
	fail_unless(eps_equals(wt, 100, 3), "wt: %u", wt);

	wt = hash_ring_weight(&ring, nreps, 0xc0ffee);
	fail_unless(eps_equals(wt, 50, 3), "wt: %u", wt);

	wt = hash_ring_weight(&ring, nreps, 0xd5adb4);
	fail_unless(eps_equals(wt, 21, 3), "wt: %u", wt);

	wt = hash_ring_weight(&ring, nreps, 0x112200);
	fail_unless(eps_equals(wt, 5, 3), "wt: %u", wt);

	wt = hash_ring_weight(&ring, nreps, 0x712204);
	fail_unless(eps_equals(wt, 75, 3), "wt: %u", wt);

	hash_ring_clean(&ring);
}
END_TEST

START_TEST(wht_bounds1)
{

	// Silence assert
	stderr = fopen("/dev/null", "w+b");
	fail_unless((intptr_t)stderr);

	hash_ring_add(NULL, 0xdeadbf, 101);
}
END_TEST

START_TEST(wht_bounds2)
{

	// Silence assert
	stderr = fopen("/dev/null", "w+b");
	fail_unless((intptr_t)stderr);

	hash_ring_add(NULL, 0xdeadbf, 0);
}
END_TEST

START_TEST(wht_bounds3)
{

	// Silence assert
	stderr = fopen("/dev/null", "w+b");
	fail_unless((intptr_t)stderr);

	hash_ring_remove(NULL, 0xdeadbf, 100);
}
END_TEST

START_TEST(wht_getn_terminates)
{
	struct hash_ring ring;
	uint32_t out[10];
	int rc;

	hash_ring_init(&ring, isi_hasher64, 16);

	hash_ring_add(&ring, 0xdeadbf, 100);
	hash_ring_add(&ring, 0xc0ffee, 50);

	rc = hash_ring_getn(&ring, 0x11000f, 10, out);
	fail_unless(rc == ENOENT);

	hash_ring_clean(&ring);

}
END_TEST

void
suite_add_t_weights(Suite *s)
{
	struct TCase *t;

	t = tcase_create("weighted_hashing");
	tcase_add_test(t, wht_basic);
	tcase_add_test(t, wht_basic_remove);
	tcase_add_test(t, wht_bigger);
	tcase_add_test_raise_signal(t, wht_bounds1, SIGABRT);
	tcase_add_test_raise_signal(t, wht_bounds2, SIGABRT);
	tcase_add_test_raise_signal(t, wht_bounds3, SIGABRT);
	tcase_add_test(t, wht_getn_terminates);
	suite_add_tcase(s, t);
}
