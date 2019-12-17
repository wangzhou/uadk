/* SPDX-License-Identifier: Apache-2.0 */
/* the common drv header define the unified interface for wd */
#ifndef __WD_UTIL_H__
#define __WD_UTIL_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/queue.h>

#include "wd.h"

#define WD_HW_ERR		62

/* WD error code */
#define	WD_SUCCESS		0
#define	WD_STREAM_END		1
#define	WD_EAGAIN		EAGAIN
#define	WD_ENOMEM		ENOMEM
#define	WD_EACCESS		EACCESS
#define	WD_EBUSY		EBUSY
#define	WD_ENODEV		ENODEV
#define	WD_EINVAL		EINVAL
#define	WD_ETIMEDOUT		ETIMEDOUT
#define	WD_MSG_PARA_ERR	61
#define	WD_HW_EACCESS		62
#define	WD_SGL_ERR		63
#define	WD_VERIFY_ERR		64

#define BYTE_BITS			8
#define BYTE_BITS_SHIFT		3
#define CRT_PARAMS_SZ(key_size)		((5 * (key_size)) >> 1)
#define CRT_GEN_PARAMS_SZ(key_size)	((7 * (key_size)) >> 1)
#define GEN_PARAMS_SZ(key_size)		((key_size) << 1)
#define CRT_PARAM_SZ(key_size)		((key_size) >> 1)
#define GET_NEGATIVE(val)	(0 - (val))

struct wd_lock {
	__u32 lock;
};

struct wd_ss_region {
	void *va;
	unsigned long long pa;
	size_t size;

	TAILQ_ENTRY(wd_ss_region) next;
};

TAILQ_HEAD(wd_ss_region_list, wd_ss_region);

typedef void (*wcrypto_cb)(const void *msg, void *tag);

/* memory APIs for Algorithm Layer */
typedef void *(*wd_alloc)(void *usr, size_t size);
typedef void (*wd_free)(void *usr, void *va);

struct wcrypto_cb_tag {
	void *ctx;
	void *tag;
	int ctx_id;
};

struct wcrypto_paras {
	 /* 0--encipher/compress .etc, 1 ---decipher/decomp .etc */
	__u8 direction;

	 /* to be extended */
};

enum wcrypto_type {
	WD_RSA,
	WD_DH,
	WD_CIPHER,
	WD_DIGEST,
	WD_COMP,
	WD_EC,
	WD_RNG,
	WD_MAX_ALG,
};

/* Memory from user, it is given at ctx creating. */
struct wd_mm_ops {
	wd_alloc alloc;
	wd_free free;
//	wd_dmap dma_map;
//	wd_undmap dma_unmap;
	void *usr; /* data for the above operations */
};

struct q_info {
	const char *hw_type;
	int hw_type_id;
	int ref;
	void *priv; /* private data used by the drv layer */
	const void *dev_info;
	void *ss_va;
	int fd;
	int iommu_type;
	struct wd_ss_region_list ss_list;
	struct wd_ss_region_list *head;
	unsigned int dev_flags;
	unsigned int ss_size;
	enum wcrypto_type atype;
	int ctx_num;
	struct wd_mm_ops ops;
	unsigned long qfrs_offset[UACCE_QFRT_MAX];
	struct wd_lock qlock;
};

/* Digest tag format of Warpdrive */
struct wcrypto_digest_tag {
	struct wcrypto_cb_tag wcrypto_tag;
	__u64 long_data_len;
	void *priv;
};

/* Cipher tag format of Warpdrive */
struct wcrypto_cipher_tag {
	struct wcrypto_cb_tag wcrypto_tag;
	void *priv;
};

/*EC tag format of Warpdrive */
struct wcrypto_ec_tag {
	struct wcrypto_cb_tag wcrypto_tag;
	__u64 tbl_addr;
	__u64 priv_data;
};

#ifdef DEBUG_LOG
#define dbg(msg, ...) fprintf(stderr, msg, ##__VA_ARGS__)
#else
#define dbg(msg, ...)
#endif

#ifdef DEBUG
#define ASSERT(f) assert(f)
#else
#define ASSERT(f)
#endif

#if defined(__AARCH64_CMODEL_SMALL__) && __AARCH64_CMODEL_SMALL__

//#define dsb(opt)	{ asm volatile("dsb " #opt : : : "memory"); }
#define rmb()		dsb(ld)	/* read fence */
#define wmb()		dsb(st)	/* write fence */
#define mb()		dsb(sy)	/* rw fence */

#else

#define rmb()	/* read fence */
#define wmb()	/* write fence */
#define mb()	/* rw fence */
#ifndef __UT__
#error "no platform mb, define one before compiling"
#endif

#endif

#if 0
static inline void wd_reg_write(void *reg_addr, uint32_t value)
{
	*((uint32_t *)reg_addr) = value;
	wmb();	/* load fence */
}

static inline uint32_t wd_reg_read(void *reg_addr)
{
	uint32_t temp;

	temp = *((uint32_t *)reg_addr);
	rmb();	/* load fence */

	return temp;
}
#endif
void wd_spinlock(struct wd_lock *lock);
void wd_unspinlock(struct wd_lock *lock);
void *wd_drv_mmap_qfr(struct wd_queue *q, enum uacce_qfrt qfrt,
				    enum uacce_qfrt qfrt_next, size_t size);
void wd_drv_unmmap_qfr(struct wd_queue *q, void *addr,
				     enum uacce_qfrt qfrt,
				     enum uacce_qfrt qfrt_next, size_t size);
//void *drv_dma_map(struct wd_queue *q, void *va, size_t sz);
//void drv_dma_unmap(struct wd_queue *q, void *va, void *dma, size_t sz);
#endif
