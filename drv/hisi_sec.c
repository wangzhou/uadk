/* SPDX-License-Identifier: Apache-2.0 */
#include <stdbool.h>
#include <pthread.h>
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

/* fix me */
#define SEC_QP_NUM_PER_PROCESS	  1
#define MAX_CIPHER_RETRY_CNT	  20000000
/* should be remove to qm module */
struct hisi_qp_req {
	void (*callback)(void *parm);
};

struct hisi_qp_task_pool {
	pthread_mutex_t task_pool_lock;
	struct hisi_qp_req *queue;
	__u32 tail;
	__u32 head;
	__u32 depth;
};

struct hisi_qp_async {
	struct hisi_qp_task_pool task_pool;
	struct hisi_qp *qp;
};

static struct hisi_qp_async *hisi_qm_alloc_qp_async(char *node_path)
{
	return NULL;
}

static void hisi_qm_free_qp_async(struct hisi_qp_async *qp)
{
}

static int hisi_qm_send_async(struct hisi_qp_async *qp, void *req,
			      void (*callback)(void *parm))
{
	/* store req callback in task pool */

	/* send request */

	return 0;
}

static void hisi_qm_poll_async_qp(struct hisi_qp_async *qp, __u32 num)
{
	/* hisi_qm_recv */

	/* find related task in task pool and call its cb */
}
/* end qm demo */

/* session like request ctx */
struct hisi_sec_sess {
	struct hisi_qp *qp;
	struct hisi_qp_async *qp_async;
	char *node_path;
};

struct hisi_qp_async_list {
	struct hisi_qp_async *qp;
	struct hisi_qp_async_list *next;
};

struct hisi_sec_qp_async_pool {
	pthread_mutex_t lock;
	struct hisi_qp_async_list head;
} hisi_sec_qp_async_pool;

static int get_qp_num_in_pool(void)
{
	return 0;
}
 
static void hisi_sec_add_qp_to_pool(struct hisi_sec_qp_async_pool *pool,
				    struct hisi_qp_async *qp)
{
}

static struct hisi_qp *get_qp_in_poll(void)
{
	return NULL;
}

static int hisi_sec_alloc_qps(struct hisi_sec_sess *sec_sess, int num)
{
	return 0;
}

int hisi_sec_init(struct hisi_sec_sess *sec_sess)
{
	struct hisi_qp *qp = sec_sess->qp;
	struct hisi_qp_async *qp_async;
	handle_t h_ctx;
	int num;
	/* fix me: should have a flag in sec_sess to indicate sync or async qp */
	int qp_type = 0;

	num = get_qp_num_in_pool();

	if (num < SEC_QP_NUM_PER_PROCESS) {
		if (!qp_type) {
			h_ctx = hisi_qm_alloc_ctx(sec_sess->node_path, qp);
			if (!h_ctx)
				return -EINVAL;
		} else {
			qp_async = hisi_qm_alloc_qp_async(sec_sess->node_path);
			if (!qp_async)
				return -EINVAL;

			hisi_sec_add_qp_to_pool(&hisi_sec_qp_async_pool,
						qp_async);
		}

	} else {
		sec_sess->qp = get_qp_in_poll();
	}

	return 0;
}

void hisi_sec_exit(struct hisi_sec_sess *sec_sess)
{
	/* wd_ctx_stop */
	wd_ctx_stop(sec_sess->qp->h_ctx);

	/* free alloc_qp_ctx */
	hisi_qm_free_ctx(sec_sess->qp->h_ctx);
}

int hisi_sec_set_key(struct wd_cipher_sess *sess, const __u8 *key, __u32 key_len)
{
	/* store key to sess */
	memcpy(sess->key, key, key_len);
	sess->key_bytes = key_len;

	return 0;
}

static int get_aes_c_key_len(struct wd_cipher_sess *sess, __u8 *c_key_len)
{
	__u16 len;

	len = sess->key_bytes;

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

	if (sess->key_bytes == SEC_3DES_2KEY_SIZE) {
		*c_key_len = CKEY_LEN_3DES_2KEY;
	} else if (sess->key_bytes == SEC_3DES_3KEY_SIZE) {
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
	sqe->type2.c_key_addr = (__u64)sess->key;
	return 0;
}

/* should define a struct to pass aead, cipher to this function */
int hisi_sec_crypto(struct wd_cipher_sess *sess, struct wd_cipher_arg *arg)
{
	struct hisi_sec_sess *priv;
	struct hisi_sec_sqe msg = {0};
	struct hisi_sec_sqe recv_msg = {0};
	__u64 recv_count = 0;
	int ret;

	priv = (struct hisi_sec_sess *)sess->priv;
	//fill hisi sec sqe;
	ret = hisi_cipher_create_request(sess, arg, &msg);
	if (ret) {
		WD_ERR("fill cipher bd2 failed!\n");
		return ret;
	}

	ret = hisi_qm_send(priv->qp->h_ctx, &msg);
	if (ret == -EBUSY) {
		usleep(1);
	}
	if (ret) {
		WD_ERR("send failed (%d)\n", ret);
		return ret;
	}
	while (true) {
		ret = hisi_qm_recv(priv->qp->h_ctx, (void **)&recv_msg);
		if (ret == -EIO) {
			WD_ERR("wd recv msg failed!\n");
			break;
		} else if (ret == -EAGAIN) {
			if (++recv_count > MAX_CIPHER_RETRY_CNT) {
				WD_ERR("%s:sec recv timeout fail!\n", __func__);
				ret = -ETIMEDOUT;
				break;
			}
		} else {
			// recive output_msg;
			break;
		}
	}

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

int hisi_cipher_poll(struct wd_cipher_sess *sess, __u32 count)
{
	struct hisi_qp_async *qp;

	/* iterate async qp in hisi_sec_qp_async_pool */

	hisi_qm_poll_async_qp(qp, count);

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
		sqe->type2.mac_key_alg |= (sess->key_bytes / WORD_BYTES) << SEC_AUTH_KEY_OFFSET;
		sqe->type2.a_key_addr = (__u64)(sess->key);
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

	hisi_qm_send(sec_sess->qp->h_ctx, &sqe);

	type = sqe_recv.type_auth_cipher & SEC_TYPE_MASK;
	/* error handle */
	done = sqe_recv.type2.done_flag & SEC_DONE_MASK;
	flag = (sqe_recv.type2.done_flag & SEC_FLAG_MASK) >> SEC_FLAG_OFFSET;
	etype = sqe_recv.type2.error_type;

	/* fix me: how to handle parall, some place we need a lock */
	hisi_qm_recv(sec_sess->qp->h_ctx, (void **)&sqe_recv);
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
