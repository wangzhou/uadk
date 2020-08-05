/* SPDX-License-Identifier: Apache-2.0 */
#ifndef	__HISI_SEC_H
#define	__HISI_SEC_H

#include "hisi_qm_udrv.h"
#include "include/sec_usr_if.h"
#include "wd.h"
#include "wd_cipher.h"
#include "wd_digest.h"

enum C_ALG {
	C_ALG_DES  = 0x0,
	C_ALG_3DES = 0x1,
	C_ALG_AES  = 0x2,
	C_ALG_SM4  = 0x3,
};

enum A_ALG {
	A_ALG_SHA1	 = 0x0,
	A_ALG_SHA256 = 0x1,
	A_ALG_MD5	 = 0x2,
	A_ALG_SHA224 = 0x3,
	A_ALG_SHA384 = 0x4,
	A_ALG_SHA512 = 0x5,
	A_ALG_SHA512_224 = 0x6,
	A_ALG_SHA512_256 = 0x7,
	A_ALG_HMAC_SHA1   = 0x10,
	A_ALG_HMAC_SHA256 = 0x11,
	A_ALG_HMAC_MD5	  = 0x12,
	A_ALG_HMAC_SHA224 = 0x13,
	A_ALG_HMAC_SHA384 = 0x14,
	A_ALG_HMAC_SHA512 = 0x15,
	A_ALG_HMAC_SHA512_224 = 0x16,
	A_ALG_HMAC_SHA512_256 = 0x17,
	A_ALG_AES_XCBC_MAC_96  = 0x20,
	A_ALG_AES_XCBC_PRF_128 = 0x20,
	A_ALG_AES_CMAC = 0x21,
	A_ALG_AES_GMAC = 0x22,
	A_ALG_SM3	   = 0x25,
	A_ALG_HMAC_SM3 = 0x26
};

enum C_MODE {
	C_MODE_ECB	  = 0x0,
	C_MODE_CBC	  = 0x1,
	C_MODE_CTR	  = 0x4,
	C_MODE_CCM	  = 0x5,
	C_MODE_GCM	  = 0x6,
	C_MODE_XTS	  = 0x7,
	C_MODE_CBC_CS     = 0x9,
};

enum C_KEY_LEN {
	CKEY_LEN_128BIT = 0x0,
	CKEY_LEN_192BIT = 0x1,
	CKEY_LEN_256BIT = 0x2,
	CKEY_LEN_SM4    = 0x0,
	CKEY_LEN_DES    = 0x1,
	CKEY_LEN_3DES_3KEY = 0x1,
	CKEY_LEN_3DES_2KEY = 0x3,
};

enum {
	NO_AUTH,
	AUTH_HMAC_CALCULATE,
	AUTH_MAC_VERIFY,
};

enum sec_cipher_dir {
	SEC_CIPHER_ENC = 0x1,
	SEC_CIPHER_DEC = 0x2,
};

struct wd_cipher_msg {
	__u8 alg_type;		/* Denoted by enum wcrypto_type */
	__u8 alg;		/* Denoted by enum wcrypto_cipher_type */
	__u8 op_type;		/* Denoted by enum wcrypto_cipher_op_type */
	__u8 mode;		/* Denoted by enum wcrypto_cipher_mode_type */
	__u8 data_fmt;		/* Data format, include pbuffer and sgl */
	__u8 result;		/* Operation result, denoted by WD error code */

	__u16 key_bytes;	/* Key bytes */
	__u16 iv_bytes;		/* iv bytes */
	__u32 in_bytes;		/* in bytes */
	__u32 out_bytes;	/* out_bytes */

	__u8 *key;		/* input key pointer */
	__u8 *iv;		/* input iv pointer */
	__u8 *in;		/* input data pointer */
	__u8 *out;		/* output data pointer  */
};

extern int hisi_sec_init(struct wd_ctx_config *config, void *priv);
extern void hisi_sec_exit(void *priv);
extern int hisi_cipher_poll(handle_t ctx, __u32 num);
extern int hisi_digest_poll(handle_t ctx, __u32 num);
extern int hisi_sec_cipher_send(handle_t ctx, struct wd_cipher_msg *msg);
extern int hisi_sec_cipher_recv(handle_t ctx, struct wd_cipher_msg *msg);
extern int hisi_sec_cipher_recv_async(handle_t ctx, struct wd_cipher_req *req);
#endif	/* __HISI_SEC_H */
