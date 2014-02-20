#ifdef __FreeBSD__
# include <sys/endian.h>
#else
# include <stdint.h>

static inline void
le32enc(void *vp, uint32_t val)
{
	uint8_t *bp = vp;

	bp[0] = val & 0xff;
	bp[1] = (val >> 8) & 0xff;
	bp[2] = (val >> 16) & 0xff;
	bp[3] = val >> 24;
}
#endif  /* !__FreeBSD__ */

#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "t_bias.h"
#include "siphash24.h"

struct histogram {
	uint64_t min,
		 max;
	unsigned nbuckets,
		 histheight;
	uint64_t buckets[0];
};

struct histogram *
new_histogram(uint64_t min, uint64_t max, unsigned nbuckets, unsigned height)
{
	struct histogram *res;

	fail_unless(max > min);

	res = malloc(sizeof *res + nbuckets * sizeof res->buckets[0]);
	fail_unless((uintptr_t)res);

	res->min = min;
	res->max = max;
	res->nbuckets = nbuckets;
	res->histheight = height;
	memset(res->buckets, 0, nbuckets * sizeof res->buckets[0]);

	return res;
}

void
free_histogram(struct histogram *h)
{

	free(h);
}

void
hist_add(struct histogram *h, uint64_t val)
{
	unsigned bucket;

	bucket = (val - h->min) * h->nbuckets / (h->max - h->min);
	h->buckets[bucket]++;
}

static void
xprint(const char *s, ...)
{
#if 0
	va_list ap;

	va_start(ap, s);
	vprintf(s, ap);
	va_end(ap);
#else
	(void)s;
#endif
}

struct hist_summary {
	double rootssd;
};

/*
 * Normalize to maximum bucket, or to sum. Sum means histograms with the same
 * number of points will all have the same scale; max means the histograms will
 * use as much space is possible to graph variance.
 */
#define NMLZ_MAX 1

struct hist_summary
hist_print(struct histogram *h)
{
	uint64_t max, sum, sumsqdiff, uniform;
	unsigned i, j, uni, act;
	double rootssd;

	sum = 0;
	max = 0;
	sumsqdiff = 0;
	for (i = 0; i < h->nbuckets; i++) {
		sum += h->buckets[i];
		if (h->buckets[i] > max)
			max = h->buckets[i];
	}

	for (i = 0; i < h->histheight; i++)
		xprint("-");
	xprint("\n");

	uniform = (sum + h->nbuckets - 1) / h->nbuckets;
#if 0
	xprint("Exp. Uniform value: %lu (max: %lu)\n", uniform, max);
#endif
#if NMLZ_MAX == 1
	uni = (uniform * h->histheight + max - 1) / max;
#else
	uni = (uniform * h->histheight + sum - 1) / sum;
#endif

	for (i = 0; i < h->nbuckets; i++) {
#if 0
		xprint("Bucket value: %lu\n", h->buckets[i]);
#endif
#if NMLZ_MAX == 1
		act = (h->buckets[i] * h->histheight + max - 1) / max;
#else
		act = (h->buckets[i] * h->histheight + sum - 1) / sum;
#endif
		for (j = 0; j <= act; j++) {
			if (act > uni && j == uni)
				xprint("|");
			else
				xprint("#");
		}

		for (; j < uni; j++)
			xprint(" ");
		if (act < uni)
			xprint("|");
		xprint("\n");

		sumsqdiff += (h->buckets[i] < uniform)?
		    (uniform - h->buckets[i])*(uniform - h->buckets[i]) :
		    (h->buckets[i] - uniform)*(h->buckets[i] - uniform);
	}

	for (i = 0; i < h->histheight; i++)
		xprint("-");
	xprint("\n");

	rootssd = sqrt(sumsqdiff);
	xprint("Sqrt Sum Sq diff: %.02g\n", rootssd);
	return (struct hist_summary){ .rootssd = rootssd };
}

/*
 * This hash function is very far from adequate. This will need some
 * investigating. We need a function that gives good distribution for short,
 * constant-sized keys. PRNG?
 */
uint32_t
djb_hasher(const void *data, size_t len)
{
	uint32_t hash = 5381;
	const uint8_t *d = data;

	for (; len > 0; len--) {
		hash *= 33;
		hash += *d;
		d++;
	}

	return hash;
}

static uint32_t
be32dec(const void *v)
{
	const uint8_t *d = v;
	uint32_t res;

	res = ((uint32_t)d[0] << 24) |
	    ((uint32_t)d[1] << 16) |
	    ((uint32_t)d[2] << 8) |
	    (uint32_t)d[3];
	return res;
}

uint32_t
isi_hasher64(const void *data, size_t len)
{
	uint64_t res;

	res = isi_hash64(data, len, 0);
	return (uint32_t)res;
}

uint32_t
isi_hasher32(const void *data, size_t len)
{

	return isi_hash32(data, len, 0);
}

/*
 * This is overkill...
 */
uint32_t
md5_hasher(const void *data, size_t len)
{
	unsigned char md[MD5_DIGEST_LENGTH];

	MD5(data, len, md);

	return be32dec(md);
}

uint32_t
sha1_hasher(const void *data, size_t len)
{
	unsigned char md[SHA_DIGEST_LENGTH];

	SHA1(data, len, md);

	return be32dec(md);
}

uint32_t
mmh3_32_hasher(const void *data, size_t len)
{
	unsigned char md[4];

	MurmurHash3_x86_32(data, len, 0x0/*seed*/, md);

	return be32dec(md);
}

uint32_t
mmh3_128_hasher(const void *data, size_t len)
{
	unsigned char md[16];

	MurmurHash3_x64_128(data, len, 0x0/*seed*/, md);

	return be32dec(md);
}

uint32_t
crc32er(const void *data, size_t len)
{
	uint32_t crc = crc32(0L, Z_NULL, 0);

	return crc32(crc, data, len);
}

uint32_t
crc32cer(const void *vdata, size_t len)
{
	const uint8_t *data = vdata;
	uint32_t crc = ~(uint32_t)0;

	for (size_t i = 0; i < len; i++)
		CRC32C(crc, data[i]);

	return __builtin_bswap32(crc);
}

uint32_t
siphasher(const void *d, size_t len)
{
	const uint64_t k[2] = {
		0xe276920babca796dULL,
		0x443ef008123a77ceULL,
	};
	uint64_t sr;

	sr = siphash24(d, len, k);
	return (sr >> 32) ^ sr;
}



static struct hist_summary
sample_hr(struct histogram *h, const struct hash_compare *hc, unsigned drives)
{
	struct hist_summary hs;
	uint8_t val[8];
	uint32_t i, j;
	uint64_t res;

	for (i = 0; i < drives; i++) {
		for (j = 0; j < 64; j++) {
			le32enc(val, i);
			le32enc(&val[4], j);

			res = hc->hash(val, sizeof val);
			hist_add(h, res);
		}
	}

	hs = hist_print(h);
	printf("Hash:: %s (rootssd=%0.02g)\n", hc->name, hs.rootssd);
	return hs;
}


START_TEST(bias_ring)
{
	const struct hash_compare *hc, *bhc = NULL;
	struct histogram *h;
	const unsigned HISTHEIGHT = 79;
	double besterr, err;
	struct hist_summary hs;
	unsigned buckets, dr;

	for (dr = 1; dr < 8; dr++) {
		besterr = DBL_MAX;
		buckets = 64*dr;

		printf("# drives: %d\n", dr);

		for (hc = comparison_functions; hc->name != NULL; hc++) {
			h = new_histogram(0, UINT32_MAX, buckets, HISTHEIGHT);

			hs = sample_hr(h, hc, dr);
			err = hs.rootssd;
			if (err < besterr && hc->usable) {
				besterr = err;
				bhc = hc;
			}

			free_histogram(h);
		}

		printf("%d: Best root(sum err^2) = %s (%.02g)\n", dr,
		    bhc->name, besterr);
	}
}
END_TEST

void
suite_add_t_bias(Suite *s)
{
	TCase *t;
	
	t = tcase_create("bias_ring");
	tcase_add_test(t, bias_ring);
	suite_add_tcase(s, t);
}
