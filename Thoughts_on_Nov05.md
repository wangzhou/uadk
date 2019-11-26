
# Thoughts on Warpdrive

## Basic thoughts

List the usage case of all scenarios. Try to simpilify the warpdrive. "UACCE_MODE_NOUACCE" mode is ignored since warpdrive can't work without UACCE.

There're two kinds of interfaces, communication interfaces and addressing interfaces. Now we're planning to replace communication interfaces by algorithm interfaces.


## Communication Interfaces

***int wd_send(struct wd_queue \*q, void \*req);***

It's used to send data from user apps to hardware. Address are mapped in this function.

***int wd_recv(struct wd_queue \*q, void \*\*resp);***

It's used to receive data from hardware to user apps. Address are mapped in this function.

***int wd_recv_sync(struct wd_queue \*q, void \*\*resp, __u16 ms);***

It's used to receive data from hardware to user apps. Address are mapped in this function.

Since warpdrive project is mainly focus on accelerator implementations, different algorithm implementations should be supported by warpdrive project. Communication interfaces could abstract the send/receive operations between accelerator hardware and software. Although the send/receive operation is the basic operation, different algorithm need different operations. These different operations could not be covered by send/receive operations. We could move the send/receive operation into algorithm implementation, and it could simplify warpdrive project. So we could remove the communication interfaces.


## Compression Algorithm Interfaces

### Compression

```
    struct wd_comp_ctx {
        struct wd_queue *q;
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

***int wd_comp_init(struct wd_comp_ctx \*ctx);***

***int wd_comp(struct wd_comp_ctx \*ctx, const unsigned char \*src, unsigned int \*src_len, unsigned char \*dest, unsigned int \*dest_len, int flush);***

***int wd_comp_end(struct wd_comp_ctx \*ctx);***


### Decompression

***int wd_decomp_init(struct wd_comp_ctx \*ctx);***

***int wd_decomp(struct wd_comp_ctx \*ctx, const unsigned char \*src, unsigned int \*src_len, unsigned char \*dest, unsigned int \*dest_len, int flush_type);***

***int wd_decomp_end(struct wd_comp_ctx \*ctx);***


### Support zlib-ng

It's discussed in another document "Thoughts_on_zlib_ng.md". The implementation should be in warpdrive. zlib-ng only calls the exported interfaces in warpdrive.


## Addressing interfaces

### Updating "struct _dev_info"

```
struct _dev_info {
  int node_id;
  int numa_dis;
  int iommu_type;
  int flags;
  ...
  unsigned long qfrs_offset[UACCE_QFRT_MAX];
};
```

Suggest to rename the "_dev_info" to "uacce_dev_info". And suggest to move "uacce_dev_info" from "wd.c" to "wd.h". Most fields in "uacce_dev_info" could be parsed from sysfs node. There's no reason to hide these kinds of information.  


### Updating "struct wd_queue"

#### Avoid copy fields from "struct _dev_info"

Some fields in "struct wd_queue" are just copied from "struct _dev_info". Since warpdrive just copy them without changing them, the copy operation is unnecessary at all. So we could simplify the "struct wd_queue" in below.  

```
struct wd_queue {
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

Since "struct wd_queue" is allocated out of "wd_request_queue()", suggest vendor allocate "struct vendor_chan" directly. The head of "struct vendor_chan" is "struct wd_queue". Additional hardware informations are stored in higher address. User could use convert pointer type between "struct wd_queue" and "struct vendor_chan".


#### Rename "struct wd_queue"

Since QM is not common enough for each vendor, suggest to change the name of "wd_queue" to "wd_chan" for more generic.


### Updating "struct wd_drv_dio_if"

#### Clean structure

Since communication interface is abandoned, we don't need to keep "send()" and "recv()" in "struct wd_drv_dio_if" any more.


#### Support different algorithms

In original implementation, different vendor devices are registered with "struct wd_drv_dio_if". Now we're moving to support algorithm interfaces, not communication interfaces. But there're may several methods in one algorithm interfaces. If we fill all of the algorithm methods in a structure, it may be huge. It's better to split "struct wd_drv_dio_if" into different algorithm structures.

Maybe vendor only supports a few hardware accelerators. Vendor should only register related algorithm interface.

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


#### Questions on Share Domain Scenario

IOMMU could bring lots of benefits. One of the benefit is resolving the memory fragement issue. IOMMU could map discrete memory pages into one continuous address, IPA. But it current design, memory is still allocated in kernel space by *alloc_pages()*. System may fail to allocate memory by *alloc_pages()* because of memory fragement.

Maybe we can change to allocate memory in user space instead. Then pin the memory and make IOMMU do the mapping. If so, we need to discard *wd_reserve_mem()* in this scenario.


#### Limitations on Share Domain Scenario

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


## Suggestions

*malloc()*, *wd_reserve_mem()* and *smm_alloc()* all returns virtual memory. In **NOIOMMU** scenario, process needs to fill physical address in DMA transaction, *wd_get_pa_from_va()* is important to get physical address. But *wd_get_va_from_pa()* is unnecessary. We could remove it to simplify the code.

Add the algorithm interfaces into warpdrive.
