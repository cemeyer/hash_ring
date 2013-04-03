#ifndef ISI_HASH_H
#define ISI_HASH_H

#include <stdint.h>
#include <stdlib.h>

uint32_t	isi_hash32(const void *_k, size_t len, uint32_t initval);
uint64_t	isi_hash64(const void *_k, size_t len, uint64_t initval);

#endif
