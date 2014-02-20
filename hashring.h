/*
 * Copyright (c) 2013 Conrad Meyer <cse.cem@gmail.com>
 *
 * This is available for use under the terms of the MIT license, see the
 * LICENSE file.
 *
 * Implementation of a consistent hashing instance, derived from the
 * MIT-licensed Go-language implementation, 'consistent,' available here:
 *         https://github.com/stathat/consistent
 *
 * Note: Users are responsible for ensuring access is appropriately serialized.
 * 'clean()', 'add()', and 'remove()' should be performed only with exclusive
 * access. 'getn()' can be performed with shared locking (so long as the
 * modifying calls are excluded).
 */

#ifndef _HASHRING_H_
#define _HASHRING_H_

#ifdef _KERNEL
# ifndef __FreeBSD__
#  error Unsupported kernel, figure out how to include stdint yourself
# endif
# include <sys/malloc.h>
# include <sys/stdint.h>
#else /* !_KERNEL */
# include <stdint.h>
struct malloc_type;
#endif

#include <stdbool.h>

/*
 * ===============================================================
 * Public API
 * ===============================================================
 */

typedef uint32_t	(*hr_hasher_t)(const void *, size_t);

struct hash_ring;

/*
 * Initializes a hash_ring @h. @hash should be a good hashing function for
 * short keys, and @nreplicas should be fairly high (64 seems reasonable).
 */
void	hash_ring_init(struct hash_ring *h, hr_hasher_t hash,
		       struct malloc_type *mt, uint32_t nreplicas);

/* Cleans a hash_ring @h. */
void	hash_ring_clean(struct hash_ring *h);

/*
 * Increases the @weightpct (1-100) of @member in @h (potentially from zero,
 * i.e., not present). If member's weight is greater than @weightpct, does
 * nothing. A non-zero @weightpct will always round up to at least one entry in
 * the ring.
 *
 * If newmemb isn't big enough, fails and returns a size of buffer for caller
 * to allocate. On success, returns zero.
 *
 * Only the low 24 bits of member are usable.
 */
size_t	hash_ring_add(struct hash_ring *h, uint32_t member, unsigned weightpct,
		      void *newmemb, size_t sz);

/*
 * Decreases the @weightpct (0-99) of @member in @h (potentially to zero, i.e.,
 * full removal). If the member is already absent or its weight is less than
 * @weightpct, does nothing. As with hash_ring_add(), a non-zero @weightpct
 * will always round up to at least one entry in the ring.
 *
 * Like add(), takes an auxiliary buf and returns a new size if this one isn't
 * big enough. The passed buf is always freed. On success, returns zero.
 */
size_t	hash_ring_remove(struct hash_ring *h, uint32_t member,
			 unsigned weightpct, void *aux, size_t sz);

/*
 * Gets @n (1 or more) replicas from the hash_ring @h appropriate for key
 * @hash, putting them in the array @memb_out, which must be large enough for
 * @n results.
 *
 * Returns zero on success or an error code on error.
 *
 * EINVAL - @n is zero
 * ENOENT - If the request is unsatisfiable (for example, because fewer members
 *          exist)
 */
int	hash_ring_getn(struct hash_ring *h, uint32_t hash, unsigned n,
		       uint32_t *memb_out);

/*
 * Copies a hash_ring object.
 *
 * Does not clean dst first.
 *
 * Given a buf m (can be null) and size of buf (can be zero), copy src to dst.
 * If m isn't big enough, returns new size for caller to allocate. On success,
 * returns zero.
 */
size_t	hash_ring_copy(struct hash_ring *dst, struct hash_ring *src, void *m,
		       size_t sz);

/*
 * ===============================================================
 * Private! Do not access any of these directly.
 * ===============================================================
 */

struct hr_kv_pair {
	uint32_t	 kv_hash;
	uint32_t	 kv_value;
};

struct hash_ring {
	hr_hasher_t		 hr_hash_fn;
	struct malloc_type	*hr_mtype;

	/* Sorted hash->value map */
	struct hr_kv_pair	*hr_ring;
	/* In units of struct hr_kv_pair: */
	size_t			 hr_ring_used;
	size_t			 hr_ring_capacity;

	/* No. of replicas per member in map */
	uint32_t		 hr_nreplicas;

#ifdef INVARIANTS
	bool			 hr_initialized;
#endif
};

#endif  /* _HASHRING_H_ */
