#ifndef _T_BIAS_H
#define _T_BIAS_H

#include <check.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <zlib.h>

#include "hashring.h"
#include "isi_hash.h"
#include "MurmurHash3.h"
#include "crc32c.h"

/* Add bias tests to check suite */
void suite_add_t_bias(Suite *s);
void suite_add_t_biased(Suite *s);

extern const struct hash_compare {
	const char	*name;
	hr_hasher_t	 hash;
	bool		 usable;
} comparison_functions[];

/* Various normalized 'hash' functions */
uint32_t djb_hasher(const void *data, size_t len);
uint32_t isi_hasher64(const void *data, size_t len);
uint32_t isi_hasher32(const void *data, size_t len);
uint32_t md5_hasher(const void *data, size_t len);
uint32_t sha1_hasher(const void *data, size_t len);
uint32_t mmh3_32_hasher(const void *data, size_t len);
uint32_t mmh3_128_hasher(const void *data, size_t len);
uint32_t crc32er(const void *data, size_t len);
uint32_t crc32cer(const void *vdata, size_t len);
uint32_t siphasher(const void *d, size_t len);

#endif
