/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _UAPIUUACCE_H
#define _UAPIUUACCE_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define UACCE_CLASS_NAME	"uacce"
#define UACCE_DEV_ATTRS		"attrs"
#define UACCE_CMD_SHARE_SVAS	_IO('W', 0)
#define UACCE_CMD_START		_IO('W', 1)
#define UACCE_CMD_GET_SS_DMA	_IOR('W', 2, unsigned long)

/**
 * UACCE Device Attributes:
 *
 * PASID: the device has IOMMU which support PASID setting
 *	can do share sva, mapped to dev per process
 * FAULT_FROM_DEV: the device has IOMMU which can do page fault request
 *	no need for share sva, should be used with PASID
 * SVA: full function device
 * SHARE_DOMAIN: no PASID, can do share sva only for one process and the kernel
 */
#define UACCE_DEV_PASID			(1 << 0)
#define UACCE_DEV_FAULT_FROM_DEV	(1 << 1)
#define UACCE_DEV_SVA		(UACCE_DEV_PASID | UACCE_DEV_FAULT_FROM_DEV)
#define UACCE_DEV_SHARE_DOMAIN	(0)

#define UACCE_QFR_NA ((unsigned long)-1)
enum uacce_qfrt {
	UACCE_QFRT_MMIO = 0,	/* device mmio region */
	UACCE_QFRT_DUS,		/* device user share */
	UACCE_QFRT_SS,		/* static share memory */
	UACCE_QFRT_MAX,
};
#define UACCE_QFRT_INVALID UACCE_QFRT_MAX

#endif
