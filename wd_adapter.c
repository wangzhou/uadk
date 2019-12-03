/* SPDX-License-Identifier: GPL-2.0 */
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>


#include "wd_adapter.h"
#include "./drv/dummy_drv.h"
#include "./drv/hisi_qm_udrv.h"
#include "./drv/wd_drv.h"

static struct wd_drv_dio_if hw_dio_tbl[] = { {
		.hw_type = "dummy_v1",
		.open = dummy_set_queue_dio,
		.close = dummy_unset_queue_dio,
		.send = dummy_add_to_dio_q,
		.recv = dummy_get_from_dio_q,
		.flush = dummy_flush,
	}, {
		.hw_type = "dummy_v2",
		.open = dummy_set_queue_dio,
		.close = dummy_unset_queue_dio,
		.send = dummy_add_to_dio_q,
		.recv = dummy_get_from_dio_q,
		.flush = dummy_flush,
	}, {
		.hw_type = HISI_QM_API_VER_BASE UACCE_API_VER_NOIOMMU_SUBFIX,
		.open = hisi_qm_set_queue_dio,
		.close = hisi_qm_unset_queue_dio,
		.send = hisi_qm_add_to_dio_q,
		.recv = hisi_qm_get_from_dio_q,
	}, {
		.hw_type = HISI_QM_API_VER2_BASE UACCE_API_VER_NOIOMMU_SUBFIX,
		.open = hisi_qm_set_queue_dio,
		.close = hisi_qm_unset_queue_dio,
		.send = hisi_qm_add_to_dio_q,
		.recv = hisi_qm_get_from_dio_q,
	}, {
		.hw_type = HISI_QM_API_VER_BASE,
		.open = hisi_qm_set_queue_dio,
		.close = hisi_qm_unset_queue_dio,
		.send = hisi_qm_add_to_dio_q,
		.recv = hisi_qm_get_from_dio_q,
	}, {
		.hw_type = HISI_QM_API_VER2_BASE,
		.open = hisi_qm_set_queue_dio,
		.close = hisi_qm_unset_queue_dio,
		.send = hisi_qm_add_to_dio_q,
		.recv = hisi_qm_get_from_dio_q,
	},
};

/* todo: there should be some stable way to match the device and the driver */
#define MAX_HW_TYPE (sizeof(hw_dio_tbl) / sizeof(hw_dio_tbl[0]))

int drv_open(struct wd_queue *q)
{
	int i;

	//todo: try to find another dev if the user driver is not available
	for (i = 0; i < MAX_HW_TYPE; i++) {
		if (!strcmp(q->hw_type,
			hw_dio_tbl[i].hw_type)) {
			q->hw_type_id = i;
			return hw_dio_tbl[q->hw_type_id].open(q);
		}
	}
	WD_ERR("No matched driver to use (%s)!\n", q->hw_type);
	errno = ENODEV;
	return -ENODEV;
}

void drv_close(struct wd_queue *q)
{
	hw_dio_tbl[q->hw_type_id].close(q);
}

int drv_send(struct wd_queue *q, void *req)
{
	return hw_dio_tbl[q->hw_type_id].send(q, req);
}

int drv_recv(struct wd_queue *q, void **req)
{
	return hw_dio_tbl[q->hw_type_id].recv(q, req);
}

void drv_flush(struct wd_queue *q)
{
	if (hw_dio_tbl[q->hw_type_id].flush)
		hw_dio_tbl[q->hw_type_id].flush(q);
}

void *drv_unreserve_mem(struct wd_queue *q)
{
	struct uacce_dma_buf buf;

	buf.fd = q->info.buffd;
	ioctl(q->fd, UACCE_CMD_DMA_BUF_DETACH, &buf);

	ion_close_buffer_fd(&q->info);
}

void *drv_reserve_mem(struct wd_queue *q, size_t size)
{
	struct uacce_dma_buf buf;
	int ret;
#if 0
	q->ss_va = wd_drv_mmap_qfr(q, UACCE_QFRT_SS, size);

	if (q->ss_va == MAP_FAILED) {
		WD_ERR("wd drv mmap fail!\n");
		return NULL;
	}
#endif
	fprintf(stderr, "gzf drv_reserve_mem q=0x%x\n", q);

	q->info.heap_type = ION_HEAP_TYPE_SYSTEM_CONTIG;
//	q->info.heap_type = ION_HEAP_TYPE_SYSTEM;
	q->info.heap_size = size;

	ret = ion_export_buffer_fd(&q->info);
	if (ret < 0) {
		fprintf(stderr, "FAILED: ion_get_buffer_fd\n");
		goto err_export;
	}
	q->ss_va = q->info.buffer;

	//if (q->dev_flags & UACCE_DEV_NOIOMMU) {
	if (1) {
		buf.fd = q->info.buffd;
		buf.size = q->info.buflen;
		errno = (long)ioctl(q->fd, UACCE_CMD_DMA_BUF_ATTACH, &buf);
		if (errno) {
			WD_ERR("get PA fail!\n");
			return NULL;
		}
		q->ss_pa = buf.ptr;
		fprintf(stderr, "gzf drv_reserve_mem q->ss_va=0x%x, q->ss_pa=0x%x\n", q->ss_va, q->ss_pa);
	} else
		q->ss_pa = q->ss_va;

	return q->ss_va;

err_export:
	ion_close_buffer_fd(&q->info);
}
