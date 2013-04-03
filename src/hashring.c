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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashring.h"

struct hr_kv_pair {
	uint32_t	 kv_hash;
	uint32_t	 kv_value;
};

/* Helpers ... */
static int	 hr_kv_cmp(const void *a, const void *b);
static void	 add_ring_item(struct hash_ring *, uint32_t hash,
			       uint32_t member);
static void	 remove_ring_item(struct hash_ring *, uint32_t hash,
				  uint32_t member);
static void	*bsearch_or_next(const void *key, const void *base,
				 size_t nmemb, size_t size,
				 int (*cmp)(const void *, const void *));

/*
 * =========================================
 * API Implementations
 * =========================================
 */

void
hash_ring_init(struct hash_ring *h, hr_hasher_t hash, uint32_t nreplicas)
{

	sx_init(&h->hr_lock, "hash ring lock");
	h->hr_hash_fn = hash;
	h->hr_nreplicas = nreplicas;
	h->hr_count = 0;

	h->hr_ring = NULL;
	h->hr_ring_used = 0;
	h->hr_ring_capacity = 0;
}

void
hash_ring_clean(struct hash_ring *h)
{
	sx_destroy(&h->hr_lock);
	free(h->hr_ring);
	memset(h, 0, sizeof *h);
}

void
hash_ring_add(struct hash_ring *h, uint32_t member)
{
	uint8_t hashdata[8];

	sx_xlock(&h->hr_lock);

	if (h->hr_ring_capacity - h->hr_ring_used < h->hr_nreplicas) {
		size_t newsize;

		h->hr_ring_capacity += h->hr_nreplicas;
		newsize = h->hr_ring_capacity * sizeof(struct hr_kv_pair);

		h->hr_ring = realloc(h->hr_ring, newsize);
		assert(h->hr_ring != NULL);
	}

	le32enc(hashdata, member);
	for (uint32_t i = 0; i < h->hr_nreplicas; i++) {
		uint32_t rhash;

		le32enc(&hashdata[4], i);
		rhash = h->hr_hash_fn(hashdata, sizeof hashdata);

#ifdef HASH_RING_DEBUG
		printf("insert rhash: %#x -> %#x\n", rhash, member);
#endif

		add_ring_item(h, rhash, member);
	}

	h->hr_count++;

	sx_unlock(&h->hr_lock);
}

void
hash_ring_remove(struct hash_ring *h, uint32_t member)
{
	uint8_t hashdata[8];

	sx_xlock(&h->hr_lock);

	assert(h->hr_count > 0);

	le32enc(hashdata, member);
	for (uint32_t i = 0; i < h->hr_nreplicas; i++) {
		uint32_t rhash;

		le32enc(&hashdata[4], i);
		rhash = h->hr_hash_fn(hashdata, sizeof hashdata);

#ifdef HASH_RING_DEBUG
		printf("remove rhash: %#x -> %#x\n", rhash, member);
#endif

		remove_ring_item(h, rhash, member);
	}

	/* TODO: possibly shrink h->hr_ring at this point if underfull */

	h->hr_count--;

	sx_unlock(&h->hr_lock);
}

int
hash_ring_getn(struct hash_ring *h, uint32_t hash, unsigned n,
    uint32_t *memb_out)
{
	int error = EINVAL;
	uint32_t i, found;
	struct hr_kv_pair *bucket, pairkey;

	if (n == 0)
		goto out;

	sx_slock(&h->hr_lock);

	if (h->hr_count < n) {
		error = ENOENT;
		sx_unlock(&h->hr_lock);
		goto out;
	}

	error = 0;

	/*
	 * Find the smallest 'i' for which ring[i]->kv_hash > hash.
	 */
	pairkey.kv_hash = hash;

	bucket = bsearch_or_next(&pairkey, h->hr_ring, h->hr_ring_used,
	    sizeof pairkey, hr_kv_cmp);
	i = bucket - h->hr_ring;

	if (i == h->hr_ring_used)
		i = 0;

	/*
	 * We start with hr_ring[i], walking until we find enough distinct
	 * items
	 */
	for (found = 0; n > found; i = (i + 1) % h->hr_ring_used) {
		bool already_found = false;

		for (unsigned j = 0; j < found; j++) {
			if (memb_out[j] == h->hr_ring[i].kv_value) {
				already_found = true;
				break;
			}
		}

		if (already_found)
			continue;

		memb_out[found] = h->hr_ring[i].kv_value;
		found++;
	}

	sx_unlock(&h->hr_lock);

out:
	return error;
}


/*
 * =========================================
 * Helper functions
 * =========================================
 */

/*
 * Bsearch, except returns a pointer to where the item would be inserted if
 * missing, instead of NULL.
 *
 * Borrowed from FreeBSD libc/stdlib/bsearch.c (BSD licensed).
 */
static void *
bsearch_or_next(const void *key, const void *base0, size_t nmemb, size_t size,
    int (*compar)(const void *, const void *))
{
	const char *base = base0;
	size_t lim;
	int cmp;
	const void *p;

	for (lim = nmemb; lim != 0; lim >>= 1) {
		p = base + (lim >> 1) * size;
		cmp = (*compar)(key, p);
		if (cmp == 0)
			return (void *)p;
		if (cmp > 0) {	/* key > p: move right */
			base = (char *)p + size;
			lim--;
		}		/* else move left */
	}
	return (void*)base;
}

/*
 * Compares two hr_kv_pairs.
 */
static int
hr_kv_cmp(const void *a, const void *b)
{
	const struct hr_kv_pair *pa = a, *pb = b;

	if (pa->kv_hash > pb->kv_hash)
		return 1;
	else if (pa->kv_hash < pb->kv_hash)
		return -1;
	return 0;
}

/*
 * Insert a new mapping into the ordered map internal to this hash_ring.
 */
static void
add_ring_item(struct hash_ring *h, uint32_t hash, uint32_t member)
{
	struct hr_kv_pair *insert, *end, newpair;

	sx_assert(&h->hr_lock, SX_XLOCKED);

	assert(h->hr_ring_capacity - h->hr_ring_used >= 1);
	
	newpair.kv_hash = hash;
	newpair.kv_value = member;

	/* Find the point at which this entry should be inserted */
	insert = bsearch_or_next(&newpair, h->hr_ring, h->hr_ring_used,
	    sizeof newpair, hr_kv_cmp);

	end = h->hr_ring + h->hr_ring_used;

	if (insert != end && insert->kv_hash == hash) {
#ifdef HASH_RING_DEBUG
		printf("collision on %#x, skipping\n", hash);
#endif
		return;
	}

	/* We insert in *front* of 'insert' */
	if (insert != end)
		memmove(insert + 1, insert, (end - insert) * sizeof newpair);

	assert(insert == h->hr_ring || (insert-1)->kv_hash < hash);
	assert(insert == end || hash < (insert+1)->kv_hash);

	*insert = newpair;
	h->hr_ring_used++;

#ifdef HASH_RING_DEBUG
	printf("inserted { %#x, %#x } at idx %lu\n", hash, member,
	    insert - h->hr_ring);
#endif
}

static void
remove_ring_item(struct hash_ring *h, uint32_t hash, uint32_t member)
{
	struct hr_kv_pair *remove, *end, rpair;

	sx_assert(&h->hr_lock, SX_XLOCKED);

	rpair.kv_hash = hash;
	rpair.kv_value = member;

	end = h->hr_ring + h->hr_ring_used;
	remove = bsearch(&rpair, h->hr_ring, h->hr_ring_used,
	    sizeof(struct hr_kv_pair), hr_kv_cmp);

	if (remove == NULL || remove->kv_value != member)
		return;

	if (remove + 1 != end)
		memmove(remove, remove + 1,
		    (end - remove - 1) * sizeof(struct hr_kv_pair));

	h->hr_ring_used--;
}
