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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "hashring.h"

#include "t_bias.h"

#define hash_ring_init(r, h, n) hash_ring_init(r, h, NULL, n)

#define NBYTES (16*1024)

#define hash_ring_add(r, m) \
fail_if(hash_ring_add(r, m, 100/*weight*/, malloc(NBYTES), NBYTES))

#define hash_ring_remove(r, m) \
fail_if(hash_ring_remove(r, m, 0/*weight*/, malloc(NBYTES), NBYTES))

#define silence_hash_ring_dump 1
#define skip_noisy_tests 1

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

#define hasher md5_hasher

START_TEST(basic_init)
{
	struct hash_ring ring;

	hash_ring_init(&ring, hasher, 5);

	hash_ring_clean(&ring);
}
END_TEST

START_TEST(basic_additions)
{
	struct hash_ring ring;

	hash_ring_init(&ring, hasher, 5);

	hash_ring_add(&ring, 0xdeadbf);
	hash_ring_add(&ring, 0xc0ffee);

	fail_unless(ring.hr_ring_used == 10);
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(basic_removes)
{
	struct hash_ring ring;

	hash_ring_init(&ring, hasher, 128);

	hash_ring_add(&ring, 0x124dbe);
	hash_ring_add(&ring, 0x0ff426);
	hash_ring_add(&ring, 0x234962);

	fail_unless(ring.hr_ring_used == 3*128);

	hash_ring_remove(&ring, 0x234962);
	fail_unless(ring.hr_ring_used == 2*128);

	hash_ring_remove(&ring, 0x124dbe);
	fail_unless(ring.hr_ring_used == 128);

	hash_ring_remove(&ring, 0x0ff426);
	fail_unless(ring.hr_ring_used == 0);

	hash_ring_clean(&ring);
}
END_TEST

#define NELEM(a) ((sizeof(a))/(sizeof((a)[0])))
static const uint32_t characters[] = {
	0x50000123,
	0x40321000,
	0x3000ABC0,
	0x2DEF0000,
	0x10076800,
	0x77700000,
};

#if !skip_noisy_tests
START_TEST(example_plain)
{
	struct hash_ring ring;

	hash_ring_init(&ring, hasher, 128);

	hash_ring_add(&ring, 0xAAAA);
	hash_ring_add(&ring, 0xBBBB);
	hash_ring_add(&ring, 0xCCCC);

#if !silence_hash_ring_dump
	_hash_ring_dump(&ring);
#endif

	for (unsigned i = 0; i < NELEM(characters); i++) {
		uint32_t bin;
		int err;

		err = hash_ring_getn(&ring, characters[i], 1, &bin);
		fail_if(err != 0, "getn: %d:%s", err, strerror(err));

		printf("0x%08x => 0x%08x\n", characters[i], bin);
	}

	/*
	 * With MD5_hasher, output is:
	 * 0x50000123 => 0x0000cccc
	 * 0x40321000 => 0x0000aaaa
	 * 0x3000abc0 => 0x0000bbbb
	 * 0x2def0000 => 0x0000aaaa
	 * 0x10076800 => 0x0000cccc
	 * 0x77700000 => 0x0000bbbb
	 */
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(example_adds)
{
	struct hash_ring ring;

	hash_ring_init(&ring, hasher, 128);

	hash_ring_add(&ring, 0xAAAA);
	hash_ring_add(&ring, 0xBBBB);
	hash_ring_add(&ring, 0xCCCC);

	printf("initial:\n");
	for (unsigned i = 0; i < NELEM(characters); i++) {
		uint32_t bin;
		int err;

		err = hash_ring_getn(&ring, characters[i], 1, &bin);
		fail_if(err != 0, "getn: %d:%s", err, strerror(err));

		printf("0x%08x => 0x%08x\n", characters[i], bin);
	}

	hash_ring_add(&ring, 0xDDDD);
	hash_ring_add(&ring, 0xEEEE);

	printf("added new caches:\n");

	for (unsigned i = 0; i < NELEM(characters); i++) {
		uint32_t bin;
		int err;

		err = hash_ring_getn(&ring, characters[i], 1, &bin);
		fail_if(err != 0, "getn: %d:%s", err, strerror(err));

		printf("0x%08x => 0x%08x\n", characters[i], bin);
	}

	/*
	 * With MD5_hasher, initial output for [A, B, C] is:
	 * 0x50000123 => 0x0000cccc
	 * 0x40321000 => 0x0000aaaa
	 * 0x3000abc0 => 0x0000bbbb
	 * 0x2def0000 => 0x0000aaaa
	 * 0x10076800 => 0x0000cccc
	 * 0x77700000 => 0x0000bbbb
	 *
	 * After adding D, E:
	 * 0x50000123 => 0x0000cccc
	 * 0x40321000 => 0x0000aaaa
	 * 0x3000abc0 => 0x0000bbbb
	 * 0x2def0000 => 0x0000eeee
	 * 0x10076800 => 0x0000cccc
	 * 0x77700000 => 0x0000eeee
	 */
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(example_remove)
{
	struct hash_ring ring;

	hash_ring_init(&ring, hasher, 128);

	hash_ring_add(&ring, 0xAAAA);
	hash_ring_add(&ring, 0xBBBB);
	hash_ring_add(&ring, 0xCCCC);

	printf("initial:\n");
	for (unsigned i = 0; i < NELEM(characters); i++) {
		uint32_t bin;
		int err;

		err = hash_ring_getn(&ring, characters[i], 1, &bin);
		fail_if(err != 0, "getn: %d:%s", err, strerror(err));

		printf("0x%08x => 0x%08x\n", characters[i], bin);
	}

	hash_ring_remove(&ring, 0xCCCC);

	printf("removed 0xCCCC:\n");

	for (unsigned i = 0; i < NELEM(characters); i++) {
		uint32_t bin;
		int err;

		err = hash_ring_getn(&ring, characters[i], 1, &bin);
		fail_if(err != 0, "getn: %d:%s", err, strerror(err));

		printf("0x%08x => 0x%08x\n", characters[i], bin);
	}

	/*
	 * With MD5_hasher, initial output for [A, B, C] is:
	 * 0x50000123 => 0x0000cccc
	 * 0x40321000 => 0x0000aaaa
	 * 0x3000abc0 => 0x0000bbbb
	 * 0x2def0000 => 0x0000aaaa
	 * 0x10076800 => 0x0000cccc
	 * 0x77700000 => 0x0000bbbb
	 *
	 * After removing C:
	 * 0x50000123 => 0x0000bbbb
	 * 0x40321000 => 0x0000aaaa
	 * 0x3000abc0 => 0x0000bbbb
	 * 0x2def0000 => 0x0000aaaa
	 * 0x10076800 => 0x0000aaaa
	 * 0x77700000 => 0x0000bbbb
	 */
	hash_ring_clean(&ring);
}
END_TEST
#endif /* !skip_noisy_tests */

static void
ring_is_sorted(struct hash_ring *ring)
{
	uint32_t last = 0;

	for (size_t i = 0; i < ring->hr_ring_used; i++) {
		if (ring->hr_ring[i].kv_hash <= last && last != 0)
			fail("hashes aren't sorted");
		last = ring->hr_ring[i].kv_hash;
	}
}

START_TEST(func_add)
{
	struct hash_ring ring;

	hash_ring_init(&ring, hasher, 128);

	hash_ring_add(&ring, 0xABCDEF);

	fail_unless(ring.hr_ring_used == ring.hr_nreplicas);
	fail_if(ring.hr_ring == NULL);

	ring_is_sorted(&ring);

	hash_ring_add(&ring, 0x1C0FED);

	/* This can fail in unlikely event of a collision: */
	fail_unless(ring.hr_ring_used == 2*ring.hr_nreplicas);
	fail_if(ring.hr_ring == NULL);

	ring_is_sorted(&ring);
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(func_remove)
{
	struct hash_ring ring;

	hash_ring_init(&ring, hasher, 128);

	hash_ring_add(&ring, 0xABCDEF);
	hash_ring_remove(&ring, 0xABCDEF);

	fail_unless(ring.hr_ring_used == 0);
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(func_remove_nonexist)
{
	struct hash_ring ring;

	hash_ring_init(&ring, hasher, 128);

	hash_ring_add(&ring, 0xABCDEF);
	hash_ring_remove(&ring, 0xFEDCBA);

	fail_unless(ring.hr_ring_used == ring.hr_nreplicas);
	fail_if(ring.hr_ring == NULL);
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(func_getempty)
{
	struct hash_ring ring;
	int err;
	uint32_t bin;

	hash_ring_init(&ring, hasher, 128);

	err = hash_ring_getn(&ring, 0x1234, 1, &bin);
	fail_unless(err == ENOENT);
	hash_ring_clean(&ring);
}
END_TEST

static uint32_t _lotsa_inputs[512];
static void
fill_lotsa_inputs(void)
{
	FILE *f;

	f = fopen("/dev/urandom", "rb");
	for (unsigned i = 0; i < NELEM(_lotsa_inputs); i++) {
		size_t rd;

		rd = fread(&_lotsa_inputs[i], 4, 1, f);
		if (rd != 1)
			abort();
	}

	fclose(f);
}

#define MANY_INPUTS \
for (uint32_t *_INP = _lotsa_inputs; _INP != &_lotsa_inputs[NELEM(_lotsa_inputs)]; _INP++)

#define AN_INPUT (*_INP)

START_TEST(func_getsingle)
{
	struct hash_ring ring;
	const uint32_t the_bin = 0xABCDEF;

	hash_ring_init(&ring, hasher, 128);

	hash_ring_add(&ring, the_bin);

	MANY_INPUTS {
		int err;
		uint32_t bin;

		err = hash_ring_getn(&ring, AN_INPUT, 1, &bin);
		fail_if(err != 0, "0x%08x got err: %d:%s", AN_INPUT, err,
		    strerror(err));

		fail_if(bin != the_bin, "0x%08x got invalid bin: 0x%08x",
		    AN_INPUT, bin);
	}
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(func_get_multiple)
{
	struct hash_ring ring;
	const uint32_t bin1 = 0xABCDEF,
	      bin2 = 0xDC0FEE,
	      bin3 = 0x80F000;
	uint32_t bin;
	int err;

	/*
	 * These tests are specific to our particular md5 hash
	 * implementation... may fail otherwise, probably safe to ignore.
	 */
	hash_ring_init(&ring, md5_hasher, 512);

	hash_ring_add(&ring, bin1);
	hash_ring_add(&ring, bin2);
	hash_ring_add(&ring, bin3);

#if !silence_hash_ring_dump
	_hash_ring_dump(&ring);
#endif

	err = hash_ring_getn(&ring, 0xbcfdda1d, 1, &bin);
	fail_if(err);
	fail_if(bin != bin2, "#1 got bin: %08x", bin);

	err = hash_ring_getn(&ring, 0x5, 1, &bin);
	fail_if(err);
	fail_if(bin != bin1, "#2 got bin: %08x", bin);

	err = hash_ring_getn(&ring, 0xefffffff, 1, &bin);
	fail_if(err);
	fail_if(bin != bin3, "#3 got bin: %08x", bin);
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(func_get_multiple_quick)
{
	struct hash_ring ring;
	const uint32_t bin1 = 0xABCDEF,
	      bin2 = 0xDC0FEE,
	      bin3 = 0x80F000;
	uint32_t bin;
	int err;

	hash_ring_init(&ring, hasher, 128);

	hash_ring_add(&ring, bin1);
	hash_ring_add(&ring, bin2);
	hash_ring_add(&ring, bin3);

	MANY_INPUTS {
		err = hash_ring_getn(&ring, AN_INPUT, 1, &bin);
		fail_if(err);
		fail_unless(bin == bin1 || bin == bin2 || bin == bin3);
	}
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(func_get_multiple_remove)
{
	struct hash_ring ring;
	const uint32_t bin1 = 0xABCDEF,
	      bin2 = 0xDC0FEE,
	      bin3 = 0x80F000;
	uint32_t bin;
	int err;

	/*
	 * These tests are specific to our particular md5 hash
	 * implementation... may fail otherwise, probably safe to ignore.
	 */
	hash_ring_init(&ring, md5_hasher, 512);

	hash_ring_add(&ring, bin1);
	hash_ring_add(&ring, bin2);
	hash_ring_add(&ring, bin3);

	err = hash_ring_getn(&ring, 0xbcfdda1d, 1, &bin);
	fail_if(err);
	fail_if(bin != bin2, "#1 got bin: %08x", bin);

	err = hash_ring_getn(&ring, 0x5, 1, &bin);
	fail_if(err);
	fail_if(bin != bin1, "#2 got bin: %08x", bin);

	err = hash_ring_getn(&ring, 0x12f9578, 1, &bin);
	fail_if(err);
	fail_if(bin != bin1, "#3 got bin: %08x", bin);

	/* remove one */
	hash_ring_remove(&ring, bin1);

	err = hash_ring_getn(&ring, 0xbcfdda1d, 1, &bin);
	fail_if(err);
	fail_if(bin != bin2, "#4 got bin: %08x", bin);

	/* no longer hashes to removed bin */
	err = hash_ring_getn(&ring, 0x5, 1, &bin);
	fail_if(err);
	fail_if(bin != bin2, "#5 got bin: %08x", bin);

	err = hash_ring_getn(&ring, 0x12f9578, 1, &bin);
	fail_if(err);
	fail_if(bin != bin2, "#6 got bin: %08x", bin);
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(func_get_multiple_remove_quick)
{
	struct hash_ring ring;
	const uint32_t bin1 = 0xABCDEF,
	      bin2 = 0xDC0FEE,
	      bin3 = 0x80F000;

	hash_ring_init(&ring, hasher, 128);

	hash_ring_add(&ring, bin1);
	hash_ring_add(&ring, bin2);
	hash_ring_add(&ring, bin3);

	hash_ring_remove(&ring, bin3);

	MANY_INPUTS {
		int err;
		uint32_t bin;

		err = hash_ring_getn(&ring, AN_INPUT, 1, &bin);
		fail_if(err);

		fail_unless(bin == bin1 || bin == bin2);
	}
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(func_get_two)
{
	struct hash_ring ring;
	const uint32_t bin1 = 0xABCDEF,
	      bin2 = 0xDC0FEE,
	      bin3 = 0x80F000;

	hash_ring_init(&ring, md5_hasher, 128);

	hash_ring_add(&ring, bin1);
	hash_ring_add(&ring, bin2);
	hash_ring_add(&ring, bin3);

	uint32_t bins[2];
	int err;

	err = hash_ring_getn(&ring, 0xf0234adf, 2, bins);
	fail_if(err);
	fail_if(bins[0] == bins[1]);
	/* only going to work for a particular hash function... */
	fail_if(bins[0] != bin2, "1 0x%08x", bins[0]);
	fail_if(bins[1] != bin1, "2 0x%08x", bins[1]);
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(func_get_two_quick)
{
	struct hash_ring ring;
	const uint32_t bin1 = 0xABCDEF,
	      bin2 = 0xDC0FEE,
	      bin3 = 0x80F000;
	uint32_t bins[2];
	int err;

	hash_ring_init(&ring, hasher, 128);

	hash_ring_add(&ring, bin1);
	hash_ring_add(&ring, bin2);
	hash_ring_add(&ring, bin3);

	MANY_INPUTS {
		err = hash_ring_getn(&ring, AN_INPUT, 2, bins);
		fail_if(err);
		fail_if(bins[0] == bins[1]);
		fail_unless(bins[0] == bin1 || bins[0] == bin2 || bins[0] == bin3);
		fail_unless(bins[1] == bin1 || bins[1] == bin2 || bins[1] == bin3);
	}
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(func_get_two_quick2)
{
	struct hash_ring ring;
	const uint32_t bin1 = 0xABCDEF,
	      bin2 = 0xDC0FEE;
	uint32_t bins[2];
	int err;

	hash_ring_init(&ring, hasher, 128);

	hash_ring_add(&ring, bin1);
	hash_ring_add(&ring, bin2);

	MANY_INPUTS {
		err = hash_ring_getn(&ring, AN_INPUT, 2, bins);
		fail_if(err);
		fail_if(bins[0] == bins[1]);
		fail_unless(bins[0] == bin1 || bins[0] == bin2);
		fail_unless(bins[1] == bin1 || bins[1] == bin2);
	}
	hash_ring_clean(&ring);
}
END_TEST

START_TEST(err_get_two_with_one_in_ring)
{
	struct hash_ring ring;
	int err;
	uint32_t bin;

	hash_ring_init(&ring, hasher, 128);
	hash_ring_add(&ring, 0xABCDEF);

	err = hash_ring_getn(&ring, 0x0, 2, &bin);
	fail_unless(err == ENOENT);
	hash_ring_clean(&ring);
}
END_TEST

#undef hasher

const struct hash_compare comparison_functions[] = {
#if 0
	{ "DJB", djb_hasher, true },
	{ "MD5", md5_hasher, false },
	{ "SHA1", sha1_hasher, false },
	{ "MH3_32", mmh3_32_hasher, true },
#endif
	{ "MH3_128", mmh3_128_hasher, true },
#if 0
	{ "isi32", isi_hasher32, true },
#endif
	{ "isi64", isi_hasher64, true },
#if 0
	{ "crc32", crc32er, true },
#endif
	{ "crc32c", crc32cer, true },
	{ "siphash", siphasher, true },
	{ 0 },
};

const uint32_t comparison_replicas[] = {
#if 0
	4,
	8,
	16,
	32,
#endif
	64,
	128,
	256,
#if 0
	512,
#endif
};

#define NBUCKETS 3

static void
test_rmse(struct hash_ring *ring, double *distr, uint32_t buckets[NBUCKETS])
{
	uint64_t total_keyspace = (uint64_t)UINT32_MAX + 1,
		 total_ring_items = ring->hr_ring_used;

	int64_t exptd_item_keyspace = total_keyspace / total_ring_items;

	uint64_t last = -(total_keyspace - ring->hr_ring[ring->hr_ring_used-1].kv_hash);

	double dMSE = 0.;

	for (size_t i = 0; i < ring->hr_ring_used; i++) {
		uint64_t cur = ring->hr_ring[i].kv_hash,
			 dist = cur - last;
		int64_t err = exptd_item_keyspace - (int64_t)dist;

		double derr = err * err;
		derr /= total_keyspace;
		derr /= total_keyspace;
		dMSE += derr;

		last = cur;

		for (unsigned b = 0; b < NBUCKETS; b++)
			if (buckets[b] == ring->hr_ring[i].kv_value)
				distr[b] += (double)dist;
	}

	for (unsigned b = 0; b < NBUCKETS; b++)
		distr[b] /= (double)total_keyspace;

	double RdMSE = sqrt(dMSE),
	       lRdMSE = log(RdMSE)/log(2.);

	printf("%.01e\t(log: %.01f)\t", RdMSE, lRdMSE);
	/*
	printf("\t%d =\t%.01e\t(log: %.01f)\n", ring->hr_nreplicas, RdMSE,
	    lRdMSE);
	    */
}

START_TEST(distribution)
{
	uint32_t bins[3];
	FILE *f;
	struct hash_ring hr;
	double *dist;

	f = fopen("/dev/urandom", "rb");
	for (unsigned i = 0; i < NELEM(bins); i++) {
		size_t rd;

		rd = fread(&bins[i], 4, 1, f);
		fail_unless(rd == 1);

		bins[i] &= (1<<24) - 1;
	}
	fclose(f);

	dist = malloc(sizeof(double) * NELEM(bins) * NELEM(comparison_functions) * NELEM(comparison_replicas));

	printf("Region size error; lower is better.\n");
	printf("# replicas:\t");
	for (unsigned j = 0; j < NELEM(comparison_replicas); j++)
		printf("%"PRIu32"\t\t\t", comparison_replicas[j]);
	printf("\n");
	for (unsigned i = 0; comparison_functions[i].name != NULL; i++) {
		const char *hash_name;
		hr_hasher_t hash_fn;

		hash_name = comparison_functions[i].name;
		hash_fn = comparison_functions[i].hash;

		printf("%s\t\t", hash_name);

		for (unsigned j = 0; j < NELEM(comparison_replicas); j++) {
			uint32_t replicas;

			replicas = comparison_replicas[j];

			hash_ring_init(&hr, hash_fn, replicas);

			hash_ring_add(&hr, bins[0]);
			hash_ring_add(&hr, bins[1]);
			hash_ring_add(&hr, bins[2]);

			for (unsigned b = 0; b < NBUCKETS; b++)
				dist[i * NELEM(bins) * NELEM(comparison_replicas) + j * NELEM(bins) + b] = 0.;

			test_rmse(&hr, dist + i * NELEM(bins) * NELEM(comparison_replicas) + j * NELEM(bins), bins);

			hash_ring_clean(&hr);
		}

		printf("\n");
	}

	printf("\n");
	printf("Distribution error (lower is better):\n");
	for (unsigned i = 0; comparison_functions[i].name != NULL; i++) {
		const char *hash_name;

		hash_name = comparison_functions[i].name;

		const double exptd = 1. / ((double)NBUCKETS);

		printf("%s\t\t", hash_name);

		for (unsigned j = 0; j < NELEM(comparison_replicas); j++) {
			//uint32_t replicas;

			//replicas = comparison_replicas[j];

			double err[NBUCKETS];
			for (unsigned b = 0; b < NBUCKETS; b++)
				err[b] = dist[i*NBUCKETS*NELEM(comparison_replicas) + j*NBUCKETS + b] - exptd;

			double Rmse = sqrt(err[0]*err[0] + err[1]*err[1] + err[2]*err[2]);
			printf("%.02f\t\t\t", Rmse);
		}

		printf("\n");
	}

	free(dist);
}
END_TEST

START_TEST(err_idempotent)
{
	struct hash_ring ring;

	hash_ring_init(&ring, isi_hasher64, 64);

	fail_unless(ring.hr_ring_used == 0);
	hash_ring_add(&ring, 0x123456);
	fail_unless(ring.hr_ring_used == 64);

	/* adding the same item? don't count it */
	hash_ring_add(&ring, 0x123456);
	fail_unless(ring.hr_ring_used == 64);

	hash_ring_remove(&ring, 0x123456);
	fail_unless(ring.hr_ring_used == 0);

	/* removing the same item also works. */
	hash_ring_remove(&ring, 0x123456);
	fail_unless(ring.hr_ring_used == 0);

	hash_ring_clean(&ring);
}
END_TEST

/* Easy to collide, for test err_collisions_add below. */
static uint32_t
stupid_hash(const void *d, size_t len)
{
	const uint8_t *d8 = d;
	uint32_t res = 0;

	for (size_t i = 0; i < len; i++)
		res += d8[i];

	return res;
}

START_TEST(err_collisions_add)
{
	struct hash_ring ring;

	hash_ring_init(&ring, stupid_hash, 64);

	fail_unless(ring.hr_ring_used == 0);
	hash_ring_add(&ring, 1);
	fail_unless(ring.hr_ring_used == 64);

	/*
	 * 33-0 collides with 1-32, 33-1 with 1-33, etc, through 33-31 and 1-63
	 */
	hash_ring_add(&ring, 33);
	fail_if(ring.hr_ring_used != 64+32);

	hash_ring_clean(&ring);
}
END_TEST

START_TEST(err_collisions_remove)
{
	struct hash_ring ring;

	hash_ring_init(&ring, stupid_hash, 64);

	fail_unless(ring.hr_ring_used == 0);
	hash_ring_add(&ring, 1);
	fail_unless(ring.hr_ring_used == 64);

	hash_ring_add(&ring, 33);
	fail_if(ring.hr_ring_used != 64+32);

	hash_ring_remove(&ring, 1);
	fail_if(ring.hr_ring_used != 64);

	hash_ring_clean(&ring);
}
END_TEST

int
main(void)
{
	int nr_failed = 0;

	fill_lotsa_inputs();

	Suite *s = suite_create("all");

	TCase *t = tcase_create("basic");
	tcase_add_test(t, basic_init);
	tcase_add_test(t, basic_additions);
	tcase_add_test(t, basic_removes);
	suite_add_tcase(s, t);

#if !skip_noisy_tests
	t = tcase_create("useless_printfs");
	tcase_add_test(t, example_plain);
	tcase_add_test(t, example_adds);
	tcase_add_test(t, example_remove);
	suite_add_tcase(s, t);
#endif

	t = tcase_create("functional_tests");
	tcase_add_test(t, func_add);
	tcase_add_test(t, func_remove);
	tcase_add_test(t, func_remove_nonexist);
	tcase_add_test(t, func_getempty);
	tcase_add_test(t, func_getsingle);
	tcase_add_test(t, func_get_multiple);
	tcase_add_test(t, func_get_multiple_quick);
	tcase_add_test(t, func_get_multiple_remove);
	tcase_add_test(t, func_get_multiple_remove_quick);
	tcase_add_test(t, func_get_two);
	tcase_add_test(t, func_get_two_quick);
	tcase_add_test(t, func_get_two_quick2);
	suite_add_tcase(s, t);

	t = tcase_create("error_tests");
	tcase_add_test(t, err_get_two_with_one_in_ring);
	tcase_add_test(t, err_idempotent);
	tcase_add_test(t, err_collisions_add);
	tcase_add_test(t, err_collisions_remove);
	suite_add_tcase(s, t);

	t = tcase_create("keyspace_distribution");
	tcase_add_test(t, distribution);
	suite_add_tcase(s, t);

	suite_add_t_bias(s);
	suite_add_t_weights(s);

	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_VERBOSE);

	nr_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	if (nr_failed > 0)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
