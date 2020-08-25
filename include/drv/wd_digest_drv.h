/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __WD_DIGEST_DRV_H
#define __WD_DIGEST_DRV_H

#include "include/wd_alg_common.h"

/* fixme wd_digest_msg */
struct wd_digest_msg {
	struct wd_digest_req req;
	__u32 tag_id;
	__u8 alg_type;		/* Denoted by enum wcrypto_type */
	__u8 alg;			/* Denoted by enum wcrypto_digest_type */
	__u8 has_next;		/* is there next block data */
	__u8 mode;			/* Denoted by enum wcrypto_digest_mode_type */
	__u8 data_fmt;		/* Data format, include pbuffer and sgl */
	__u8 result;		/* Operation result, denoted by WD error code */
	__u64 usr_data;		/* user identifier: struct wcrypto_cb_tag */

	__u16 key_bytes;	/* Key bytes */
	__u16 iv_bytes;		/* iv bytes */
	__u32 in_bytes;		/* in bytes */
	__u32 out_bytes;	/* out_bytes */

	__u8 *key;		/* input key pointer */
	__u8 *iv;		/* input iv pointer */
	__u8 *in;		/* input data pointer */
	__u8 *out;		/* output data pointer  */
};

struct wd_digest_driver {
	const char	*drv_name;
	const char	*alg_name;
	__u32	drv_ctx_size;
	int	(*init)(struct wd_ctx_config *config, void *priv);
	void	(*exit)(void *priv);
	int	(*digest_send)(handle_t ctx, struct wd_digest_msg *msg);
	int	(*digest_recv)(handle_t ctx, struct wd_digest_msg *msg);
};

void wd_digest_set_driver(struct wd_digest_driver *drv);

#ifdef WD_STATIC_DRV
#define WD_DIGEST_SET_DRIVER(drv)					      \
extern const struct wd_digest_driver wd_digest_##drv __attribute__((alias(#drv)));

#else
#define WD_DIGEST_SET_DRIVER(drv)					      \
static void __attribute__((constructor)) set_drivers(void)		      \
{									      \
	wd_digest_set_driver(&drv);					      \
}
#endif
#endif /* __WD_DIGEST_DRV_H */