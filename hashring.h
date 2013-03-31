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

#ifndef HASHRING_H
#define HASHRING_H

#include <stdbool.h>
#include <stdint.h>

#define LINUX_USERSPACE 1
#include "sx_emu.h"

typedef uint32_t (*hasher_t)(const void *, size_t);

struct hash_ring;

/*
 * Initializes a hash_ring @h. @hash should be a good hashing function for
 * short keys, and @nreplicas should be fairly high (20-128 seems reasonable).
 */
void	hash_ring_init(struct hash_ring *h, hasher_t hash, uint32_t nreplicas);

/*
 * Adds a member to hash_ring @h. @member should not already be included in
 * this ring; this routine makes no attempt to validate that.
 */
void	hash_ring_add(struct hash_ring *h, uint32_t member);

/*
 * Remove a member from the hash_ring @h. @member should be present in the
 * ring; this routine does not validate that.
 */
void	hash_ring_remove(struct hash_ring *h, uint32_t member);

/*
 * Gets @n (1 or more) replicas from the hash_ring @h appropriate for key
 * @hash, putting them in the array @memb_out.
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
 * Do not access any fields of hash_ring directly...
 */
struct _kv_pair {
	uint32_t	 kv_hash;
	uint32_t	 kv_value;
};

struct hash_ring {
	struct sx	 hr_lock;
	hasher_t	 hr_hash_fn;

	/* No. of members of this ring */
	uint32_t	 hr_count;

	/* No. of replicas per member in map */
	uint32_t	 hr_nreplicas;

	/* Sorted hash->value map */
	struct _kv_pair	*hr_ring;
	size_t		 hr_ring_used;
	size_t		 hr_ring_capacity;
};

#endif
