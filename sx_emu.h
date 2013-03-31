#ifndef SX_EMU_H
#define SX_EMU_H

#if LINUX_USERSPACE
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <pthread.h>

struct sx {
	pthread_rwlock_t lock;
};

static inline void
sx_xlock(struct sx *lk)
{
	int r;
	r = pthread_rwlock_wrlock(&lk->lock);
	assert(r == 0);
}

static inline void
sx_slock(struct sx *lk)
{
	int r;
	r = pthread_rwlock_rdlock(&lk->lock);
	assert(r == 0);
}

static inline void
sx_unlock(struct sx *lk)
{
	int r;
	r = pthread_rwlock_unlock(&lk->lock);
	assert(r == 0);
}

static inline void
sx_init(struct sx *lk, const char *desc)
{
	int r;
	(void)desc;
	r = pthread_rwlock_init(&lk->lock, NULL);
	assert(r == 0);
}

static inline void
sx_destroy(struct sx *lk)
{
	int r;
	r = pthread_rwlock_destroy(&lk->lock);
	assert(r == 0);
}

enum {
	SX_XLOCKED,
};

static inline void
sx_assert(struct sx *lk, int sx_what)
{

	(void)lk;
	(void)sx_what;
}

static void
le32enc(void *v, uint32_t u)
{
	uint8_t *d = v;

	d[0] = (uint8_t)(u & 0xff);
	d[1] = (uint8_t)((u>>8) & 0xff);
	d[2] = (uint8_t)((u>>16) & 0xff);
	d[3] = (uint8_t)((u>>24) & 0xff);
}
#endif  /* LINUX_USERSPACE */

#endif  /* SX_EMU_H */
