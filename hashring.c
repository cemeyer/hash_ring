/*
 * Copyright (c) 2013 Conrad Meyer <cse.cem@gmail.com>
 *
 * This is available for use under the terms of the MIT license, see the
 * LICENSE file.
 *
 * Implementation of a consistent hashing instance, derived from the
 * MIT-licensed Go-language implementation, 'consistent,' available here:
 *         https://github.com/stathat/consistent
 */

#ifdef _KERNEL
# ifndef __FreeBSD__
#  error Unsupported kernel
# endif
# include <sys/endian.h>
# include <sys/errno.h>
# include <sys/libkern.h>
# include <sys/malloc.h>
#else /* !_KERNEL */
# ifdef __FreeBSD__
#  include <sys/endian.h>
# endif

# include <assert.h>
# include <errno.h>
# include <stdint.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# ifndef __FreeBSD__
static inline void
le32enc(void *vp, uint32_t val)
{
	uint8_t *bp = vp;

	bp[0] = val & 0xff;
	bp[1] = (val >> 8) & 0xff;
	bp[2] = (val >> 16) & 0xff;
	bp[3] = val >> 24;
}
# endif /* !__FreeBSD__ */

# define __DECONST(type, var)	((type)(uintptr_t)(const void *)(var))
# define ASSERT(expr)		assert(expr)
# define ASSERT_DEBUG(expr)	assert(expr)
# define free(p, tag)		free(p)
struct malloc_type;
#endif /* _KERNEL */

#include "hashring.h"

/* Extract weight, value from combined field in hr_kv_pair */
#define HR_VAL_BITS		24
#define HR_VAL_MASK		((1U << HR_VAL_BITS) - 1)
#define HR_VAL(u32val)		((u32val) & HR_VAL_MASK)
#define HR_WEIGHT(u32val)	((u32val) >> HR_VAL_BITS)

/* Combine weight and 24-bit value into combined kv_value */
#define HR_MK_VAL(u32wt, u32member) \
	(((u32wt) << HR_VAL_BITS) | HR_VAL(u32member))

static void	*bsearch_or_next(const void *key, const void *base,
				 size_t nmemb, size_t size,
				 int (*cmp)(const void *, const void *));
static int	 hr_kv_cmp(const void *a, const void *b);

static void	 add_ring_item(struct hash_ring *, uint32_t hash,
			       uint32_t member);
static void	 remove_ring_item(struct hash_ring *, uint32_t hash,
				  uint32_t member);

static void	 rehash(struct hash_ring *, uint32_t *memb);
static void	 ring_fixup_weights(struct hash_ring*, uint32_t mempair);

/*
 * =========================================
 * API Implementations
 * =========================================
 */

void
hash_ring_init(struct hash_ring *h, hr_hasher_t hash, struct malloc_type *mt,
    uint32_t nreplicas)
{

	h->hr_hash_fn = hash;
	h->hr_mtype = mt;
	h->hr_nreplicas = nreplicas;

	h->hr_ring = NULL;
	h->hr_ring_used = 0;
	h->hr_ring_capacity = 0;

#ifdef INVARIANTS
	h->hr_initialized = true;
#endif
}

void
hash_ring_clean(struct hash_ring *h)
{

	if (h->hr_ring != NULL)
		free(h->hr_ring, h->hr_mtype);
	memset(h, 0, sizeof *h);

#ifdef INVARIANTS
	h->hr_initialized = false;
#endif
}

size_t
hash_ring_add(struct hash_ring *h, uint32_t member, unsigned weightpct,
    void *newmemb, size_t sz)
{
	uint8_t hashdata[8];
	uint32_t reps;
	size_t need;

#ifdef INVARIANTS
	ASSERT(h->hr_initialized);
#endif
	ASSERT(weightpct > 0 && weightpct <= 100);
	ASSERT(HR_WEIGHT(member) == 0);

	need = (h->hr_ring_used + h->hr_nreplicas) * sizeof(h->hr_ring[0]);

	if (need <= h->hr_ring_capacity) {
		if (newmemb != NULL)
			free(newmemb, h->hr_mtype);
	} else if (need <= sz) {
		if (h->hr_ring_used > 0) {
			memcpy(newmemb, h->hr_ring,
			    h->hr_ring_used*sizeof(h->hr_ring[0]));
		}
		if (h->hr_ring != NULL)
			free(h->hr_ring, h->hr_mtype);
		h->hr_ring = newmemb;
		h->hr_ring_capacity = sz / sizeof(h->hr_ring[0]);
	} else {
		if (newmemb != NULL)
			free(newmemb, h->hr_mtype);
		return need;
	}

	le32enc(hashdata, member);

	reps = weightpct * h->hr_nreplicas / 100;
	if (reps == 0)
		reps = 1;

	for (uint32_t i = 0; i < reps; i++) {
		uint32_t rhash;

		le32enc(&hashdata[4], i);
		rhash = h->hr_hash_fn(hashdata, sizeof hashdata);

		add_ring_item(h, rhash, member);
	}

	ring_fixup_weights(h, HR_MK_VAL(weightpct, member));

	return 0;
}

/*
 * Caller must preallocate member buffer in case we need to rehash.
 *
 * aux is always freed.
 */
size_t
hash_ring_remove(struct hash_ring *h, uint32_t member, unsigned weightpct,
    void *aux, size_t auxsz)
{
	uint8_t hashdata[8];
	size_t hr_used,
	       memb_exp;
	uint32_t reps, i;

#ifdef INVARIANTS
	ASSERT(h->hr_initialized);
#endif
	ASSERT(weightpct < 100);
	ASSERT(HR_WEIGHT(member) == 0);

	memb_exp = 0;
	if (h->hr_nreplicas > 0)
		memb_exp = (h->hr_ring_used + (h->hr_nreplicas - 1)) / h->hr_nreplicas;

	memb_exp *= sizeof(uint32_t);
	if (auxsz < memb_exp) {
		if (aux != NULL)
			free(aux, h->hr_mtype);
		return memb_exp;
	}

	hr_used = h->hr_ring_used;

	le32enc(hashdata, member);

	reps = weightpct * h->hr_nreplicas / 100;
	if (reps == 0 && weightpct > 0)
		reps = 1;

	for (i = h->hr_nreplicas - 1; i >= reps && i < UINT32_MAX; i--) {
		uint32_t rhash;

		le32enc(&hashdata[4], i);
		rhash = h->hr_hash_fn(hashdata, sizeof hashdata);

		remove_ring_item(h, rhash, member);
	}

	ring_fixup_weights(h, HR_MK_VAL(weightpct, member));

	/* TODO: possibly shrink h->hr_ring at this point if underfull */

	if (hr_used != h->hr_ring_used)
		rehash(h, aux);

	if (aux != NULL)
		free(aux, h->hr_mtype);

	return 0;
}

int
hash_ring_getn(const struct hash_ring *h, uint32_t hash, unsigned n,
    uint32_t *memb_out)
{
	int error = EINVAL;
	uint32_t i, found, walked;
	struct hr_kv_pair *bucket, pairkey;

#ifdef INVARIANTS
	ASSERT(h->hr_initialized);
#endif

	if (n == 0)
		goto out;

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
	walked = 0;
	for (found = 0; n > found; i = (i + 1) % h->hr_ring_used) {
		bool already_found = false;

		/*
		 * Since we no longer have a reliable hash member count, error
		 * out if we walk the whole ring and don't have enough members
		 * to satisfy the request.
		 *
		 * Since this may be expensive (O(N) instead of amortized
		 * O(1)), callers are advised to only call getn() with N <= the
		 * number of members they have inserted. This is often easy for
		 * the user to track.
		 */
		if (walked >= h->hr_ring_used) {
			error = ENOENT;
			goto out;
		}
		walked++;

		for (unsigned j = 0; j < found; j++) {
			if (memb_out[j] == HR_VAL(h->hr_ring[i].kv_value)) {
				already_found = true;
				break;
			}
		}

		if (already_found)
			continue;

		memb_out[found] = HR_VAL(h->hr_ring[i].kv_value);
		found++;
	}

out:
	return error;
}

/*
 * Does not clean dst first.
 *
 * Given a buf m (can be null) and size of buf (can be zero), copy src to dst.
 * If m isn't big enough, returns new size for caler to allocate. On success,
 * returns zero.
 */
size_t
hash_ring_copy(struct hash_ring *dst, struct hash_ring *src, void *m, size_t sz)
{
	size_t ring_size;

#ifdef INVARIANTS
	ASSERT(src->hr_initialized);
#endif

	ring_size = src->hr_ring_used * sizeof(struct hr_kv_pair);

	if (sz < ring_size) {
		if (m != NULL)
			free(m, src->hr_mtype);
		return ring_size;
	}

	memcpy(dst, src, sizeof *dst);

	if (ring_size > 0) {
		memcpy(m, src->hr_ring, ring_size);
		dst->hr_ring = m;
	} else {
		if (m != NULL)
			free(m, src->hr_mtype);
		dst->hr_ring = NULL;
	}

	return 0;
}

void
hash_ring_swap(struct hash_ring *h1, struct hash_ring *h2)
{
	struct hash_ring t;

	memcpy(&t, h1, sizeof t);
	memcpy(h1, h2, sizeof *h1);
	memcpy(h2, &t, sizeof *h2);
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
 * Borrowed from BSD libc/stdlib/bsearch.c.
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
			return __DECONST(void *, p);
		if (cmp > 0) {	/* key > p: move right */
			base = (const char *)p + size;
			lim--;
		}		/* else move left */
	}
	return __DECONST(void *, base);
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
add_ring_item(struct hash_ring *h, uint32_t hash, uint32_t member_)
{
	struct hr_kv_pair *insert, *end, newpair;
	uint32_t member = HR_VAL(member_);

	ASSERT_DEBUG(h->hr_ring_capacity - h->hr_ring_used >= 1);
	
	newpair.kv_hash = hash;
	newpair.kv_value = member;

	/* Find the point at which this entry should be inserted */
	insert = bsearch_or_next(&newpair, h->hr_ring, h->hr_ring_used,
	    sizeof newpair, hr_kv_cmp);

	end = h->hr_ring + h->hr_ring_used;

	if (insert != end && insert->kv_hash == hash) {
		/* Collision on 'hash', lowest value wins */
		if (member < HR_VAL(insert->kv_value))
			insert->kv_value = member_;
		return;
	}

	/* We insert in *front* of 'insert' */
	if (insert != end)
		memmove(insert + 1, insert, (end - insert) * sizeof newpair);

	ASSERT_DEBUG(insert == h->hr_ring || (insert-1)->kv_hash < hash);
	ASSERT_DEBUG(insert == end || hash < (insert+1)->kv_hash);

	*insert = newpair;
	h->hr_ring_used++;
}

static void
remove_ring_item(struct hash_ring *h, uint32_t hash, uint32_t member_)
{
	struct hr_kv_pair *remove, *end, rpair;
	uint32_t member = HR_VAL(member_);

	rpair.kv_hash = hash;
	rpair.kv_value = member;

	end = h->hr_ring + h->hr_ring_used;
	remove = bsearch(&rpair, h->hr_ring, h->hr_ring_used,
	    sizeof(struct hr_kv_pair), hr_kv_cmp);

	if (remove == NULL || HR_VAL(remove->kv_value) != member)
		return;

	if (remove + 1 != end)
		memmove(remove, remove + 1,
		    (end - remove - 1) * sizeof(struct hr_kv_pair));

	h->hr_ring_used--;
}

static void
rehash(struct hash_ring *h, uint32_t *memb)
{
	uint8_t hashdata[8];
	unsigned nmemb = 0,
		 i, j;
	uint32_t reps;

	/* Extract all members... */
	for (i = 0; i < h->hr_ring_used; i++) {
		uint32_t m = h->hr_ring[i].kv_value;

		for (j = 0; j < nmemb; j++)
			if (memb[j] == m)
				break;
		if (j >= nmemb) {
			memb[j] = m;
			nmemb++;
		}
	}

	/* Re-add all hashes... hurray */
	for (i = 0; i < nmemb; i++) {
		unsigned weightpct;

		le32enc(hashdata, HR_VAL(memb[i]));
		weightpct = HR_WEIGHT(memb[i]);

		reps = weightpct * h->hr_nreplicas / 100;
		if (reps == 0)
			reps = 1;

		for (uint32_t r = 0; r < reps; r++) {
			uint32_t rhash;

			le32enc(&hashdata[4], r);
			rhash = h->hr_hash_fn(hashdata, sizeof hashdata);
			add_ring_item(h, rhash, memb[i]);
		}
	}
}

static void
ring_fixup_weights(struct hash_ring *h, uint32_t mempair)
{
	struct hr_kv_pair *it, *end;
	uint32_t member = HR_VAL(mempair);

	end = &h->hr_ring[h->hr_ring_used];
	for (it = h->hr_ring; it < end; it++)
		if (HR_VAL(it->kv_value) == member)
			it->kv_value = mempair;
}
