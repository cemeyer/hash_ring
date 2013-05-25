#ifndef _SIPHASH24_H
#define _SIPHASH24_H

#include <stdint.h>

typedef uint64_t u64;
typedef uint8_t u8;

/*
 * SipHash-2-4
 */
u64 siphash24(const u8 *in, u64 inlen, const u64 k[2]);

#endif
