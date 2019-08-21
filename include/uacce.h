/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _UAPIUUACCE_H
#define _UAPIUUACCE_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define UACCE_CMD_SHARE_SVAS	_IO('W', 0)
#define UACCE_CMD_START		_IO('W', 1)
#define UACCE_CMD_GET_SS_DMA	_IOR('W', 2, unsigned long)

/**
 * UACCE Device Attributes:
 *
 * NOIOMMU: the device has no IOMMU support
 *	can do share sva, but no map to the dev
 * PASID: the device has IOMMU which support PASID setting
 *	can do share sva, mapped to dev per process
 * FAULT_FROM_DEV: the device has IOMMU which can do page fault request
 *	no need for share sva, should be used with PASID
 * SVA: full function device
 * SHARE_DOMAIN: no PASID, can do share sva only for one process and the kernel
 */
enum {
        UACCE_DEV_SHARE_DOMAIN = 0x0,
        UACCE_DEV_PASID = 0x1,
        UACCE_DEV_FAULT_FROM_DEV = 0x2,
        UACCE_DEV_SVA = UACCE_DEV_PASID | UACCE_DEV_FAULT_FROM_DEV,
        UACCE_DEV_NOIOMMU = 0x4,
};

#define UACCE_API_VER_NOIOMMU_SUBFIX	"_noiommu"

#define UACCE_QFR_NA ((unsigned long)-1)
enum uacce_qfrt {
	UACCE_QFRT_MMIO = 0,	/* device mmio region */
	UACCE_QFRT_DKO,		/* device kernel-only */
	UACCE_QFRT_DUS,		/* device user share */
	UACCE_QFRT_SS,		/* static share memory */
	UACCE_QFRT_MAX,
};
#define UACCE_QFRT_INVALID UACCE_QFRT_MAX

#endif
