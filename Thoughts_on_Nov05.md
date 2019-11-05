
# Changes on Warpdrive

## 1. Basic thoughts on the changes

Since more and more communication interfaces are appended into warpdrive library, the layer of communication interface is a bit thicker. And it's only focused on Hisilicon hardware. Maybe it's not good enough to other vendor's hardware. Kenneth suggests to move this kind of interface out of warpdrive library. So warpdrive could be focus on handling address. It could benefit all vendors.


## 2. Obsolete communication interfaces

***int wd_send(struct wd_queue \*q, void \*req);***

It's used to send data from user apps to hardware. Address are mapped in this function.

***int wd_recv(struct wd_queue \*q, void \*\*resp);***

It's used to receive data from hardware to user apps. Address are mapped in this function.

***int wd_recv_sync(struct wd_queue \*q, void \*\*resp, __u16 ms);***

It's used to receive data from hardware to user apps. Address are mapped in this function.

Although these communication interfaces are obsolete, "wd_adapter.c" still exists. We need to coalesce different hardware implementations into warpdrive library. QM is just an implementation of Hisilicon.  


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


### 3.2 Obsolete Interfaces

***void *wd_drv_mmap_qfr(struct wd_queue \*q, enum uacce_qfrt qfrt, enum uacce_qfrt qfrt_next, size_t size);***

It's used to map qfile region to user space. It's just fill "q->fd" and "q->qfrs_offset[qfrt]" into mmap(). It seems the wrap is unnecessary.  

***void wd_drv_unmap_qfr(struct wd_queue \*q, void \*addr, enum uacce_qfrt qfrt, enum uacce_qfrt qfrt_next, size_t size);***

It's used to destroy the qfile region by unmap(). It seems that we can use munmap() directly.  


### 3.3 Merging Interfaces

Actually there're 3 modes in UACCE framework. They're "SVA", "SHARE_DOMAIN" and "NOIOMMU". Whatever user needs or not, wd_request_queue() maps all qfile regions to user space. This behavior could simplify the operation in user space. And we can use the same idea on coalescing those APIs in warpdrive library.

If only "NOIOMMU" mode is used in user apps, wd_reserve_memory() could allocate memory from SMM area. If "SVA" or "SHARE_DOMAIN" mode is used in user apps, malloc() is used to allocate memory. We could merge these two different behavior into one new interface, "wd_alloc_mem()".

***void \*wd_alloc_mem(struct wd_queue \*q, size_t size);***

If wd_alloc_mem() is used in "SVA" or "SHARE_DOMAIN" mode, malloc() is used to allocate memory and the VA is stored in "q->ss_va". If wd_alloc_mem() is used in "NOIOMMU" mode, "SS" region is mapped and both VA & PA of "SS" region are stored in queue structure.

wd_alloc_mem() returns the virtual address of allocated memory.

***void wd_free_mem(struct wd_queue \*q, size_t size);***

It's used to free memory. If "NOIOMMU" mode is used in user apps, it unmaps "SS" region. If "SVA" or "SHARE_DOMAIN" mode is used in user apps, free() is used and the VA comes from "q->ss_va".

Then we can drop wd_reserve_memory().

### 3.4 Obscure Interfaces

wd_share_reserved_memory() is used in both "SHARE_DOMAIN" and "NOIOMMU" mode. But I do not understand how to share memory in "SVA" mode. I hope to use a share API to cover all 3 modes.


### 3.5 Obscure Names

"NOIOMMU" is a mode. But in uacce_cmd_share_qfr(), we can know iommu_map() is used even in "NOIOMMU" mode. So the name of "NOIOMMU" is obscure, please change it to the right name.