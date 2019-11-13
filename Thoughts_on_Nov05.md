
# Thoughts on Warpdrive

## 1. Basic thoughts

List the usage case of all scenarios. Try to simpilify the warpdrive. "UACCE_MODE_NOUACCE" mode is ignored since warpdrive can't work without UACCE.


## 2. Communication Interfaces

***int wd_send(struct wd_queue \*q, void \*req);***

It's used to send data from user apps to hardware. Address are mapped in this function.

***int wd_recv(struct wd_queue \*q, void \*\*resp);***

It's used to receive data from hardware to user apps. Address are mapped in this function.

***int wd_recv_sync(struct wd_queue \*q, void \*\*resp, __u16 ms);***

It's used to receive data from hardware to user apps. Address are mapped in this function.

Since these communication interfaces are based on *struct wd_queue*, move them out of warpdrive is not easy to maintain. Suggest to keep them. But do not suggest to append any more interfaces for communication.


## 3. Addressing interfaces

### 3.1 Updating Structures

#### 3.1.1 struct _dev_info
struct _dev_info {  
  int node_id;  
  int numa_dis;  
  int iommu_type;  
  int flags;  
  ...  
  unsigned long qfrs_offset[UACCE_QFRT_MAX];  
};  

Suggest to rename the "_dev_info" to "uacce_dev_info". And suggest to move "uacce_dev_info" from "wd.c" to "wd.h". Most fields in "uacce_dev_info" could be parsed from sysfs node. There's no reason to hide these kinds of information.  

#### 3.1.2 struct wd_queue

Some fields in "struct wd_queue" are just copied from "struct _dev_info". Since warpdrive just copy them without changing them, the copy operation is unnecessary at all. So we could simplify the "struct wd_queue" in below.  

struct wd_queue {  
  struct uacce_dev_info *dev_info;  
  int fd;  
  char dev_path[PATH_STR_SIZE];  
  void *ss_va;  
  void *ss_pa;  
};  

And suggest to rename "ss_pa" to "ss_dma". "ss_pa" is used to fill DMA descriptor. In NOIOMMU scenario, "ss_pa" is physical address. But in SVA or Share Domain scenario, "ss_pa" is just IOVA. So "ss_pa" is easy to make user confusion.

### 3.2 Obsolete Interfaces

***void *wd_drv_mmap_qfr(struct wd_queue \*q, enum uacce_qfrt qfrt, enum uacce_qfrt qfrt_next, size_t size);***

It's used to map qfile region to user space. It's just fill "q->fd" and "q->qfrs_offset[qfrt]" into mmap(). It seems the wrap is unnecessary.  

***void wd_drv_unmap_qfr(struct wd_queue \*q, void \*addr, enum uacce_qfrt qfrt, enum uacce_qfrt qfrt_next, size_t size);***

It's used to destroy the qfile region by unmap(). It seems that we can use munmap() directly.  


### 3.3 SVA Scenario

In the SVA scenario, IOVA is used in DMA transaction. DMA observes continuous memory because of IOVA, and it's unnecessary to reserve any memory in kernel. So user process could allocate or get memory from anywhere.

The addressing interfaces in Share Domain scenario are in below.

***int wd_request_queue(struct wd_queue \*q);***

***void wd_release_queue(struct wd_queue \*q);***

When a process wants to communicate with hardware device, it needs to get a new by *wd_request_queue()*. The process could get memory by allocation (malloc) or any existed memory. If the process wants to use multiple small memory blocks, it just need to get multiple memory. SMM is unnecessary. And the process doesn't need to map the allocated memory to any qfile region of queue.

When a process wants to access the same memory by multiple queues, it could rely on the POSIX shared memory API.


### 3.4 Share Domain Scenario

#### 3.4.1 Share Domain Interfaces

In the Share Domain scenario, IOMMU is enabled but PASID isn't supported by device. So all DMA transactions are limited in the same IOMMU domain. The memory for DMA transaction is allocated in kernel space. And IOVA is used in DMA transaction.

The addressing interfaces in Share Domain scenario are in below.

***int wd_request_queue(struct wd_queue \*q);***

***void wd_release_queue(struct wd_queue \*q);***

***void \*wd_reserve_mem(struct wd_queue \*q, size_t size);***

***int wd_share_reserved_memory(struct wd_queue \*q, struct wd_queue \*target_q);***

When a process wants to communicate with hardware device, it needs to call *wd_request_queue()* and *wd_reserve_mem()* in turn to get a chunk of memory. If a process wants to use multiple small memory blocks, it just need to get multiple memory by SMM.

Multiple devices could share same memory region in one process. Then the same memory region needs to be shared between multiple devices. Since there's only one IOMMU domain and one process in this scenario, different devices could understand the same memory region. In order to manage the refcount, *wd_share_reserved_memory()* is necessary when Queue B needs to accesses the memory of Queue A.


#### 3.4.2 Questions on Share Domain Scenario

IOMMU could bring lots of benefits. One of the benefit is resolving the memory fragement issue. IOMMU could map discrete memory pages into one continuous address, IPA. But it current design, memory is still allocated in kernel space by *alloc_pages()*. System may fail to allocate memory by *alloc_pages()* because of memory fragement.

Maybe we can change to allocate memory in user space instead. Then pin the memory and make IOMMU do the mapping. If so, we need to discard *wd_reserve_mem()* in this scenario.


#### 3.4.3 Limitations on Share Domain Scenario

All DMA transitions are limited in one IOMMU domain.


### 3.5 NOIOMMU Scenario

#### 3.5.1 NOIOMMU Interfaces

In the NOIOMMU scenario, physical address is used in DMA transaction. Since scatter/gather mode isn't enabled, DMA needs continuous memory. Now *alloc_pages()* is used to allocate continuous address. When user process needs physical memory, it has to allocate memory from the reserved memory region. When user wants to integrate warpdrive into some libraries, user has to do an extra memory copy to make DMA working if memory isn't allocated from the reserved memory region.

The addressing interfaces in NOIOMMU scenario are in below.

***int wd_request_queue(struct wd_queue \*q);***

***void wd_release_queue(struct wd_queue \*q);***

***void \*wd_reserve_mem(struct wd_queue \*q, size_t size);***

***int wd_share_reserved_memory(struct wd_queue \*q, struct wd_queue \*target_q);***

***int wd_get_pa_from_va(struct wd_queue \*q, void \*va);***

***int wd_get_va_from_pa(struct wd_queue \*q, void \*pa);***

When a process wants to communicate the hardware device, it calls *wd_request_queue()* and *wd_reserve_mem()* in turn to get a chunk of memory.

*wd_reserve_mem()* maps the **UACCE_QFRT_SS** qfile region to a process, so it could only be called once in a process.

When a process prepares the DMA transaction, it needs to convert VA into PA by *wd_get_pa_from_va()*.

Multiple devices could share same memory region in one process. Then the same memory region needs to be shared between multiple devices. In order to manage the refcount, *wd_share_reserved_memory()* is necessary when Queue B needs to accesses the memory of Queue A.


#### 3.5.2 Redundant Interfaces

Both *wd_reserve_mem()* and *smm_alloc()* all returns virtual memory. In NOIOMMU scenario, process needs to fill physical address in DMA transaction, *wd_get_pa_from_va()* is important to get physical address. But *wd_get_va_from_pa()* is unnecessary. We could remove it to simplify the code.


#### 3.5.3 Limitations on NOIOMMU Scenario

Physical address is used in DMA transactions. It's not good to expose physical address to user space, since it'll cause security issues.

DMA always requires continuous address. Now *alloc_pages()* tries to allocate physical continuous memory in kernel space. But it may fail on large memory size while memory is full of fragement.


#### 3.6 SMM Interfaces

**UACCE_QFRT_SS** qfile region is used in both Share Domain Scenario and NOIOMMU Scenario. SMM could manage the **UACCE_QFRT_SS** region.

When a process needs to use multiple memory blocks, it could make use of SMM. *smm_init()* could do the conversion from big chunk memory to smaller memory. Then the process could allocate and free small memory blocks by *smm_alloc()* and *smm_free()*.

The SMM interfaces are in below.

***int smm_init(void \*pt_addr, size_t size, int align_mask);***

***void \*smm_alloc(void \*pt_addr, size_t size);***

***void smm_free(void \*pt_addr, void \*ptr);***


### 4 Suggestions

*malloc()*, *wd_reserve_mem()* and *smm_alloc()* all returns virtual memory. In **NOIOMMU** scenario, process needs to fill physical address in DMA transaction, *wd_get_pa_from_va()* is important to get physical address. But *wd_get_va_from_pa()* is unnecessary. We could remove it to simplify the code.

*wd_reserve_mem()* is used to allocate memory in kernel for both **Share Domain** and **NOIOMMU** scenario. Actually IOMMU could map continuous IPA address to discrete PA addresses. We could allocate memory in user space, and do the address mapping in IOMMU.
