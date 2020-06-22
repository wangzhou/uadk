/* SPDX-License-Identifier: Apache-2.0 */
#include <stdbool.h>
#include "hisi_sec.h"

#define SEC_DIGEST_ALG_OFFSET 11
#define BD_TYPE2 	      0x2
#define WORD_BYTES	      4
#define SEC_FLAG_OFFSET	      7
#define SEC_AUTH_OFFSET	      6
#define SEC_AUTH_KEY_OFFSET   5
#define SEC_HW_TASK_DONE      0x1
#define SEC_DONE_MASK	      0x0001
#define SEC_FLAG_MASK	      0x780
#define SEC_TYPE_MASK	      0x0f

#define SEC_COMM_SCENE	      0
#define SEC_SCENE_OFFSET	  3
#define SEC_CMOME_OFFSET	  12
#define SEC_CKEY_OFFSET		  9
#define SEC_CIPHER_OFFSET	  4
#define XTS_MODE_KEY_DIVISOR	  2

#define DES_KEY_SIZE		  8
#define SEC_3DES_2KEY_SIZE	  (2 * DES_KEY_SIZE)
#define SEC_3DES_3KEY_SIZE	  (3 * DES_KEY_SIZE)
#define AES_KEYSIZE_128		  16
#define AES_KEYSIZE_192		  24
#define AES_KEYSIZE_256		  32

/* should be removed to qm module */
struct hisi_qp_ctx_temp {
	handle_t h_ctx;
	void *sq_base;
	void *cq_base;
	int sqe_size;
	void *mmio_base;
	void *db_base;
	__u16 sq_tail_index;
	__u16 sq_head_index;
	__u16 cq_head_index;
	__u16 sqn;
	bool cqc_phase;
	void *req_cache[QM_Q_DEPTH];
	int is_sq_full;
	int (*db)(struct hisi_qp_ctx_temp *qp_ctx, __u8 cmd, __u16 index,
		  __u8 priority);
};

/* fix me: should be removed to qm module */
struct hisi_qp_ctx_temp *hisi_qm_alloc_qp_ctx_t(handle_t h_ctx)
{
	return NULL;
}

/* session like request ctx */
struct hisi_sec_sess {
	struct hisi_qp qp;
	char *node_path;
	void *key;
	__u32 key_bytes;
};

int hisi_sec_init(struct hisi_sec_sess *sec_sess)
{
	struct hisi_qp *qp = &sec_sess->qp;
	int ret;
	/* wd_request_ctx */
	sec_sess->qp.h_ctx = wd_request_ctx(sec_sess->node_path);

	/* alloc_qp_ctx */
	ret = hisi_qm_alloc_qp_ctx(sec_sess->node_path, qp);
	if (ret)
		return ret;

	/* update qm private info: sqe_size, op_type */

	/* update sec private info: something maybe */

	ret = wd_ctx_start(sec_sess->qp.h_ctx);
	if (ret) {
		WD_ERR("ctx start failed!\n");
		goto out_ctx;
	}

	return 0;
out_ctx:
	hisi_qm_free_ctx(sec_sess->qp.h_ctx);

	return ret;
}

void hisi_sec_exit(struct hisi_sec_sess *sec_sess)
{
	/* wd_ctx_stop */
	wd_ctx_stop(sec_sess->qp.h_ctx);

	/* free alloc_qp_ctx */
	hisi_qm_free_ctx(sec_sess->qp.h_ctx);

	/* wd_release_ctx */
	wd_release_ctx(sec_sess->qp.h_ctx);
}

int hisi_sec_set_key(struct hisi_sec_sess *sess, const __u8 *key, __u32 key_len)
{
	/* store key to sess */
	memcpy(sess->key, key, key_len);
	sess->key_bytes = key_len;

	return 0;
}

static int get_aes_c_key_len(struct wd_cipher_sess *sess, __u8 *c_key_len)
{
	struct hisi_sec_sess *sec_sess = (struct hisi_sec_sess *)sess->priv;
	__u16 len;

	len = sec_sess->key_bytes;

	if (sess->mode == WD_CIPHER_XTS)
		len = len / XTS_MODE_KEY_DIVISOR;

	switch(len) {
		case AES_KEYSIZE_128:
			*c_key_len = CKEY_LEN_128BIT;
			break;
		case AES_KEYSIZE_192:
			*c_key_len = CKEY_LEN_192BIT;
			break;
		case AES_KEYSIZE_256:
			*c_key_len = CKEY_LEN_256BIT;
			break;
		default:
			WD_ERR("Invalid AES key size!\n");
			return -EINVAL;
			break;
	}

	return 0;
}

static int get_3des_c_key_len(struct wd_cipher_sess *sess, __u8 *c_key_len)
{
	struct hisi_sec_sess *sec_sess = sess->priv;

	if (sec_sess->key_bytes == SEC_3DES_2KEY_SIZE) {
		*c_key_len = CKEY_LEN_3DES_2KEY;
	} else if (sec_sess->key_bytes == SEC_3DES_3KEY_SIZE) {
		*c_key_len = CKEY_LEN_3DES_3KEY;
	} else {
		WD_ERR("Invalid 3DES key size!\n");
		return -EINVAL;
	}

	return 0;
}

static int fill_cipher_bd2_alg(struct wd_cipher_sess *sess, struct hisi_sec_sqe *sqe)
{
	int ret = 0;
	__u8 c_key_len = 0;

	switch (sess->alg) {
	case WD_CIPHER_SM4:
		sqe->type2.c_alg = C_ALG_SM4;
		sqe->type2.icvw_kmode = CKEY_LEN_SM4 << SEC_CKEY_OFFSET;
		break;
	case WD_CIPHER_AES:
		sqe->type2.c_alg = C_ALG_AES;
		ret = get_aes_c_key_len(sess, &c_key_len);
		sqe->type2.icvw_kmode = (__u16)c_key_len << SEC_CKEY_OFFSET;
		break;
	case WD_CIPHER_DES:
		sqe->type2.c_alg = C_ALG_DES;
		sqe->type2.icvw_kmode = CKEY_LEN_DES;
		break;
	case WD_CIPHER_3DES:
		sqe->type2.c_alg = C_ALG_3DES;
		ret = get_3des_c_key_len(sess, &c_key_len);
		sqe->type2.icvw_kmode = (__u16)c_key_len << SEC_CKEY_OFFSET;
		break;
	default:
		WD_ERR("Invalid cipher type!\n");
		return -EINVAL;
		break;
	}

	return ret;
}

static int hisi_cipher_create_request(struct wd_cipher_sess *sess, struct wd_cipher_arg *arg,
				struct hisi_sec_sqe *sqe)
{
	struct hisi_sec_sess *sec_sess = sess->priv;
	__u8 scene, cipher;
	__u8 de = 1;
	int ret;

	/* config BD type */
	sqe->type_auth_cipher = BD_TYPE2;
	/* config scence */
	scene = SEC_COMM_SCENE << SEC_SCENE_OFFSET;
	sqe->sds_sa_type = (de | scene);

	sqe->type2.clen_ivhlen |= (__u32)arg->in_bytes;
	sqe->type2.data_src_addr = (__u64)arg->src;

	sqe->type2.data_dst_addr = (__u64)arg->dst;

	sqe->type2.c_ivin_addr = (__u64)arg->iv;

	ret = fill_cipher_bd2_alg(sess, sqe);
	if (ret) {
		WD_ERR("fill cipher bd2 alg failed!\n");
		return ret;
	}
	//sqe->type2.c_alg = (__u8)sess->alg;
	//sqe->type2.icvw_kmode |= (__u16)(sess->mode) << SEC_CMOME_OFFSET;
	if (arg->op_type == WD_CIPHER_ENCRYPTION)
		cipher = SEC_CIPHER_ENC << SEC_CIPHER_OFFSET;
	else
		cipher = SEC_CIPHER_DEC << SEC_CIPHER_OFFSET;

	// config key
	//sqe->type2.c_key_addr = (__u64)sec_sess->key;
	//sqe->type2.icvw_kmode |= (__u16)(sec_sess->key_bytes) << SEC_CKEY_OFFSET;
	return 0;
}

/* should define a struct to pass aead, cipher to this function */
int hisi_sec_crypto(struct wd_cipher_sess *sess, struct wd_cipher_arg *arg)
{
	struct hisi_sec_sess *priv;
	struct hisi_sec_sqe msg = {0};
	struct hisi_sec_sqe recv_msg = {0};
	int ret;

	priv = (struct hisi_sec_sess *)sess->priv;
	//fill hisi sec sqe;
	ret = hisi_cipher_create_request(sess, arg, &msg);
	if (ret) {
		WD_ERR("fill cipher bd2 failed!\n");
		return ret;
	}

	ret = hisi_qm_send(priv->qp.h_ctx, &msg);
	if (ret == -EBUSY) {
		usleep(1);
	}
	if (ret) {
		WD_ERR("send failed (%d)\n", ret);
		goto out;
	}
recv_again:
	ret = hisi_qm_recv(priv->qp.h_ctx, (void **)&recv_msg);
	if (ret == -EIO) {
		WD_ERR("wd recv msg failed!\n");
		goto out;
	} else if (ret == -EAGAIN)
		goto recv_again;

	return 0;
out:
	return ret;
}

int hisi_sec_encrypt(struct wd_cipher_sess *sess, struct wd_cipher_arg *arg)
{
	if (arg->op_type != WD_CIPHER_ENCRYPTION) {
		WD_ERR("wd encryption input op_type err.\n");
		return -EINVAL;
	}

	return hisi_sec_crypto(sess, arg);
}

int hisi_sec_decrypt(struct wd_cipher_sess *sess, struct wd_cipher_arg *arg)
{
	if (arg->op_type != WD_CIPHER_DECRYPTION) {
		WD_ERR("wd decryption input op_type err.\n");
		return -EINVAL;
	}

	return hisi_sec_crypto(sess, arg);
}

int hisi_cipher_init(struct wd_cipher_sess *sess)
{
	struct hisi_sec_sess *sec_sess;
	int ret = 0;

	sec_sess = calloc(1, sizeof(*sec_sess));
	if (!sec_sess)
		return -ENOMEM;

	sess->priv = sec_sess;
	sec_sess->node_path = strdup(sess->node_path);

	/* fix me: how to do with this? */
	ret = hisi_sec_init(sec_sess);
	if (ret < 0) {
		WD_ERR("hisi sec init failed\n");
		free(sec_sess);
		sess->priv = NULL;
	}

	return ret;
}

void hisi_cipher_exit(struct wd_cipher_sess *sess)
{
	struct hisi_sec_sess *sec_sess = sess->priv;

	hisi_sec_exit(sess->priv);

	free(sec_sess->node_path);
	free(sec_sess);
}

int hisi_cipher_prep(struct wd_cipher_sess *sess, struct wd_cipher_arg *arg)
{
	return 0;
}

void hisi_cipher_fini(struct wd_cipher_sess *sess)
{
}

int hisi_cipher_set_key(struct wd_cipher_sess *sess, const __u8 *key, __u32 key_len)
{
	return hisi_sec_set_key(sess->priv, key, key_len);
}

int hisi_cipher_encrypt(struct wd_cipher_sess *sess, struct wd_cipher_arg *arg)
{
	/* this function may be reused by aead, should change to proper inputs */
	return hisi_sec_encrypt(sess, arg);
}

int hisi_cipher_decrypt(struct wd_cipher_sess *sess, struct wd_cipher_arg *arg)
{
	/* this function may be reused by aead, should change to proper inputs */
	return hisi_sec_decrypt(sess, arg);
}

int hisi_cipher_poll(struct wd_cipher_sess *sess, struct wd_cipher_arg *arg)
{
	return 0;
}

int hisi_digest_init(struct wd_digest_sess *sess)
{
	struct hisi_sec_sess *sec_sess;
	int ret = 0;

	sec_sess = calloc(1, sizeof(*sec_sess));
	if (!sec_sess)
		return -ENOMEM;

	sess->priv = sec_sess;
	sec_sess->node_path = strdup(sess->node_path);

	ret = hisi_sec_init(sec_sess);
	if (ret < 0) {
		free(sec_sess);
		sess->priv = NULL;
	}

	return ret;
}

void hisi_digest_exit(struct wd_digest_sess *sess)
{
	struct hisi_sec_sess *sec_sess = sess->priv;

	hisi_sec_exit(sess->priv);

	free(sec_sess->node_path);
	free(sec_sess);
}

int hisi_digest_prep(struct wd_digest_sess *sess, struct wd_digest_arg *arg)
{
	return 0;
}

void hisi_digest_fini(struct wd_digest_sess *sess)
{
}

int hisi_digest_set_key(struct wd_digest_sess *sess, const __u8 *key, __u32 key_len)
{
	return hisi_sec_set_key(sess->priv, key, key_len);
}

static void qm_fill_digest_alg(struct wd_digest_sess *sess,
			       struct wd_digest_arg *arg,
			       struct hisi_sec_sqe *sqe)
{
	sqe->type2.mac_key_alg = (__u32)(arg->out_bytes / WORD_BYTES);

	if (arg->mode == WD_DIGEST_NORMAL)
		sqe->type2.mac_key_alg |= (__u32)(g_digest_a_alg[sess->alg]) <<
					SEC_DIGEST_ALG_OFFSET;
	else
		sqe->type2.mac_key_alg |= (__u32)(g_hmac_a_alg[sess->alg]) <<
					SEC_DIGEST_ALG_OFFSET;
}

static void hisi_digest_create_request(struct wd_digest_sess *sess,
				struct wd_digest_arg *arg,
				struct hisi_sec_sqe *sqe)
{
	struct hisi_sec_sess *sec_sess = sess->priv;
	__u8 de = 1;
	__u8 scene;

	/* config BD type */
	sqe->type_auth_cipher = BD_TYPE2;
	sqe->type_auth_cipher = AUTH_HMAC_CALCULATE << SEC_AUTH_OFFSET;
	/* config scence */
	scene = SEC_COMM_SCENE << SEC_SCENE_OFFSET;
	sqe->sds_sa_type = (de | scene);

	sqe->type2.alen_ivllen = (__u32)arg->in_bytes;
	sqe->type2.data_src_addr = (__u64)arg->in;
	sqe->type2.mac_addr = (__u64)arg->out;
        if (arg->mode == WD_DIGEST_HMAC) {
		/* config a key */
		sqe->type2.mac_key_alg |= (sec_sess->key_bytes / WORD_BYTES) << SEC_AUTH_KEY_OFFSET;
		sqe->type2.a_key_addr = (__u64)(sec_sess->key);
	}
	/* fix me */
	qm_fill_digest_alg(sess, arg, sqe);	
}

int hisi_digest_digest(struct wd_digest_sess *sess, struct wd_digest_arg *arg)
{
	struct hisi_sec_sess *sec_sess = sess->priv;
	struct hisi_sec_sqe sqe = {0};
	struct hisi_sec_sqe sqe_recv = {0};
	__u16 done, flag;
	__u8 type, etype;

	hisi_digest_create_request(sess, arg, &sqe);

	hisi_qm_send(sec_sess->qp.h_ctx, &sqe);

	type = sqe_recv.type_auth_cipher & SEC_TYPE_MASK;
	/* error handle */
	done = sqe_recv.type2.done_flag & SEC_DONE_MASK;
	flag = (sqe_recv.type2.done_flag & SEC_FLAG_MASK) >> SEC_FLAG_OFFSET;
	etype = sqe_recv.type2.error_type;

	/* fix me: how to handle parall, some place we need a lock */
	hisi_qm_recv(sec_sess->qp.h_ctx, (void **)&sqe_recv);
	if (type == BD_TYPE2) {
		if (done != SEC_HW_TASK_DONE || etype) {
			WD_ERR("Digest fail! done=0x%x, etype=0x%x\n", done, etype);
			return -EIO;
		}
	}

	return 0;
}

int hisi_digest_poll(struct wd_digest_sess *sess, struct wd_digest_arg *arg)
{
	return 0;
}
