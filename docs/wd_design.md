
# Warpdrive Architecture Design

version 0.90

## Overview

Warpdrive is an accelerator software architecture to help vendors to use their 
hardware accelerator in user space easily and efficiently. It includes a kernel 
driver named UACCE and a user space library named libwd.

libwd provides a wrapper of basic UACCE user space interfaces, they will be a 
set of help functions. And libwd offers a set of APIs based on specific 
algorithms to users, who could use this set of APIs to do specific task without 
accessing low level implementations. libwd offers a register interface to let 
hardware vendors to register their own user space driver, which could use above 
help functions to do UACCE related work.

![overview](./wd_overview.png)

The key point of Warpdrive is that it is based on Linux SVA technology, which 
makes device sharing the same virtual address space with CPU in user space. 
This technology is based on IOMMU.

This document focuses on the design of libwd.


## UACCE user space API

As the kernel driver of Warpdrive, UACCE offers a set of API between kernel 
and user space.

Since UACCE driver is still under upstreaming process, latest APIs of UACCE 
can be found in <https://lkml.org/lkml/2019/11/22/1728>. UACCE is introduced 
in "uacce.rst" and "sysfs-driver-uacce" of this patchset.

The basic idea of UACCE driver is that a char device will be created for an 
accelerator device, which attributes will be showed in sysfs, like 
"/sys/class/uacce/[device]/[attr_files]". After opening this char device once, 
user will get a channel to access the resource of this accelerator device. 
User can configure above channel by ioctl of this opened fd, and mmap hardware 
resource, like MMIO or channel to user space.


## Warpdrive Algorithm Interface

Warpdrive is a framework that supports multiple algorithm interfaces. We'll 
discuss compression algorithm interface in below. And crypto algorithm 
interface will be added later.


### Compression Algorithm Interface

Warpdrive compression algorithm interface is between user application and 
vendor driver.


#### Context

The hardware accelerator is shared in the system. When compression is ongoing, 
warpdrive needs to track the current task. So a context is necessary to record 
key informations. User application needs to request a context before 
compression or decompression.

```
    struct wd_alg_comp {
        char *name;
        int (*init)(...);
        void (*exit)(...);
        int (*deflate)(...);
        int (*inflate)(...);
    };

    struct wd_comp_ctx {
        int                 alg_type;    /* zlib or gzip */
        
        /* 3rd party memory allocation */
        void                *(*alloc_mem)(size_t size);
        void                (*free_mem)(void *address);
        void                *(*va_to_dma)(void *address);
        
        struct wd_alg_comp  *hw;
        void                *priv;       /* vendor specific structure */
    };

    struct wd_comp_arg {
        void         *src;
        size_t       src_len;
        void         *dst;
        size_t       dst_len;
        int          flush_type;  /* NO_FLUSH, SYNC_FLUSH, FINISH, INVALID */
    };
```

***struct wd_comp_ctx \*wd_alg_comp_alloc_ctx(void)*** requests a context.

***void wd_alg_comp_free_ctx(struct wd_comp_ctx \*ctx)*** releases a context.


#### Compression & Decompression

Warpdrive compression algorithm provides the hook interface of hardware 
implementation. It's "hw" field in "struct wd_comp_ctx".

"struct wd_comp_arg" stores the information about source and destination 
buffers. This structure is the input parameter of compression and decompression.

If user application needs the synchronous compression or decompression, 
functions are used in below.

***wd_alg_compress(struct wd_comp_ctx \*ctx, struct wd_comp_arg \*arg)*** 
***wd_alg_decompress(struct wd_comp_ctx \*ctx, struct wd_comp_arg \*arg)*** 

When the above two functions are invoked, user application is blocked until
the hardware operation is done.

If user application wants to handle more tasks in parallel, asynchronous 
operations are used in below.

***wd_alg_async_compress(struct wd_comp_ctx \*ctx, struct wd_comp_arg \*arg, 
callback_t \*callback)***
***wd_alg_async_decompress(struct wd_comp_ctx \*ctx, struct wd_comp_arg \*arg, 
callback_t \*callback)***

When the above two functions are invoked, they return immediately. User 
application could execute other tasks without delay. The *callback()* function 
is defined in user application. When hardware compression or decompression
is finished, it calls the callback function to inform user application.


#### Register Compression Algorithm

When user application executes compression or decompression, the hardware 
implementation that defined in "hw" field of the context is invoked. This
"hw" field is registered by vendor driver.

Warpdrive compression algorithm layer maintains a list of "struct wd_alg_comp". 
When the request of compression or decompression arrives, it compares the 
device name with vendor driver. If they match, related hardware implementation 
is linked to "hw" field of "struct wd_comp_ctx". By this way, vendor driver 
binds to compression algorithm layer with context.


#### 3rd party memory allocation

In addition, we hope to support the case that 3rd party application allocates 
memory and shares the address to Warpdrive. It's necessary to provide the hook 
in the context. If user application doesn't want to use 3rd party application, 
vendor driver could fill the function pointer to Warpdrive helper or malloc(). 
By now, 3rd party memory allocation isn't supported by UACCE yet.


## Warpdrive helper interfaces

Warpdrive helper interfaces that are based on UACCE in kernel space. UACCE 
supports two scenarios, SVA scenario and NOSVA scenario.

Some structures are evolving and all scenarios are mentioned in below.

### struct uacce_dev_info

```
    struct uacce_dev_info {
        int node_id;
        int numa_dis;
        int iommu_type;
        int flags;
        ...
        unsigned long qfrs_offset[UACCE_QFRT_MAX];
    };
```

Most fields of "uacce_dev_info" are parsed from sysfs node. This structure 
is used in Warpdrive helper layer. And it won't be exported to user application.


### struct wd_chan

QM (queue management) is a hardware communication mechanism that is used in 
Hisilicon platform. But it's not common enough that every vendor adopts this 
communication mechanism. So suggest to use a much generic name, "wd_chan". 
*(chan means channel)*


```
    struct wd_chan {
        int fd;
        char dev_path[PATH_STR_SIZE];
        void *ss_va;
        void *ss_dma;
        void *dev_info;   // point to struct uacce_dev_info
        unsigned int flags; // WD_CHANNEL_FLAG_3RD_MEM_ALLOC
        struct wd_3rd_memory mem_3rd;
    };
```

In NOSVA scenario, "ss_dma" is physical address. But "ss_dma" is just IOVA 
in SVA scenario.

"struct wd_chan" is expected to be allocated by vendor driver and used in wd 
helper functions. Suggest vendor driver allocates "struct vendor_chan" 
instead. The head of "struct vendor_chan" is "struct wd_queue". Additional 
hardware informations are stored in higher address. Vendor driver could 
convert the pointer type between "struct wd_chan" and "struct vendor_chan".


### mmap

***void *wd_drv_mmap_qfr(struct wd_chan \*ch, enum uacce_qfrt qfrt, 
size_t size);*** maps qfile region to user space. It's just fill "q->fd" 
and "q->qfrs_offset[qfrt]" into mmap().

***void wd_drv_unmap_qfr(struct wd_chan \*ch, void \*addr, 
enum uacce_qfrt qfrt, size_t size);*** destroys the qfile region by unmap().


### SVA Scenario

In the SVA scenario, IOVA is used in DMA transaction. DMA observes continuous 
memory because of IOVA, and it's unnecessary to reserve any memory in kernel. 
So user process could allocate or get memory from anywhere.

The addressing interfaces in Share Domain scenario are in below.

***int wd_request_channel(struct wd_chan \*ch);***

***void wd_release_channel(struct wd_chan \*ch);***

When a process wants to communicate with hardware device, it needs to get a 
new by *wd_request_channel()*. The process could get memory by allocation 
(malloc) or any existed memory. If the process wants to use multiple small 
memory blocks, it just need to get multiple memory.

When a process wants to access the same memory by multiple queues, it could 
rely on the POSIX shared memory API.


### NOSVA Scenario


In the NOSVA scenario, Warpdrive works whatever IOMMU is enabled or not. Since 
scatter/gather mode isn't enabled, DMA needs continuous memory. So physical 
address should to be filled for DMA transaction.

When user application needs memory, it should send memory request to Warpdrive. 
The memory with physical continuous address is allocated by DMA API in kernel 
space. Warpdrive maps the continuous memory in kernel space to 
**UACCE_QFRT_SS** qfile region.

When user wants to integrate Warpdrive into some libraries with NOSVA scenario, 
user has to do an extra memory copy since it needs the memory with physical 
continuous address.

And DMA API can't allocate large size memory. NOSVA scenario isn't the major 
target in Warpdrive. Warpdrive is designed for SVA scenario. So NOSVA scenario 
will be abandoned in the future.


#### NOSVA Interfaces

The interfaces in NOSVA scenario are in below.

***int wd_request_channel(struct wd_chan \*ch);***

***void wd_release_channel(struct wd_chan \*ch);***

***void \*wd_reserve_mem(struct wd_chan \*ch, size_t size);***

***int wd_get_dma_from_va(struct wd_chan \*ch, void \*va);***

When a process wants to communicate the hardware device, it calls 
*wd_request_channel()* and *wd_reserve_mem()* in turn to get a chunk of memory.

*wd_reserve_mem()* maps the **UACCE_QFRT_SS** qfile region to a process, so it 
could only be called once in a process.

When a process prepares the DMA transaction, it needs to convert VA into PA by 
*wd_get_pa_from_va()*.

*wd_reserve_mem()* and *wd_get_dma_from_va()* are only used in NOSVA scenario. 
Since NOSVA isn't a longtime supported scenario, these APIs will be abandoned 
in the future.


### Sharing address

The address could be easily shared in SVA scenario. But we don't support 
sharing address in NOSVA scenario.


