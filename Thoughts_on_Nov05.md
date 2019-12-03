
# Thoughts on Warpdrive

## Overview

Warpdrive is designed for hardware accelerator in user-space. From the view of architecture, three kinds of interfaces were defined. They were addressing interfaces, communication interfaces and algorithm interfaces.

Communication interfaces cover the communication between vendor's hardware and user applicantion. And it abstracts the communication behavior as *wd_send()* and *wd_recv()*. Once message is sent to hardware, it's expected to receive a response. From the view of communication, it's the right behavior. From the view of accelerator, it's not exactly true. Maybe user sends multiple messages to hardware and just gets one response from hardware.

We can just move the implementation of communication in the vendor's driver. So we could simply drop the communication interfaces.

When communication interfaces are removed, the overview of warpdrive is in below.

![overview](./wd_overview.png)

Then there're only addressing interfaces and algorithm interfaces. Vendor driver is between these two interfaces.

*In this document, we'll try to evolve the implementation of warpdrive. And all the scenarios are also mentioned. "UACCE_MODE_NOUACCE" mode in kernel is ignored since warpdrive can't work without UACCE.*


## Addressing interfaces

Addressing interfaces are in the wd helper functions level. They're the interfaces that are based on UACCE in kernel space. UACCE supports three scenarios, SVA scenario, Sharing Domain scenario and NOIOMMU scenario.

Some structures are evolving and all scenarios are mentioned in below.

### Evolving "struct _dev_info" to "struct uacce_dev_info"

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

Suggest to rename the "_dev_info" to "uacce_dev_info". Since we know it's uacce device, it's unnecessary to use "_dev" as name. And suggest to move "uacce_dev_info" from "wd.c" to "wd.h". Most fields in "uacce_dev_info" could be parsed from sysfs node. There's no reason to hide these kinds of information.


### Evolving "struct wd_queue" to "struct wd_chan"

#### Rename to "struct wd_channel"

QM (queue management) is a hardware communication mechanism that is used in Hisilicon platform. But it's not common enough that every vendor adopts this communication mechanism. So suggest to use a much generic name, "wd_chan". *(chan means channel)*


#### Avoid copy fields from "struct uacce_dev_info"

Some fields in "struct wd_queue" are just copied from "struct _dev_info". Since warpdrive just copy them without changing them, the copy operation is unnecessary at all. Simplify the "struct wd_chan" in below.

```
struct wd_chan {
  struct uacce_dev_info *dev_info;
  int fd;
  char dev_path[PATH_STR_SIZE];
  void *ss_va;
  void *ss_pa;
};
```


#### Rename ss_pa field

And suggest to rename "ss_pa" to "ss_dma". "ss_pa" is used to fill DMA descriptor. In NOIOMMU scenario, "ss_pa" is physical address. But "ss_pa" is just IOVA in SVA scenario. It is easy to make user confusion. The name of "ss_dma" should be better.


#### Suggestion on "struct vendor_chan"

"struct wd_chan" is expected to allocated by vendor's driver and used in wd helper functions. Suggest vendor driver allocates "struct vendor_chan" instead. The head of "struct vendor_chan" is "struct wd_queue". Additional hardware informations are stored in higher address. Vendor driver could convert the pointer type between "struct wd_chan" and "struct vendor_chan".


### Obsolete "struct wd_drv_dio_if"

"struct wd_drv_dio_if" is focus on communication interface and basic operations. Since communication interface is abandoned and algorithm interface is widely used. This structure is a bit redundant. Some new structures based on algorithm are expected.


### Obsolete Interfaces

***void *wd_drv_mmap_qfr(struct wd_queue \*q, enum uacce_qfrt qfrt, enum uacce_qfrt qfrt_next, size_t size);***

It's used to map qfile region to user space. It's just fill "q->fd" and "q->qfrs_offset[qfrt]" into mmap(). It seems the wrap is unnecessary.

***void wd_drv_unmap_qfr(struct wd_queue \*q, void \*addr, enum uacce_qfrt qfrt, enum uacce_qfrt qfrt_next, size_t size);***

It's used to destroy the qfile region by unmap(). It seems that we can use munmap() directly.


### SVA Scenario

In the SVA scenario, IOVA is used in DMA transaction. DMA observes continuous memory because of IOVA, and it's unnecessary to reserve any memory in kernel. So user process could allocate or get memory from anywhere.

The addressing interfaces in Share Domain scenario are in below.

***int wd_request_channel(struct wd_chan \*ch);***

***void wd_release_channel(struct wd_chan \*ch);***

When a process wants to communicate with hardware device, it needs to get a new by *wd_request_channel()*. The process could get memory by allocation (malloc) or any existed memory. If the process wants to use multiple small memory blocks, it just need to get multiple memory. SMM is unnecessary. And the process doesn't need to map the allocated memory to any qfile region of queue.

When a process wants to access the same memory by multiple queues, it could rely on the POSIX shared memory API.


### Share Domain Scenario

#### Share Domain Interfaces

In the Share Domain scenario, IOMMU is enabled but PASID isn't supported by device. So all DMA transactions are limited in the same IOMMU domain. The memory for DMA transaction is allocated in kernel space. And IOVA is used in DMA transaction.

The addressing interfaces in Share Domain scenario are in below.

***int wd_request_channel(struct wd_chan \*ch);***

***void wd_release_channel(struct wd_chan \*ch);***

***void \*wd_reserve_mem(struct wd_chan \*ch, size_t size);***

***int wd_share_reserved_memory(struct wd_chan \*ch, struct wd_chan \*target_ch);***

When a process wants to communicate with hardware device, it needs to call *wd_request_channel()* and *wd_reserve_mem()* in turn to get a chunk of memory. If a process wants to use multiple small memory blocks, it just need to get multiple memory by SMM.

Multiple devices could share same memory region in one process. Then the same memory region needs to be shared between multiple devices. Since there's only one IOMMU domain and one process in this scenario, different devices could understand the same memory region. In order to manage the refcount, *wd_share_reserved_memory()* is necessary when Queue B needs to accesses the memory of Queue A.


#### Limitations on Share Domain Scenario

IOMMU could bring lots of benefits. One of the benefit is resolving the memory fragement issue. IOMMU could map discrete memory pages into one continuous address, IPA. But it current design, memory is still allocated in kernel space by *alloc_pages()*. System may fail to allocate memory by *alloc_pages()* because of memory fragement.

All DMA transitions are limited in one IOMMU domain.


### NOIOMMU Scenario

#### NOIOMMU Interfaces

In the NOIOMMU scenario, physical address is used in DMA transaction. Since scatter/gather mode isn't enabled, DMA needs continuous memory. Now *alloc_pages()* is used to allocate continuous address. When user process needs physical memory, it has to allocate memory from the reserved memory region. When user wants to integrate warpdrive into some libraries, user has to do an extra memory copy to make DMA working if memory isn't allocated from the reserved memory region.

The addressing interfaces in NOIOMMU scenario are in below.

***int wd_request_channel(struct wd_chan \*ch);***

***void wd_release_channel(struct wd_chan \*ch);***

***void \*wd_reserve_mem(struct wd_chan \*ch, size_t size);***

***int wd_share_reserved_memory(struct wd_chan \*ch, struct wd_chan \*target_ch);***

***int wd_get_pa_from_va(struct wd_chan \*ch, void \*va);***

***int wd_get_va_from_pa(struct wd_chan \*ch, void \*pa);***

When a process wants to communicate the hardware device, it calls *wd_request_channel()* and *wd_reserve_mem()* in turn to get a chunk of memory.

*wd_reserve_mem()* maps the **UACCE_QFRT_SS** qfile region to a process, so it could only be called once in a process.

When a process prepares the DMA transaction, it needs to convert VA into PA by *wd_get_pa_from_va()*.

Multiple devices could share same memory region in one process. Then the same memory region needs to be shared between multiple devices. In order to manage the refcount, *wd_share_reserved_memory()* is necessary when Queue B needs to accesses the memory of Queue A.


#### Redundant Interfaces

Both *wd_reserve_mem()* and *smm_alloc()* all returns virtual memory. In NOIOMMU scenario, process needs to fill physical address in DMA transaction, *wd_get_pa_from_va()* is important to get physical address. But *wd_get_va_from_pa()* is unnecessary. We could remove it to simplify the code.


#### Limitations on NOIOMMU Scenario

Physical address is used in DMA transactions. It's not good to expose physical address to user space, since it'll cause security issues.

DMA always requires continuous address. Now *alloc_pages()* tries to allocate physical continuous memory in kernel space. But it may fail on large memory size while memory is full of fragement.


### SMM Interfaces

**UACCE_QFRT_SS** qfile region is used in both Share Domain Scenario and NOIOMMU Scenario. SMM could manage the **UACCE_QFRT_SS** region.

When a process needs to use multiple memory blocks, it could make use of SMM. *smm_init()* could do the conversion from big chunk memory to smaller memory. Then the process could allocate and free small memory blocks by *smm_alloc()* and *smm_free()*.

The SMM interfaces are in below.

***int smm_init(void \*pt_addr, size_t size, int align_mask);***

***void \*smm_alloc(void \*pt_addr, size_t size);***

***void smm_free(void \*pt_addr, void \*ptr);***


### New APIs related to sharing

The address could be easily shared in SVA scenario. But we have to use *wd_share_reserved_memory()* to share address in both Share Domain scenario and NOIOMMU scenario. At the same time, we provide the algorithm interfaces to user applicaton because we hope user application not care hardware implementation in details. The two requirements conflicts. So a few new APIs are necessary.

*int wd_is_sharing(struct wd_chan \*chan, void \*dma_addr)* indicates whether the dma_addr could be shared. Return 1 for SVA scenario. Return 0 for both Share Domain scenario and NOIOMMU scenario.

*int wd_share_dma_addr(struct wd_chan \*dst_chan, struct wd_chan \*src_chan)* shares the dma_addr from src_chan to dst_chan.

These two APIs is provided to user application by warpdrive.


### New APIs related to allocate memory by 3rd party library

Allocating memory in different three scenarios are discussed above. If memory is allocated by 3rd party library that could also be used by hardware, we also need to support it in warpdrive. These APIs are necessary in below.

```
    struct wd_3rd_memory {
        void \*alloc_mem(void \*va, void \*pa);  // returns va
        void free_mem(void \*va);
    };
```
*wd_register_3rd_memory(struct wd_3rd_memory \*mem)* registers the memory allocation and free in warpdrive.

*wd_unregister_3rd_memory(struct wd_3rd_memory \*mem)* unregisters the memory allocation and free in warpdrive.

When 3rd party memory is used, we need to mark it in "struct wd_chan".


## Compression Algorithm Interfaces

The hardware accelerator is shared in the system. When compression is ongoing, warpdrive needs to track the current task. So a context is necessary to record key informations.

```
    struct wd_comp_ctx {
        struct wd_chan *ch;
        void *in;
        void *out;
        int window_size;
        int alg_type;      /* zlib or gzip */
        int comp_level;    /* 0-9 */
        int stream_mode;   /* stateless or stateful */
        int new_stream;    /* old stream or new stream */
        int flush_type;    /* NO_FLUSH, SYNC_FLUSH, FINISH, INVALID_FLUSH */
        void *private;     /* vendor specific structure */
    };
```

The context should be allocated in user application. From the view of compression, there're two parts, compression and decompression. From the view of operation, there're two operations, synchronous and asychronous operation. These APIs are the interfaces between user application and vendor driver.

*wd_alg_deflate(struct wd_comp_ctx \*ctx, ...)* executes the synchronous compression. The function is blocked until compression done.

*wd_alg_inflate(struct wd_comp_ctx \*ctx, ...)* executes the synchronous decompression. The function is blocked until decompression done.

*wd_alg_async_deflate(struct wd_comp_ctx \*ctx, callback_t \*callback, ...)* executes the asynchronous compression. The function returns immediately while compression is still on-going. When the compression is done, a predefined callback in user application is executed.

*wd_alg_async_inflate(struct wd_comp_ctx \*ctx, callback_t \*callback, ...)* executes the asynchronous decompression. The function returns immediately while decompression is still on-going. When the decompression is done, a predefined callback in user application is executed.

These APIs are the interfaces between vendor driver and warpdrive help functions.

```
    struct wd_alg_comp {
        int (\*init)(...);
        void (\*exit)(...);
        int (\*deflate)(...);
        int (\*inflate)(...);
        int (\*callback)(...);
        ...
    };
```

*wd_alg_comp_register(struct wd_alg_comp \*vendor_comp)* registers the vendor compression and decompression implementation in warpdrive.

*wd_alg_comp_unregister(struct wd_alg_comp \*vendor_comp)* unregisters the vendor compression and decompression implementation in warpdrive.


### Support zlib-ng

It's discussed in another document "Thoughts_on_zlib_ng.md". The implementation should be in warpdrive. zlib-ng only calls the exported interfaces in warpdrive.

