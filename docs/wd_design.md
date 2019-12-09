
# Warpdrive Architecture Design

## Overview

Warpdrive is an accelerator software architecture to help vendors to use their hardware accelerator in user space easily and efficiently. It includes a kernel driver named UACCE and a user space library named libwd.

libwd provides a wrapper of basic UACCE user space interfaces, they will be a set of help functions. And libwd offers a set of APIs based on specific algorithms to users, who could use this set of APIs to do specific task without accessing low level implementations. libwd offers a register interface to let hardware vendors to register their own user space driver, which could use above help functions to do UACCE related work.

![overview](./wd_overview.png)

The key point of Warpdrive is that it is based on Linux SVA technology, which make device sharing the same virtual address space with CPU in user space.

This document focuses on the design of libwd.


## UACCE user space API

As the kernel driver of Warpdrive, UACCE offers a set of API between kernel and user space.

Since UACCE driver is still under upstreaming process, latest APIs of UACCE can be found in <https://lkml.org/lkml/2019/11/22/1728>. UACCE is introduced in "uacce.rst" and "sysfs-driver-uacce" of this patchset.

The basic idea of UACCE driver is that a char device will be created for an accelerator device, which attributes will be showed in sysfs, like "/sys/class/uacce/[device]/[attr_files]". After opening this char device once, user will get a channel to access the resource of this accelerator device. User can configure above channel by ioctl of this opened fd, and mmap hardware resource, like MMIO or channel to user space.


## Addressing interfaces

Addressing interfaces are in the wd helper functions level. They're the interfaces that are based on UACCE in kernel space. UACCE supports three scenarios, SVA scenario, Sharing Domain scenario and NOIOMMU scenario.

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

Most fields of "uacce_dev_info" are parsed from sysfs node.


### struct wd_chan

QM (queue management) is a hardware communication mechanism that is used in Hisilicon platform. But it's not common enough that every vendor adopts this communication mechanism. So suggest to use a much generic name, "wd_chan". *(chan means channel)*


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

In NOIOMMU scenario, "ss_dma" is physical address. But "ss_dma" is just IOVA in SVA scenario.

"struct wd_chan" is expected to be allocated by vendor driver and used in wd helper functions. Suggest vendor driver allocates "struct vendor_chan" instead. The head of "struct vendor_chan" is "struct wd_queue". Additional hardware informations are stored in higher address. Vendor driver could convert the pointer type between "struct wd_chan" and "struct vendor_chan".


## mmap

We can use mmap() and unmap() directly.


### SVA Scenario

In the SVA scenario, IOVA is used in DMA transaction. DMA observes continuous memory because of IOVA, and it's unnecessary to reserve any memory in kernel. So user process could allocate or get memory from anywhere.

The addressing interfaces in Share Domain scenario are in below.

***int wd_request_channel(struct wd_chan \*ch);***

***void wd_release_channel(struct wd_chan \*ch);***

When a process wants to communicate with hardware device, it needs to get a new by *wd_request_channel()*. The process could get memory by allocation (malloc) or any existed memory. If the process wants to use multiple small memory blocks, it just need to get multiple memory. SMM is unnecessary. And the process doesn't need to map the allocated memory to any qfile region of queue.

When a process wants to access the same memory by multiple queues, it could rely on the POSIX shared memory API.


### NOSVA Scenario

#### NOSVA Interfaces

In the NOSVA scenario, it supports both IOMMU and NOIOMMU mode. So physical address is used in DMA transaction. Since scatter/gather mode isn't enabled, DMA needs continuous memory. Now *alloc_pages()* is used to allocate continuous address. When user process needs physical memory, it has to allocate memory from the reserved memory region. When user wants to integrate warpdrive into some libraries, user has to do an extra memory copy to make DMA working if memory isn't allocated from the reserved memory region.

The addressing interfaces in NOSVA scenario are in below.

***int wd_request_channel(struct wd_chan \*ch);***

***void wd_release_channel(struct wd_chan \*ch);***

***void \*wd_reserve_mem(struct wd_chan \*ch, size_t size);***

***int wd_share_reserved_memory(struct wd_chan \*ch, struct wd_chan \*target_ch);***

***int wd_get_dma_from_va(struct wd_chan \*ch, void \*va);***

When a process wants to communicate the hardware device, it calls *wd_request_channel()* and *wd_reserve_mem()* in turn to get a chunk of memory.

*wd_reserve_mem()* maps the **UACCE_QFRT_SS** qfile region to a process, so it could only be called once in a process.

When a process prepares the DMA transaction, it needs to convert VA into PA by *wd_get_pa_from_va()*.

Multiple devices could share same memory region in one process. Then the same memory region needs to be shared between multiple devices. In order to manage the refcount, *wd_share_reserved_memory()* is necessary when Queue B needs to accesses the memory of Queue A.


#### Limitations on NOSVA Scenario

Physical address is used in DMA transactions. It's not good to expose physical address to user space, since it'll cause security issues.

DMA always requires continuous address. Now *alloc_pages()* tries to allocate physical continuous memory in kernel space. But it may fail on large memory size while memory is full of fragement.


### SMM Interfaces

**UACCE_QFRT_SS** qfile region is used in both Share Domain Scenario and NOIOMMU Scenario. SMM could manage the **UACCE_QFRT_SS** region.

When a process needs to use multiple memory blocks, it could make use of SMM. *smm_init()* could do the conversion from big chunk memory to smaller memory. Then the process could allocate and free small memory blocks by *smm_alloc()* and *smm_free()*.

The SMM interfaces are in below.

***int smm_init(void \*pt_addr, size_t size, int align_mask);***

***void \*smm_alloc(void \*pt_addr, size_t size);***

***void smm_free(void \*pt_addr, void \*ptr);***


### API on sharing address

The address could be easily shared in SVA scenario. But we have to use *wd_share_reserved_memory()* to share address in NOSVA scenario. At the same time, we provide the algorithm interfaces to user applicaton because we hope user application not care hardware implementation in details. The two requirements conflicts. So a few new APIs are necessary.

*int wd_is_sharing()* indicates whether dma address could be shared. Return 1 for SVA scenario. Return 0 for NOSVA scenario.

Then user don't need to care about any scenario. It only needs to check whether the address is sharable.


### APIs related to allocate memory by 3rd party library

Allocating memory in different three scenarios are discussed above. If memory is allocated by 3rd party library that could also be used by hardware, we also need to support it in warpdrive. These APIs are necessary in below.

```
    struct wd_3rd_memory {
        void *alloc_mem(void *va, void *dma_addr, size_t size);  // returns va
        void free_mem(void *va);
    };
```
***wd_register_3rd_memory(struct wd_chan \*chan, struct wd_3rd_memory \*mem)*** registers the memory allocation and free in warpdrive.

***wd_unregister_3rd_memory(struct wd_chan \*chan)*** unregisters the memory allocation and free in warpdrive.

When 3rd party memory is used, we need to mark it in flags field of "struct wd_chan".

These two APIs should be used by user application. And the "struct wd_3rd_memory" should be binded into "struct wd_chan", since "ss_va" and "ss_dma" are also stored in "struct wd_chan". And libwd should invoke 3rd party memory allocation function when map **UACCE_QFRT_SS** region to user application. UACCE should avoid to allocate memory if it finds channel could use 3rd party memory allocation.

Note: *wd_register_3rd_memory()* should be used between *wd_request_channel()* and *mmap()*. *wd_unregister_3rd_memory()* should be used between *munmap()* and *wd_unregister_3rd_memory()*.


## Compression Algorithm Interfaces

The hardware accelerator is shared in the system. When compression is ongoing, warpdrive needs to track the current task. So a context is necessary to record key informations.

```
    struct wd_comp_ctx {
        struct wd_chan *ch;
        int alg_type;      /* zlib or gzip */
        int stream_mode;   /* stateless or stateful */
        int new_stream;    /* old stream or new stream */
        int flush_type;    /* NO_FLUSH, SYNC_FLUSH, FINISH, INVALID_FLUSH */
        void *private;     /* vendor specific structure */
    };
```

The context should be allocated in user application. From the view of compression, there're two parts, compression and decompression. From the view of operation, there're two operations, synchronous and asychronous operation. These APIs are the interfaces between user application and vendor driver.

***wd_alg_deflate(struct wd_comp_ctx \*ctx, void \*src, unsigned int \*src_len, void \*dst, unsigned int \*dst_len)*** executes the synchronous compression. The function is blocked until compression done.

***wd_alg_inflate(struct wd_comp_ctx \*ctx, void \*src, unsigned int \*src_len, void \*dst, unsigned int \*dst_len)*** executes the synchronous decompression. The function is blocked until decompression done.

***wd_alg_async_deflate(struct wd_comp_ctx \*ctx, callback_t \*callback, void \*src, unsigned int \*src_len, void \*dst, unsigned int \*dst_len)*** executes the asynchronous compression. The function returns immediately while compression is still on-going. When the compression is done, a predefined callback in user application is executed.

***wd_alg_async_inflate(struct wd_comp_ctx \*ctx, callback_t \*callback, void \*src, unsigned int \*src_len, void \*dst, unsigned int \*dst_len)*** executes the asynchronous decompression. The function returns immediately while decompression is still on-going. When the decompression is done, a predefined callback in user application is executed.

User application needs to configure *struct wd_comp_ctx* directly before starting deflation/inflation.

These APIs are the interfaces between vendor driver and warpdrive help functions.

```
    struct wd_alg_comp {
        int (*init)(...);
        void (*exit)(...);
        int (*deflate)(...);
        int (*inflate)(...);
        int (*callback)(...);
        ...
    };
```

***wd_alg_comp_register(struct wd_alg_comp \*vendor_comp)*** registers the vendor compression and decompression implementation in warpdrive.

***wd_alg_comp_unregister(struct wd_alg_comp \*vendor_comp)*** unregisters the vendor compression and decompression implementation in warpdrive.


### Support zlib-ng

It's discussed in another document "Thoughts_on_zlib_ng.md". The implementation should be in warpdrive. zlib-ng only calls the exported interfaces in warpdrive.

