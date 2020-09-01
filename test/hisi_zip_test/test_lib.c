#include <signal.h>
#include <sys/mman.h>

#include "hisi_qm_udrv.h"
#include "sched_sample.h"
#include "test_lib.h"

#define HISI_DEV_NODE	"/dev/hisi_zip-0"

#define SCHED_RR_NAME	"sched_rr"

enum alg_type {
	HW_ZLIB  = 0x02,
	HW_GZIP,
};

/* fix me: just move it here to pass compile. rely on hardware directly is very bad!! */
struct hisi_zip_sqe {
	__u32 consumed;
	__u32 produced;
	__u32 comp_data_length;
	__u32 dw3;
	__u32 input_data_length;
	__u32 lba_l;
	__u32 lba_h;
	__u32 dw7;
	__u32 dw8;
	__u32 dw9;
	__u32 dw10;
	__u32 priv_info;
	__u32 dw12;
	__u32 tag;
	__u32 dest_avail_out;
	__u32 ctx_dw0;
	__u32 comp_head_addr_l;
	__u32 comp_head_addr_h;
	__u32 source_addr_l;
	__u32 source_addr_h;
	__u32 dest_addr_l;
	__u32 dest_addr_h;
	__u32 stream_ctx_addr_l;
	__u32 stream_ctx_addr_h;
	__u32 cipher_key1_addr_l;
	__u32 cipher_key1_addr_h;
	__u32 cipher_key2_addr_l;
	__u32 cipher_key2_addr_h;
	__u32 ctx_dw1;
	__u32 ctx_dw2;
	__u32 isize;
	__u32 checksum;

};

struct check_rand_ctx {
	int off;
	unsigned long global_off;
	__u32 last;
	unsigned short state[3];
};

#ifdef DEBUG_LOG
static void dbg_sqe(const char *head, struct hisi_zip_sqe *m)
{
	fprintf(stderr,
		"=== %s ===\n"
		"cons=0x%x prod=0x%x in=0x%x out=0x%x comp=0x%x\n"
		"lba=0x%08x 0x%08x\n"
		"dw3=0x%x dw7=0x%x dw8=0x%x dw9=0x%x dw10=0x%x dw12=0x%x\n"
		"priv=0x%x tag=0x%x\n"
		"ctx dw0=0x%x dw1=0x%x dw2=0x%x\n"
		"comp head addr=0x%08x 0x%08x\n"
		"source addr=0x%08x 0x%08x\n"
		"dest addr=0x%08x 0x%08x\n"
		"stream ctx addr=0x%08x 0x%08x\n"
		"cipher key1 addr=0x%08x 0x%08x\n"
		"cipher key2 addr=0x%08x 0x%08x\n"
		"isize=0x%x checksum=0x%x\n"
		"=== === === ===\n",

		head,
		m->consumed, m->produced, m->input_data_length,
		m->dest_avail_out, m->comp_data_length,
		m->lba_h, m->lba_l,
		m->dw3, m->dw7, m->dw8, m->dw9, m->dw10, m->dw12,
		m->priv_info, m->tag,
		m->ctx_dw0, m->ctx_dw1, m->ctx_dw2,
		m->comp_head_addr_h, m->comp_head_addr_l,
		m->source_addr_h, m->source_addr_l,
		m->dest_addr_h, m->dest_addr_l,
		m->stream_ctx_addr_h, m->stream_ctx_addr_l,
		m->cipher_key1_addr_h, m->cipher_key1_addr_l,
		m->cipher_key2_addr_h, m->cipher_key2_addr_l,
		m->isize, m->checksum
	       );
}
#else
#define dbg_sqe(...)
#endif

void *mmap_alloc(size_t len)
{
	void *p;
	long page_size = sysconf(_SC_PAGESIZE);

	if (len % page_size) {
		WD_ERR("unaligned allocation must use malloc\n");
		return NULL;
	}

	p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
		 -1, 0);
	if (p == MAP_FAILED)
		WD_ERR("Failed to allocate %zu bytes\n", len);

	return p == MAP_FAILED ? NULL : p;
}

static int hizip_check_rand(unsigned char *buf, unsigned int size, void *opaque)
{
	int i;
	int *j;
	__u32 n;
	struct check_rand_ctx *rand_ctx = opaque;

	j = &rand_ctx->off;
	for (i = 0; i < size; i += 4) {
		if (*j) {
			/* Somthing left from a previous run */
			n = rand_ctx->last;
		} else {
			n = nrand48(rand_ctx->state);
			rand_ctx->last = n;
		}
		for (; *j < 4 && i + *j < size; (*j)++) {
			char expected = (n >> (8 * *j)) & 0xff;
			char actual = buf[i + *j];

			if (expected != actual) {
				WD_ERR("Invalid decompressed char at offset %lu: expected 0x%x != 0x%x\n",
				       rand_ctx->global_off + i + *j, expected,
				       actual);
				return -EINVAL;
			}
		}
		if (*j == 4)
			*j = 0;
	}
	rand_ctx->global_off += size;
	return 0;
}

#ifdef SWITCH_NEW_INTERFACE
void hizip_prepare_random_input_data(struct hizip_test_info *info)
{
	__u32 seed = 0;
	unsigned short rand_state[3] = {(seed >> 16) & 0xffff, seed & 0xffff, 0x330e};

	unsigned long remain_size;
	__u32 block_size, size;
	char *in_buf;
	size_t i, j;

	/*
	 * TODO: change state for each buffer, to make sure there is no TLB
	 * aliasing. Can we store the seed into priv_info?
	 */
	//__u32 seed = info->state++;
	block_size = info->opts->block_size;
	remain_size = info->total_len;
	in_buf = info->in_buf;

	while (remain_size > 0) {
		if (remain_size > block_size)
			size = block_size;
		else
			size = remain_size;
		/*
		 * Prepare the input buffer with a reproducible sequence of
		 * numbers. nrand48() returns a pseudo-random number in the
		 * interval [0; 2^31). It's not really possible to compress a
		 * pseudo-random stream using deflate, since it can't find any
		 * string repetition. As a result the output size is bigger,
		 * with a ratio of 1.041.
		 */
		for (i = 0; i < size; i += 4) {
			__u64 n = nrand48(rand_state);

			for (j = 0; j < 4 && i + j < size; j++)
				in_buf[i + j] = (n >> (8 * j)) & 0xff;
		}

		in_buf += size;
		remain_size -= size;
	}
}

int hizip_verify_random_output(char *out_buf, struct test_options *opts,
			       struct hizip_test_info *info)
{
	int ret;
	int seed = 0;
	off_t off = 0;
	size_t checked = 0;
	size_t total_checked = 0;
	struct check_rand_ctx rand_ctx = {
		.state = {(seed >> 16) & 0xffff, seed & 0xffff, 0x330e},
	};

	if (!opts->verify)
		return 0;

	do {
		ret = hizip_check_output(out_buf + off, info->total_out,
					 &checked, hizip_check_rand, &rand_ctx);
fprintf(stderr, "#%s, %d, ret:%d\n", __func__, __LINE__, ret);
		if (ret) {
			WD_ERR("Check output failed with %d\n", ret);
			return ret;
		}
		total_checked += checked;
		off += opts->block_size * EXPANSION_RATIO;
	} while (!ret && total_checked < opts->total_len);

fprintf(stderr, "#%s, %d, global_off:%d, total_len:%d\n", __func__, __LINE__, rand_ctx.global_off, opts->total_len);
	if (rand_ctx.global_off != opts->total_len) {
		WD_ERR("Invalid output size %lu != %lu\n",
		       rand_ctx.global_off, opts->total_len);
		return -EINVAL;
	}
	return 0;
}
#else
void hizip_prepare_random_input_data(struct hizip_test_context *ctx)
{
	__u32 seed = 0;
	unsigned short rand_state[3] = {(seed >> 16) & 0xffff, seed & 0xffff, 0x330e};

	unsigned long remain_size;
	__u32 block_size, size;
	char *in_buf;
	size_t i, j;

	/*
	 * TODO: change state for each buffer, to make sure there is no TLB
	 * aliasing. Can we store the seed into priv_info?
	 */
	//__u32 seed = ctx->state++;
	block_size = ctx->opts->block_size;
	remain_size = ctx->total_len;
	in_buf = ctx->in_buf;

	while (remain_size > 0) {
		if (remain_size > block_size)
			size = block_size;
		else
			size = remain_size;
		/*
		 * Prepare the input buffer with a reproducible sequence of
		 * numbers. nrand48() returns a pseudo-random number in the
		 * interval [0; 2^31). It's not really possible to compress a
		 * pseudo-random stream using deflate, since it can't find any
		 * string repetition. As a result the output size is bigger,
		 * with a ratio of 1.041.
		 */
		for (i = 0; i < size; i += 4) {
			__u64 n = nrand48(rand_state);

			for (j = 0; j < 4 && i + j < size; j++)
				in_buf[i + j] = (n >> (8 * j)) & 0xff;
		}

		in_buf += size;
		remain_size -= size;
	}
}

int hizip_verify_random_output(char *out_buf, struct test_options *opts,
			       struct hizip_test_context *ctx)
{
	int ret;
	int seed = 0;
	off_t off = 0;
	size_t checked = 0;
	size_t total_checked = 0;
	struct check_rand_ctx rand_ctx = {
		.state = {(seed >> 16) & 0xffff, seed & 0xffff, 0x330e},
	};

	if (!opts->verify)
		return 0;

	do {
		ret = hizip_check_output(out_buf + off, ctx->total_out,
					 &checked, hizip_check_rand, &rand_ctx);
		if (ret) {
			WD_ERR("Check output failed with %d\n", ret);
			return ret;
		}
		total_checked += checked;
		off += opts->block_size * EXPANSION_RATIO;
	} while (!ret && total_checked < opts->total_len);

	if (rand_ctx.global_off != opts->total_len) {
		WD_ERR("Invalid output size %lu != %lu\n",
		       rand_ctx.global_off, opts->total_len);
		return -EINVAL;
	}
	return 0;
}
#endif

#ifdef SWITCH_NEW_INTERFACE

//#define HISI_DEV_NODE	"/dev/hisi_zip-0"

/* TODO: how to pick a context */
/*
static handle_t sched_multi_pick_next(struct wd_ctx_config *cfg,
				      void *req,
				      void *key
				      )
{
	return cfg->ctxs[0].ctx;
}

static int sched_multi_poll_policy(struct wd_ctx_config *cfg)
{
	int ret;
	__u32 count = 0;

	ret = wd_comp_poll_ctx(cfg->ctxs[0].ctx, 1, &count);
	if (ret < 0)
		return ret;
	return count;
}
*/

int lib_poll_func(handle_t h_ctx, __u32 expect, __u32 *count)
{
	return SCHED_SUCCESS;
}

int init_ctx_config(struct test_options *opts,
		    struct wd_sched *sched,
		    void *priv
		    )
{
	struct wd_comp_sess_setup setup;
	struct uacce_dev_list *list;
	struct hizip_test_info *info = priv;
	struct wd_ctx_config *ctx_conf = &info->ctx_conf;
	int i, j, ret = -EINVAL;

	list = wd_get_accel_list("zlib");
	if (!list)
		return -ENODEV;
	info->list = list;

	memset(ctx_conf, 0, sizeof(struct wd_ctx_config));
	ctx_conf->ctx_num = opts->q_num;
	ctx_conf->ctxs = calloc(1, opts->q_num * sizeof(struct wd_ctx));
	if (!ctx_conf->ctxs) {
		WD_ERR("Not enough memory to allocate contexts.\n");
		ret = -ENOMEM;
		goto out;
	}
	for (i = 0; i < ctx_conf->ctx_num; i++) {
		ctx_conf->ctxs[i].ctx = wd_request_ctx(list->dev);
fprintf(stderr, "#%s, %d, ctx[%d]:0x%x\n", __func__, __LINE__, i, ctx_conf->ctxs[i].ctx);
		if (!ctx_conf->ctxs[i].ctx) {
			WD_ERR("Fail to allocate context #%d\n", i);
			ret = -EINVAL;
			goto out_ctx;
		}
		ctx_conf->ctxs[i].op_type = opts->op_type;
		ctx_conf->ctxs[i].ctx_mode = opts->sync_mode;
	}
	sched->name = SCHED_RR_NAME;
	sched->pick_next_ctx = sample_sched_pick_next_ctx;
	sched->poll_policy = sample_sched_poll_policy;
	ret = sample_sched_init(SCHED_POLICY_RR, 2, lib_poll_func);
	if (ret < 0) {
		WD_ERR("Fail to init scheduler.\n");
		ret = -EINVAL;
		goto out_sched;
	}
	/* 40 contexts for 4 types */
	ret = sample_sched_fill_region(0, 0, 0, 0, 19);
	if (ret < 0) {
		WD_ERR("Fail to fill sched region.\n");
		ret = -EINVAL;
		goto out_fill;
	}
	ret = sample_sched_fill_region(0, 0, 1, 20, 39);
	if (ret < 0) {
		WD_ERR("Fail to fill sched region.\n");
		ret = -EINVAL;
		goto out_fill;
	}
	ret = sample_sched_fill_region(0, 1, 0, 40, 59);
	if (ret < 0) {
		WD_ERR("Fail to fill sched region.\n");
		ret = -EINVAL;
		goto out_fill;
	}
	ret = sample_sched_fill_region(0, 1, 1, 60, 79);
	if (ret < 0) {
		WD_ERR("Fail to fill sched region.\n");
		ret = -EINVAL;
		goto out_fill;
	}
	wd_comp_init(ctx_conf, sched);

	/* allocate a wd_comp session */
	memset(&setup, 0, sizeof(struct wd_comp_sess_setup));
	if (opts->alg_type == ZLIB)
		setup.alg_type = WD_ZLIB;
	else if (opts->alg_type == GZIP)
		setup.alg_type = WD_GZIP;
	setup.mode = opts->sync_mode;
	setup.op_type = opts->op_type;
	info->h_sess = wd_comp_alloc_sess(&setup);
	if (!info->h_sess) {
		ret = -EINVAL;
		goto out_sess;
	}
	return ret;
out_sess:
	wd_comp_uninit();
out_fill:
	sample_sched_release();
out_sched:
	i = ctx_conf->ctx_num;
out_ctx:
	for (j = 0; j < i; j++)
		wd_release_ctx(ctx_conf->ctxs[j].ctx);
	free(ctx_conf->ctxs);
out:
	wd_free_list_accels(list);
	return ret;
}

void uninit_config(void *priv)
{
	struct hizip_test_info *info = priv;
	struct wd_ctx_config *ctx_conf = &info->ctx_conf;
	int i;

	wd_comp_free_sess(info->h_sess);
	wd_comp_uninit();
	sample_sched_release();
	for (i = 0; i < ctx_conf->ctx_num; i++)
		wd_release_ctx(ctx_conf->ctxs[i].ctx);
	free(ctx_conf->ctxs);
	wd_free_list_accels(info->list);
}

int hizip_test_sched(struct wd_sched *sched,
		     struct test_options *opts,
		     struct hizip_test_info *info
		     )
{
	handle_t h_sess = info->h_sess;
	handle_t h_ctx;
	struct sched_key key;
	int ret;

	key.numa_id = 0;
	key.mode = opts->sync_mode;
	key.type = 0;
/*
fprintf(stderr, "#%s, %d, cfg:0x%x\n", __func__, __LINE__, &info->ctx_conf);
	h_ctx = sched->pick_next_ctx(&info->ctx_conf, "test", &key);
fprintf(stderr, "#%s, %d, h_ctx:0x%x\n", __func__, __LINE__, h_ctx);
	if (!h_ctx) {
		WD_ERR("fail to get next ctx\n");
		return -ENOENT;
	}
*/
	if (opts->sync_mode) {
		/* async */
	} else {
fprintf(stderr, "#%s, %d, comp sync, req src_len:%d, req dst_len:%d\n", __func__, __LINE__, info->req.src_len, info->req.dst_len);
		ret = wd_do_comp_sync(h_sess, &info->req);
fprintf(stderr, "#%s, %d, ret:%d, req src_len:%d, req dst_len:%d\n", __func__, __LINE__, ret, info->req.src_len, info->req.dst_len);
		if (ret < 0)
			return ret;
	}
	info->total_out = info->req.dst_len;
	return 0;
}
#else
struct test_ops default_test_ops = {
	.init_cache = hizip_test_default_init_cache,
	.input = hizip_test_default_input,
	.output = hizip_test_default_output,
};

void hizip_test_default_init_cache(struct wd_scheduler *sched, int i,
				   void *priv)
{
	struct wd_msg *wd_msg = &sched->msgs[i];
	struct hizip_test_context *ctx = priv;
	struct hisi_zip_sqe *msg;

	wd_msg->msg = &ctx->msgs[i];
	msg = &ctx->msgs[i];

	if (ctx->opts->alg_type == ZLIB)
		msg->dw9 = HW_ZLIB;
	else
		msg->dw9 = HW_GZIP;
	msg->dest_avail_out = sched->msg_data_size;
}

int hizip_test_default_input(struct wd_msg *msg, void *priv)
{
	size_t ilen;
	char *in_buf, *out_buf;
	struct hisi_zip_sqe *m = msg->msg;
	struct hizip_test_context *ctx = priv;
	struct test_options *opts = ctx->opts;

	ilen = ctx->total_len > opts->block_size ?
		opts->block_size : ctx->total_len;

	in_buf = ctx->in_buf;
	out_buf = ctx->out_buf;

	m->source_addr_l = (__u64)in_buf & 0xffffffff;
	m->source_addr_h = (__u64)in_buf >> 32;
	m->dest_addr_l = (__u64)out_buf & 0xffffffff;
	m->dest_addr_h = (__u64)out_buf >> 32;

	ctx->out_buf += ilen * EXPANSION_RATIO;

	m->input_data_length = ilen;
	ctx->in_buf += ilen;
	ctx->total_len -= ilen;

	dbg_sqe("zip input", m);

	return 0;
}

int hizip_test_default_output(struct wd_msg *msg, void *priv)
{
	struct hizip_test_context *ctx = priv;
	struct hisi_zip_sqe *m = msg->msg;
	__u32 status = m->dw3 & 0xff;
	__u32 type = m->dw9 & 0xff;

	dbg_sqe("zip output", m);

	if (status != 0 && status != 0x0d) {
		fprintf(stderr, "bad status (s=%d, t=%d%s)\n", status, type,
			ctx->faulting ? ", expected" : "");
		return -EFAULT;
	}

	ctx->total_out += m->produced;

	return 0;
}

/*
 * Initialize the scheduler with the given options and operations.
 */
int hizip_test_init(struct wd_scheduler *sched, struct test_options *opts,
		    struct test_ops *ops, void *priv)
{
	int i, j, ret = -ENOMEM;
	struct hisi_qm_priv *qm_priv;
	struct hisi_qm_capa *capa;
	struct hizip_test_context *ctx = priv;

	sched->q_num = opts->q_num;
	sched->ss_region_size = 0; /* let system make decision */
	sched->msg_cache_num = opts->req_cache_num;
	/* use twice the size of the input data, hope it is enough for output */
	sched->msg_data_size = opts->block_size * EXPANSION_RATIO;

	sched->priv = priv;
	sched->init_cache = ops->init_cache;
	sched->input = ops->input;
	sched->output = ops->output;
	sched->hw_alloc = hisi_qm_alloc_ctx;
	sched->hw_free = hisi_qm_free_ctx;
	sched->hw_send = hisi_qm_send;
	sched->hw_recv = hisi_qm_recv;

	sched->qs = calloc(opts->q_num, sizeof(*sched->qs));
	if (!sched->qs)
		return -ENOMEM;

	capa = &ctx->capa;
	qm_priv = (struct hisi_qm_priv *)&capa->priv;
	qm_priv->sqe_size = sizeof(struct hisi_zip_sqe);
	qm_priv->op_type = opts->op_type;

	if (opts->alg_type == ZLIB)
		capa->alg = "zlib";
	else
		capa->alg = "gzip";
	sched->data = ctx->qp;

	ctx->msgs = calloc(1, sizeof(*ctx->msgs) * sched->msg_cache_num);
	if (!ctx->msgs)
		goto out_msgs;

	ret = wd_sched_init(sched, HISI_DEV_NODE);
	if (ret)
		goto out_sched;

	for (i = 0; i < sched->q_num; i++) {
		sched->qs[i] = sched->hw_alloc(HISI_DEV_NODE,
					       qm_priv,
					       &sched->data);
		if (!sched->qs[i])
			goto out_hw;
	}

	return 0;

out_hw:
	for (j = i - 1; j >= 0; j--) {
		sched->hw_free(sched->qs[j]);
	}
out_sched:
	free(ctx->msgs);
out_msgs:
	free(sched->qs);
	return ret;
}

int hizip_test_sched(struct wd_scheduler *sched, struct test_options *opts,
		     struct hizip_test_context *ctx)
{
	int ret = 0;

	while (ctx->total_len || !wd_sched_empty(sched)) {
		dbg("request loop: total_len=%d\n", ctx->total_len);
		ret = wd_sched_work(sched, ctx->total_len);
		if (ret < 0) {
			WD_ERR("wd_sched_work: %d\n", ret);
			break;
		}
	}

	return (ret > 0) ? 0 : ret;
}

/*
 * Release the scheduler
 */
void hizip_test_fini(struct wd_scheduler *sched, struct test_options *opts)
{
	int i;

	for (i = 0; i < sched->q_num; i++)
		sched->hw_free(sched->qs[i]);
	wd_sched_fini(sched);
	free(sched->qs);
}
#endif

#ifdef SWITCH_NEW_INTERFACE
int parse_common_option(const char opt, const char *optarg,
			struct test_options *opts)
{
	switch (opt) {
	case 'b':
		opts->block_size = strtol(optarg, NULL, 0);
		if (opts->block_size <= 0)
			return 1;
		break;
	case 'l':
		opts->compact_run_num = strtol(optarg, NULL, 0);
		if (opts->compact_run_num <= 0)
			return 1;
		break;
	case 'n':
		opts->run_num = strtol(optarg, NULL, 0);
		SYS_ERR_COND(opts->run_num > MAX_RUNS,
			     "No more than %d runs supported\n", MAX_RUNS);
		if (opts->run_num <= 0)
			return 1;
		break;
	case 'q':
		opts->q_num = strtol(optarg, NULL, 0);
		if (opts->q_num <= 0)
			return 1;
		break;
	case 's':
		opts->total_len = strtol(optarg, NULL, 0);
		SYS_ERR_COND(opts->total_len <= 0, "invalid size '%s'\n",
			     optarg);
		break;
	case 't':
		opts->thread_num = strtol(optarg, NULL, 0);
		SYS_ERR_COND(opts->total_len < 0, "invalid thread num '%s'\n",
			     optarg);
		break;
	case 'm':
		opts->sync_mode = strtol(optarg, NULL, 0);
		SYS_ERR_COND(opts->total_len < 0, "invalid sync mode '%s'\n",
			     optarg);
		break;
	case 'V':
		opts->verify = true;
		break;
	case 'v':
		opts->verbose = true;
		break;
	case 'z':
		opts->alg_type = ZLIB;
		break;
	default:
		return 1;
	}

	return 0;
}
#else
int parse_common_option(const char opt, const char *optarg,
			struct test_options *opts)
{
	switch (opt) {
	case 'b':
		opts->block_size = strtol(optarg, NULL, 0);
		if (opts->block_size <= 0)
			return 1;
		break;
	case 'c':
		opts->req_cache_num = strtol(optarg, NULL, 0);
		if (opts->req_cache_num <= 0)
			return 1;
		break;
	case 'l':
		opts->compact_run_num = strtol(optarg, NULL, 0);
		if (opts->compact_run_num <= 0)
			return 1;
		break;
	case 'n':
		opts->run_num = strtol(optarg, NULL, 0);
		SYS_ERR_COND(opts->run_num > MAX_RUNS,
			     "No more than %d runs supported\n", MAX_RUNS);
		if (opts->run_num <= 0)
			return 1;
		break;
	case 'q':
		opts->q_num = strtol(optarg, NULL, 0);
		if (opts->q_num <= 0)
			return 1;
		break;
	case 's':
		opts->total_len = strtol(optarg, NULL, 0);
		SYS_ERR_COND(opts->total_len <= 0, "invalid size '%s'\n",
			     optarg);
		break;
	case 't':
		opts->thread_num = strtol(optarg, NULL, 0);
		SYS_ERR_COND(opts->total_len < 0, "invalid thread num '%s'\n",
			     optarg);
		break;
	case 'm':
		opts->sync_mode = strtol(optarg, NULL, 0);
		SYS_ERR_COND(opts->total_len < 0, "invalid sync mode '%s'\n",
			     optarg);
		break;
	case 'V':
		opts->verify = true;
		break;
	case 'v':
		opts->verbose = true;
		break;
	case 'z':
		opts->alg_type = ZLIB;
		break;
	default:
		return 1;
	}

	return 0;
}
#endif
