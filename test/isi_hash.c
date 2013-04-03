#include "isi_hash.h"

/*
 * This hash function code is taken from Robert Jenkins. See
 * http://burtleburtle.net/bob/hash/evahash.html
 */

/**
 * @param _k		the key
 * @param len		the length of the key in bytes
 * @param initval	the previous hash, or an arbitrary value
 */
uint32_t
isi_hash32(const void *_k, size_t len, uint32_t initval)
{
#define mix(a, b, c)				\
	do {					\
		a -= b; a -= c; a ^= (c >> 13);	\
		b -= c; b -= a; b ^= (a << 8);	\
		c -= a; c -= b; c ^= (b >> 13);	\
		a -= b; a -= c; a ^= (c >> 12);	\
		b -= c; b -= a; b ^= (a << 16);	\
		c -= a; c -= b; c ^= (b >> 5);	\
		a -= b; a -= c; a ^= (c >> 3);	\
		b -= c; b -= a; b ^= (a << 10);	\
		c -= a; c -= b; c ^= (b >> 15);	\
	} while (0)

	const uint8_t *k = _k;
	uint32_t a = 0x9e3779b9, b = 0x9e3779b9, c = initval;

	/* Hash chunks of 12 bytes. */
	while (len >= 12) {
		a += *(const uint32_t *)&k[0];
		b += *(const uint32_t *)&k[4];
		c += *(const uint32_t *)&k[8];
		mix(a, b, c);
		k += 12;
		len -= 12;
	}

	/* 
	 * Hash the remainder byte-by-byte, until it reaches a multiple of
	 * 4 bytes.
	 */
	switch (len) {
	case 11:
		c += (uint32_t)k[10] << 16;
	case 10:
		c += (uint32_t)k[9] << 8;
	case 9:
		c += (uint32_t)k[8];
	case 8:
		b += *(const uint32_t *)&k[4];
		a += *(const uint32_t *)&k[0];
		break;
	case 7:
		b += (uint32_t)k[6] << 16;
	case 6:
		b += (uint32_t)k[5] << 8;
	case 5:
		b += (uint32_t)k[4];
	case 4:
		a += *(const uint32_t *)&k[0];
		break;
	case 3:
		a += (uint32_t)k[2] << 16;
	case 2:
		a += (uint32_t)k[1] << 8;
	case 1:
		a += (uint32_t)k[0];
	}
	mix(a, b, c);

	return c;

#undef mix
}

/**
 * @param _k		the key
 * @param len		the length of the key in bytes
 * @param level		the previous hash, or an arbitrary value
 */
uint64_t
isi_hash64(const void *_k, size_t len, uint64_t level)
{
#define mix(a, b, c)				\
	do {					\
		a -= b; a -= c; a ^= (c >> 43);	\
		b -= c; b -= a; b ^= (a << 9);	\
		c -= a; c -= b; c ^= (b >> 8);	\
		a -= b; a -= c; a ^= (c >> 38);	\
		b -= c; b -= a; b ^= (a << 23);	\
		c -= a; c -= b; c ^= (b >> 5);	\
		a -= b; a -= c; a ^= (c >> 35);	\
		b -= c; b -= a; b ^= (a << 49);	\
		c -= a; c -= b; c ^= (b >> 11);	\
		a -= b; a -= c; a ^= (c >> 12);	\
		b -= c; b -= a; b ^= (a << 18);	\
		c -= a; c -= b; c ^= (b >> 22);	\
	} while (0)

	const uint8_t *k = _k;
	uint64_t a = level, b = level, c = 0x9e3779b97f4a7c13ULL;

	/* Hash chunks of 24 bytes. */
	while (len >= 24) {
		a += *(const uint64_t *)&k[0];
		b += *(const uint64_t *)&k[8];
		c += *(const uint64_t *)&k[16];
		mix(a, b, c);
		k += 24;
		len -= 24;
	}

	/*
	 * Hash the remainder byte-by-byte, until it reaches a multiple of
	 * 8 bytes.
	 */
	switch (len) {
	case 23:
		c += ((uint64_t)k[22] << 48);
	case 22:
		c += ((uint64_t)k[21] << 40);
	case 21:
		c += ((uint64_t)k[20] << 32);
	case 20:
		c += *(const uint32_t *)&k[16];
		b += *(const uint64_t *)&k[8];	/* 8 .. 15 */
		a += *(const uint64_t *)&k[0];	/* 0 .. 7 */
		break;
	case 19:
		c += ((uint64_t)k[18] << 16);
	case 18:
		c += ((uint64_t)k[17] << 8);
	case 17:
		c += ((uint64_t)k[16]);
	case 16:
		b += *(const uint64_t *)&k[8];
		a += *(const uint64_t *)&k[0];
		break;
	case 15:
		b += ((uint64_t)k[14] << 48);
	case 14:
		b += ((uint64_t)k[13] << 40);
	case 13:
		b += ((uint64_t)k[12] << 32);
	case 12:
		b += *(const uint32_t *)&k[8];	/* 8 .. 11 */
		a += *(const uint64_t *)&k[0];	/* 0 .. 7 */
		break;
	case 11:
		b += ((uint64_t)k[10] << 16);
	case 10:
		b += ((uint64_t)k[9] << 8);
	case  9:
		b += ((uint64_t)k[8]);
	case  8:
		a += *(const uint64_t *)&k[0];
		break;
	case  7:
		a += ((uint64_t)k[6] << 48);
	case  6:
		a += ((uint64_t)k[5] << 40);
	case  5:
		a += ((uint64_t)k[4] << 32);
	case  4:
		a += *(const uint32_t *)&k[0]; /* 0 .. 3 */
		break;
	case  3:
		a += ((uint64_t)k[2] << 16);
	case  2:
		a += ((uint64_t)k[1] << 8);
	case  1:
		a += ((uint64_t)k[0]);
	}

	mix(a, b, c);

	return c;

#undef mix
}
