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

bool _debug = false;


/* Helpers ... */
static int	_kv_cmp(const void *a, const void *b);
static void	_add_ring_item(struct hash_ring *, uint32_t hash,
		    uint32_t member);
static void	_remove_ring_item(struct hash_ring *, uint32_t hash,
		    uint32_t member);

/*
 * =========================================
 * API Implementations
 * =========================================
 */

void
hash_ring_init(struct hash_ring *h, hasher_t hash, uint32_t nreplicas)
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

	/*
	 * TODO a kernel implementation will likely allocate memory unlocked
	 * here:
	while (h->hr_ring_capacity - h->hr_ring_used < h->hr_nreplicas)
		realloc(h->hr_ring, ...);
	*/

	le32enc(hashdata, member);
	for (uint32_t i = 0; i < h->hr_nreplicas; i++) {
		uint32_t rhash;

		le32enc(&hashdata[4], i);
		rhash = h->hr_hash_fn(hashdata, sizeof hashdata);

		if (_debug)
			printf("insert rhash: %#x -> %#x\n", rhash, member);

		_add_ring_item(h, rhash, member);
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

		if (_debug)
			printf("remove rhash: %#x -> %#x\n", rhash, member);

		_remove_ring_item(h, rhash, member);
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
	 * TODO: this can also be bsearch
	 * */
	for (i = 0; i < h->hr_ring_used; i++) {
		if (h->hr_ring[i].kv_hash > hash)
			break;
	}

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
 * Compares two _kv_pairs.
 */
static int
_kv_cmp(const void *a, const void *b)
{
	const struct _kv_pair *pa = a, *pb = b;

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
_add_ring_item(struct hash_ring *h, uint32_t hash, uint32_t member)
{
	struct _kv_pair *insert, *end, newpair;

	sx_assert(&h->hr_lock, SX_XLOCKED);

	/*
	 * TODO a kernel implementation will likely allocate memory earlier.
	 */
	if (h->hr_ring_capacity - h->hr_ring_used < 1) {
		size_t newsize;

		h->hr_ring_capacity += h->hr_nreplicas;
		newsize = h->hr_ring_capacity * sizeof(struct _kv_pair);

		h->hr_ring = realloc(h->hr_ring, newsize);
		assert(h->hr_ring != NULL);
	}
	
	newpair.kv_hash = hash;
	newpair.kv_value = member;

	/*
	 * TODO: This could be a bsearch instead of a linear scan, but we need
	 * a bsearch implementation that returns the nearest match.
	 */
	end = h->hr_ring + h->hr_ring_used;
	for (insert = h->hr_ring; insert < end; insert++) {
		int c = _kv_cmp(insert, &newpair);

		if (c > 0)
			break;
		else if (c == 0) {
			if (_debug)
				printf("collision on %#x, skipping\n", hash);
			return;
		}
	}

	/* We insert in *front* of 'insert' */
	if (insert != end)
		memmove(insert + 1, insert,
		    (end - insert) * sizeof(struct _kv_pair));

	*insert = newpair;
	h->hr_ring_used++;

	if (_debug)
		printf("inserted { %#x, %#x } at idx %lu\n", hash, member,
		    insert - h->hr_ring);
}

static void
_remove_ring_item(struct hash_ring *h, uint32_t hash, uint32_t member)
{
	struct _kv_pair *remove, *end, rpair;

	sx_assert(&h->hr_lock, SX_XLOCKED);

	rpair.kv_hash = hash;
	rpair.kv_value = member;

	end = h->hr_ring + h->hr_ring_used;
	remove = bsearch(&rpair, h->hr_ring, h->hr_ring_used,
	    sizeof(struct _kv_pair), _kv_cmp);

	if (remove == NULL || remove->kv_value != member)
		return;

	if (remove + 1 != end)
		memmove(remove, remove + 1,
		    (end - remove - 1) * sizeof(struct _kv_pair));

	h->hr_ring_used--;
}
