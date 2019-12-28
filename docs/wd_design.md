
# WarpDrive Architecture Design


| Version | Author | Changes |
| --- | :---- | :---- |
|  0.91   | Haojian Zhuang |1) Remove the content of 3rd party memory  |
|         | Zhou Wang      |   allocation. |
|         |                |2) Remove "ss_va" and "ss_dma" from struct wd_chan.|
|         |                |3) Change to user app polling async interface.  |
|         |                |4) Add examples.  |
|  0.92   |                |1) Reorganize the document. |
|         |                |2) Remove some structures that are unused in apps. |
|  0.93   |                |1) Avoid to discuss whether IOMMU disabled in NOSVA |
|         |                |   scenario since it's not important. |
|         |                |2) Remove multiple queue since it's transparent to |
|         |                |   user application. |
|  0.94   |                |1) Split WarpDrive into UACCE, libwd, algorithm |
|         |                |   libraries and libaffinity. Change doc according |
|         |                |   to this notion. |
|         |                |2) Illustrate how to select hardware accelerator. |
|         |                |3) Illustrate how libaffinity working. |
|  0.95   |                |1) Remove libaffinity extension for unclear logic. |
|         |                |2) Add API to identify NOSVA in libwd. |
|  0.96   |                |1) Fix on asynchronous operation. |
|  0.97   |                |1) Fix the missing hook of async poll. |
|         |                |2) Illustrate more on binding driver. |
|  0.98   |                |1) Do not expose context to user application. Use |
|         |                |   handler instead. |
|         |                |2) Illustrate each parameter or field in table. |

## Terminology

| Term | Illustration |
| :-- | :-- |
| SVA  | Shared Virtual Addressing |
| NUMA | Non Uniform Memory Access |

## Overview

WarpDrive is a framework for user application to access hardware accelerator 
in a unified, secure, and efficient way. WarpDrive is comprised of UACCE, libwd 
and many other algorithm libraries for different applications.

![overview](./wd_overview.png)

Libwd provides a wrapper of basic UACCE user space interfaces, they will be a 
set of helper functions. Libwd offers a register interface to let hardware 
vendors to register their own user space driver, which could use above helper 
functions to do UACCE related work.

Algorithm libraries offer a set of APIs to users, who could use this set of 
APIs to do specific task without accessing low level implementations.

This document focuses on the design of libwd and algorithm libraries.


## Based Technology

WarpDrive relies on SVA (Shared Virtual Address). With SVA, IOMMU shares 
the same page table with MMU that is used by CPU. It means the same virtual 
address could be accepted by IOMMU. It could share the memory easily between 
CPU and devices without any memory copy.

When SVA feature isn't enabled, the CPU address is different from device 
address in WarpDrive. These two scenarioes are called SVA scenario and NOSVA 
scenario.

In SVA scenario, memory is allocated in user space. In NOSVA scenario, memory 
is allocated in kernel space. So memory copy is required. And physical address 
has to be used in vendor driver. This behavior exposes physical address to 
user space. It'll cause the potential security issue. The NOSVA scenario is 
only used when SVA feature is disabled. And it'll be removed in the future.

In SVA scenario, memory address is always virtual address whatever it's in 
user application or vendor driver. So sharing memory address is easily without 
any memory copy. In NOSVA scenario, memory address is virtual address only 
in user application. But it's physical or bus address in vendor driver. Of 
course, virtual address could be shared without any issue. But it implied 
memory copy between vendor driver and user application. Performance will 
be hurted in this case.

Because it has to allocate memory in kernel space for continuous address, it 
is limited to allocate small pieces of memory in NOSVA scenario.

According to the limitations on NOSVA scenario, it'll be removed in the future. 
WarpDrive is designed for SVA scenario.


### UACCE user space API

As the kernel driver of WarpDrive, UACCE offers a set of APIs between kernel 
and user space.

Since UACCE driver is still under upstreaming process, latest APIs of UACCE 
can be found in <https://lkml.org/lkml/2019/11/22/1728>. UACCE is introduced 
in "uacce.rst" and "sysfs-driver-uacce" of this patchset.

The basic idea of UACCE driver is that a char device will be created for an 
accelerator device, which attributes will be showed in sysfs, like 
"/sys/class/uacce/[device]/[attr_files]". After opening this char device once, 
vendor driver will get a channel to access the resource of this accelerator 
device. Vendor driver can configure above channel by ioctl of this opened fd, 
and mmap hardware resource, like MMIO or channel to user space.


## Libaffinity

### Libaffinity APIs

Libaffinity is used to set the range of hardware accelerators. Then algorithm 
library could choose a high priority device from the set. Libaffinity isn't 
the requisite component in Warpdrive framework. It's only used when user 
application knows the hardware in detail and wants to choose a few specific 
accelerators to gain better performance.

When each hardware accelerator registers in UACCE subsystem, it gets an ID that 
is attached in the device name. User could get it from sysfs node.

```
    typedef wd_dev_mask_t   unsigned long long int;
    wd_dev_mask_t           dev_mask;
```

The ID is counted from 0. The type of 64-bit unsigned value, *wd_dev_mask_t*, 
is defined to collect all hardware accelerators in the system. Each bit 
is relative to one hardware accelerator. Bit 0 means the hardware accelerator 
with ID 0. It also means that libaffinity only support 64 accelerators at most.

***int wd_get_affinity(char \*alg_name, wd_dev_mask_t \*dev_mask);***

| Layer | Parameter | Direction | Comments |
| :-- | :-- | :-- | :-- |
| affinity | *alg_name* | Input  | Indicate the name of algorithm. |
|          | *dev_mask* | Output | Indicate the mask bits of all valid UACCE |
|          |            |        | devices. |

*wd_get_affinity()* returns the set of matched accelerators that support 
specified algorithm with *alg_name*. The set is contained in *dev_mask*. The 
return value is 0 if it succeeds to find the set of accelerators. The return 
value is negative value if it fails to find the set.

***int wd_get_domain_affinity(int id, wd_dev_mask_t \*dev_mask);***

| Layer | Parameter | Direction | Comments |
| :-- | :-- | :-- | :-- |
| affinity | *id*       | Input  | Indicate the ID of accelerator in UACCE |
|          |            |        | subsystem. |
|          | *dev_mask* | Output | Indicate the mask bits of valid UACCE |
|          |            |        | devices in the same IOMMU domain. |

*wd_get_domain_affinity()* is different from *wd_get_affinity()*. It wants to 
query the accelerators in the same IOMMU domain with specified accelerator.

Mutliple hardware accelerators may share a same IOMMU domain. User application 
may hope to share data among different accelerators. If they're are in the same 
IOMMU domain, it may get better performance than different IOMMU domain.

*wd_get_domain_affinity()* returns the set of matched accelerators that are in 
the same IOMMU domain. The set is stored in *dev_mask*. The return value is 0 
if the set is found. The return value is negative value if it fails to find the 
set.

***int wd_get_numa_affinity(int id, wd_dev_mask_t \*dev_mask);***

| Layer | Parameter | Direction | Comments |
| :-- | :-- | :-- | :-- |
| affinity | *id*       | Input  | Indicate the ID of accelerator in UACCE |
|          |            |        | subsystem. |
|          | *dev_mask* | Output | Indicate the mask bits of valid UACCE |
|          |            |        | devices in the same NUMA node range. |

*wd_get_numa_affinity()* is similar to *wd_get_domain_affinity()*. The only 
difference is to query the accelerators in the range of the same NUMA node.

In the same NUMA node, memory should be in the equal distance from different 
devices. User application should gain better performance on it if data needs 
to be shared among different accelerators.

*wd_get_domain_affinity()* returns the set of matched accelerators that are in 
the range of the same NUMA node. The set is stored in *dev_mask*. The return 
value is 0 if the set is found. The return value is negative value if it fails 
to find the set.

*wd_get_affinity()*, *wd_get_domain_affinity()* and *wd_get_numa_affinity()* 
all query the set of matched accelerators. User application needs to limit 
algorithm library to chose an accelerator in this set. All are based on the 
*dev_mask* value. Libaffinity doesn't need to call any API in algorithm 
library. User application gets the *dev_mask* value already, then it just needs 
to notify algorithm library to chose an accelerator in the limited range.

The model of usage case is in below.

User already knows which specified accelerators could bring better performance. 
It just query these accelerators by *wd_get_affinity()* or the combination in 
*wd_get_affinity()*, *wd_get_domain_affinity()* and *wd_get_numa_affinity()*. 
The set is stored in *dev_mask*.

Then user application requests algorithm library with *dev_mask* to choose a 
right accelerator effiently.


## Algorithm Libraries

WarpDrive is a framework that supports multiple algorithm interfaces. We'll 
discuss compression algorithm interface in below. And crypto algorithm 
interface will be added later.


### Register Vendor Driver

Hardware accelerator registers in UACCE as a char dev. At the same time, 
hardware informations of accelerators are also exported in sysfs node. For 
example, the file path of char dev is */dev/misc/[Accel]* and hardware 
informations are in */sys/class/uacce/[Accel]/*. The same name is shared in 
both devfs and sysfs. The *Accel* is comprised of name, dash and id.

A vendor driver is the counterpart of a hardware accelerator. A vendor driver 
is the implementation of an algorithm. Because of these, algorithm library 
bind an accelerator and a vendor driver together. There're different driver 
models in algorithm library for different accelerators.

An example of binding compression device and compression driver is in below.

```
    struct wd_alg_comp {
        char *drv_name;
        char *alg_name;
        int  (*alloc_chan)(struct wd_chan *chan);
        void (*free_chan)(struct wd_chan *chan);
        int  (*init)(struct alg_comp_ctx *ctx, ...);
        void (*exit)(struct alg_comp_ctx *ctx, ...);
        ...
    };
```

The fields of *struct wd_alg_comp* are mentioned in Section Compression 
Algorithm.

Each vendor driver implements an instance of its driver model. The key field 
is *drv_name*. *drv_name* field must match *Accel name* in devfs or sysfs 
without dash and ID.

All the instances in vendor drivers must be exposed to libwd. So libwd 
maintained several lists for different driver models, such as 
*wd_comp_drv_list*, *wd_crypto_drv_list*, etc. Splitting drivers into different 
lists is just for better performance on seeking driver.

```
    struct wd_alg_comp wd_comp_drv_list[] = {
        &Accel_A,
        &Accel_B,
        ...
    };
```

*wd_comp_drv_list* is maintained in algorithm library.


### Bind Device to Vendor Driver

When user requests to use accelerator, it just sends the request to algorithm 
library. Algorithm library parses the request to choose a right accelerator 
by algorithm name and affinity value. All matched devices are listed in a 
dynamic list in below.

```
    struct wd_dev_list {
        char                dev_name[MAX_DEV_NAME_LEN];
        char                node_path[];
        char                alg_name[];
        int                 avail_chans;
        int                 id;     // ID in UACCE subsystem
        int                 mask;
        struct wd_dev_list  *list;
    };
```

| Field | Comments |
| :-- | :-- |
| *dev_name*    | Indicate the name of accelerator without UACCE ID. |
| *node_path*   | Indicate the file path of the accelerator in devfs. |
| *alg_name*    | Indicate which algorithm is supported by the accelerator. |
| *avail_chans* | Indicate how many channels are available to the accelerator. |
| *id*          | Indicate the ID in UACCE subsystem. ID is also a bit index. |
| *mask*        | Indicate the bit value that is converted from ID. |
| *list*        | Indicate to the next matched accelerator. If there's no more |
|               | accelerators, it's just NULL. |

This device list is a list with a priority on *avail_chans*. *avail_chans* 
indicates the channel resources. If there's more channels available, the 
accelerator is in a high priority to be choosen. It's based on the policy of 
resource balance.

*avail_chans* is also introduced in Section Launch Vendor Driver.

***static struct wd_dev_list \*query_dev_list(char \*alg_name)***

| Layer | Parameter | Direction | Comments |
| :-- | :-- | :-- | :-- |
| alg | *alg_name* | Input | Indicate the algorithm name. |

Return the device list if it succeds. Return NULL if it fails.

So *query_dev_list()* is an internal function of algorithm library that returns 
the matched device list.

When both device and driver and founded by algorithm library, it needs to be 
stored in somewhere. In algorithm library, these are stored in *context* that 
will be introduced in Section Context.


### Context

Context is a space that collect accelerator and driver. With context, algorithm 
library could track a task.

Context is a internal structure that only be accessed by algorithm library and 
vendor driver. User application needs the context to track the task, but it 
can't access the informations in context directly. Algorithm library converts 
the context to a handler and exposes handler to user application.

```
    struct alg_comp_ctx {
        char                *alg_name;    /* zlib or gzip */
        char                node_path[];
        int                 id;
        struct wd_dev_list  *dev_list;
        wd_dev_mask_t       affinity;
        struct wd_alg_comp  *drv;
        alg_comp_cb_t       *cb;
        void                *priv;       /* vendor specific structure */
    };
```

The fields in *struct alg_comp_ctx* are illustrated in Section Compression 
Algorithm.


### Launch Vendor Driver

With context, both accelerator and driver are stored in *node_path* and *drv* 
fields. Although driver is found, algorithm library still controls the CPU.

Algorithm library calls *drv->init()* to transfer the control of CPU to vendor 
driver.

In vendor driver, a hardware resource, channel, is important to request.

Channel is the common hardware resource of each hardware accelerator. Channels 
are managed by channel manager. When there're multiple channel managers in the 
system, different hardware accelerators may bind to different channel managers. 
*avail_chans* could denote how much channels could be accessed by the 
accelerator. When multiple accelerators share the same channel manager, 
*avail_chans* is same for these accelerators. The value of *avail_chans* could 
bet gotten from sysfs node.

Since channel is a common resource, libwd encapuslates the interface to request 
it. It's introduced in Section Libwd Helper Functions.


### Compression Algorithm

Compression algorithm is between user application and vendor driver.

```
    typedef handle_t    unsigned long long int;
```
***int wd_alg_comp_alloc_ctx(char \*alg_name, alg_comp_cb_t \*cb, 
wd_dev_mask_t affinity, handler_t \*handler)***

| Layer | Parameter | Direction | Comments |
| :-- | :-- | :-- | :-- |
| algorithm | *alg_name* | input  | Set the name of algorithm type, such as |
|           |            |        | "zlib", "gzip", etc. |
|           | *cb*       | input  | Set the application callback that used in |
|           |            |        | asynchronous operation. |
|           | *affinity* | input  | Set the mask value of UACCE devices. |
|           | *handler*  | output | A 64-bit handler value. |

*wd_alg_comp_alloc_ctx()* is used to allocate a context that is denoted by 
a handle. Return 0 if succeeds. Return error number if fails.

***void wd_alg_comp_free_ctx(handle_t handle)***

| Layer | Parameter | Direction | Comments |
| :-- | :-- | :-- | :-- |
| algorithm | *handler* | input | A 64-bit handler value. |

*wd_alg_comp_free_ctx()* is used to release a context that is denoted by a 
handle.

Both *wd_alg_comp_alloc_ctx()* and *wd_alg_comp_free_ctx()* are used by user 
applications.

```
    struct algo_comp_tag {
        int                 tag_id;
    };
    typedef void *algo_comp_cb_t(struct algo_comp_tag *tag);
```

| Field | Comments |
| :-- | :-- |
| *tag_id* | Set by user application. It's used to mark the sequence of |
|          | callback in asynchronous operation. |


```
    struct wd_alg_comp {
        char *drv_name;
        char *algo_name;
        int  (*alloc_chan)(struct wd_chan *chan);
        void (*free_chan)(struct wd_chan *chan);
        int  (*init)(struct algo_comp_ctx *ctx, ...);
        void (*exit)(struct algo_comp_ctx *ctx, ...);
        int  (*deflate)(struct algo_comp_ctx *ctx, ...);
        int  (*inflate)(struct algo_comp_ctx *ctx, ...);
        int  (*async_poll)(struct algo_comp_ctx *ctx, ...);
    };
```

| Field | Comments |
| :-- | :-- |
| *algo_name*  | Algorithm name |
| *drv_name*   | Driver name that is matched with device name. |
| *alloc_chan* | Hook to allocate hardware channel that implemented in vendor |
|              | driver. |
| *free_chan*  | Hook to free hardware channel that implemented in vendor |
|              | driver. |
| *init*       | Hook to do hardware initialization that implemented in vendor |
|              | driver. |
| *exit*       | Hook to finish hardware operation that implemented in vendor |
|              | driver. |
| *deflate*    | Hook to deflate by hardware that implemented in vendor |
|              | driver. |
| *inflate*    | Hook to inflate by hardware that implemented in vendor |
|              | driver. |
| *async_poll* | Hook to poll hardware status in asynchronous operation that |
|              | implemented in vendor driver. |

Each vendor driver needs to implement an instance of *struct wd_alg_comp*. It 
should be referenced in a driver list of algorithm library.


```
    struct alg_comp_ctx {
        char                *alg_name;    /* zlib or gzip */
        char                node_path[];
        int                 id;
        struct wd_dev_list  *dev_list;
        wd_dev_mask_t       affinity;
        struct wd_alg_comp  *drv;
        alg_comp_cb_t       *cb;
        void                *priv;       /* vendor specific structure */
    };
```

| Field | Comments |
| :-- | :-- |
| *alg_name*  | Algorithm name. |
| *node_path* | The node path of chosen hardware accelerator. |
| *id*        | Indicate the ID of accelerator in UACCE subsystem. |
| *dev_list*  | A dynamic device list is created by *query_dev_list()*. |
| *affinity*  | Mask of preferred devices. |
| *drv*       | Hook to vendor driver implementation. |
| *cb*        | It's the user application callback that used in asynchronous |
|             | operation. |

User application calls *wd_alg_comp_alloc_ctx()* to create a context. In the 
meantime, a device is chosen and a driver is matched. But of them are stored in 
the context.

Then algorithm library can call the *drv->init()*. In the *drv->init()*, vendor 
driver needs to request a channel by *wd_request_chan()*.

When user application requests for a context, it could apply hardware 
accelerator to work in synchronous mode or in asychronous mode. For synchronous 
mode, parameter *cb* should be NULL. For asynchronous mode, parameter *cb* must 
be provided.

```
    struct priv_vendor_ctx {
        void            *buf_in;
        void            *buf_out;
        size_t          in_len;
        size_t          out_len;
        __u64           phys_in;
        __u64           phys_out;
        struct wd_chan  *chan;
        ...
    };
```

| Field | Comments |
| :-- | :-- |
| *buf_in*   | Indicate virtual address of temporary IN buffer. |
| *buf_out*  | Indicate virtual address of temporary OUT buffer. |
| *in_len*   | Indicate the length of IN buffer. |
| *out_len*  | Indicate the length of OUT buffer. |
| *phys_in*  | Indicate physical address of temporary IN buffer. |
| *phys_out* | Indicate physical address of temporary OUT buffer. |

*struct priv_vendor_ctx* is an example of private structure attached in *priv* 
field of *struct wd_comp_ctx*. Vendor driver could define the structure by 
itself. And it's also bound in *init()* implementation in vendor driver.


#### Compression & Decompression


```
    struct wd_comp_arg {
        void         *src;
        size_t       src_len;
        void         *dst;
        size_t       dst_len;
    };
```

| Field | Comments |
| :-- | :-- |
| *src*     | Indicate the virtual address of source buffer that is prepared |
|           | by user application. |
| *src_len* | Indicate the length of source buffer. |
| *dst*     | Indicate the virtual address of destination buffer that is |
|           | prepared by user application. |
| *dst_len* | Indicate the length of destination buffer. |

*struct wd_comp_arg* stores the information about source and destination 
buffers. This structure is the input parameter of compression and decompression.

If user application wants to do the compression or decompression, it needs to 
functions in below.

***int wd_alg_compress(handler_t handler, struct wd_comp_arg \*arg, 
void \*(\*callback)(struct wd_comp_arg \*arg))***

| Parameter | Direction | Comments |
| :-- | :-- | :-- |
| *handler*  | Input | Indicate the context. User application doesn't know the |
|            |       | details in context. |
| *arg*      | Input | Indicate the source and destination buffer. |
| *callback* | Input | Indicate the user application callback for asynchronous |
|            |       | mode. |

Return 0 if it succeeds. Return error number if it fails.

***int wd_alg_decompress(handler_t handler, struct wd_comp_arg \*arg, 
void \*(\*callback)(struct wd_comp_arg \*arg))***

| Parameter | Direction | Comments |
| :-- | :-- | :-- |
| *handler*  | Input | Indicate the context. User application doesn't know the |
|            |       | details in context. |
| *arg*      | Input | Indicate the source and destination buffer. |
| *callback* | Input | Indicate the user application callback for asynchronous |
|            |       | mode. |

Return 0 if it succeeds. Return error number if it fails.

*callback* is the callback function of user application. And it's optional.
When it's synchronous operation, it should be always NULL.


### Asynchronous Mode

When it's in synchronous mode, user application is blocked in 
*wd_alg_compress()* or *wd_alg_decompress()* until hardware operation is 
done. When it's in asynchronous mode, user application gets return from 
*wd_alg_compress()* or *wd_alg_decompress()* immediately. But hardware 
accelerator is still running. User application could call *wd_alg_comp_poll()* 
to check whether hardware is done.

If multiple jobs are running in hardware in parallel, *wd_alg_comp_poll()* 
could save the time on polling hardware status. And user application could 
sleep for a fixed time slot before polling status, it could save CPU resources.

***int wd_alg_comp_poll(handler_t handler)***

| Parameter | Direction | Comments |
| :-- | :-- | :-- |
| *handler* | Input | Indicate the context. User application doesn't know the |
|           |       | details in context. |

Return 0 if hardware is done. Return error number if hardware is still working.

Finally, *wd_alg_comp_poll()* calls *async_poll()* in vendor driver to poll 
the hardware.

Because *wd_alg_comp_poll()* is a polling function. Suggest user application 
invokes the polling for multiple times. Otherwise, it may miss the event of
hardware accelerator completion.

*tag_id* in *struct algo_comp_tag* is used to mark the sequence for asychronous 
mode. Then user application could merge all these data pieces into a complete 
buffer.

Vendor driver could merge and copy all these pieces 
into output buffer that provided by application.

If user application wants to suggest opening devices, it could make use of 
*affinity_name_list* parameter. Each bit represents one hardware accelerator. 
Bit 0 means the first device that is registered in UACCE subsystem.


## Libwd Helper Functions

Hardware accelerator communicates with CPU by shared memory. Libwd helper 
functions provide the interface that vendor driver could access memory from 
WarpDrive.


### Channel

Channel is a dual directional hardware communication resource between hardware 
accelerator and CPU. When vendor driver wants to access memory, a channel is 
the requisite resource.

Channel is a hardware resource. UACCE creates a char dev for each registered 
hardware device. Once the char dev is opened by vendor driver, a channel is 
allocated by UACCE. *fd* (file descriptor) represents the channel that could 
be accessed by vendor driver. In libwd, channel is defined as *struct wd_chan*.

```
    struct wd_chan {
        int  fd;
        char node_path[];
        void *ctx;    // point to context in algorithm library
        void *priv;   // point to vendor specific structure
    };
```

| Field | Comments |
| :-- | :-- |
| *fd*        | Indicate the file descriptor of hardware accelerator. |
| *node_path* | Indicate the file path of hardware accelerator. |
| *ctx*       | Indicate the context that is created in algorithm library. |
| *priv*      | Indicate the vendor specific structure. |

Vendor driver must know which hardware accelerator should be accessed. From 
the view of vendor driver, the device is represented by device node path. So 
it should be recorded in *struct wd_chan*, and it's the key parameter to 
allocate a channel.

Libwd defines APIs to allocate channels.

***struct wd_chan \*wd_request_channel(char *node_path);***

| Layer | Parameter | Direction | Comments |
| :-- | :-- | :-- | :-- |
| libwd | *node_path* | Input | Indicate the file path of hardware |
|       |             |       | accelerator. |

Return the handler of channel if it succeeds. Return NULL if it fails.

***void wd_release_channel(struct wd_chan \*ch);***

| Layer | Parameter | Direction | Comments |
| :-- | :-- | :-- | :-- |
| libwd | *ch* | Input | Indicate the handler of working channel. |


### mmap

With a channel, memory could be shared between CPU and hardware accelerator. 
Libwd provides API to create the mapping between virtual address and physical 
address.

***void *wd_drv_mmap_qfr(struct wd_chan \*ch, enum uacce_qfrt qfrt, 
size_t size);***

| Layer | Parameter | Direction | Comments |
| :-- | :-- | :-- | :-- |
| libwd | *ch*   | Input | Indicate the handler of working channel. |
|       | *qfrt* | Input | Indicate the queue file region type. It could be  |
|       |        |       | MMIO (device MMIO region), DUS (device user share |
|       |        |       | region) or SS (static share memory region for |
|       |        |       | user). |
|       | *size* | Input | Indicate the size to be mapped. |

Return virtual address if it succeeds. Return NULL if it fails.

*wd_drv_mmap_qfr()* maps qfile region to user space.

***void wd_drv_unmap_qfr(struct wd_chan \*ch, void \*addr, 
enum uacce_qfrt qfrt, size_t size);*** destroys the qfile region by unmap().

| Layer | Parameter | Direction | Comments |
| :-- | :-- | :-- | :-- |
| libwd | *ch*   | Input | Indicate the handler of working channel. |
|       | *addr* | Input | Indicate the address that is mapped by |
|       |        |       | *wd_drv_mmap_qfr()*. |
|       | *qfrt* | Input | Indicate the queue file region type. It could be |
|       |        |       | MMIO (device MMIO region), DUS (device user share |
|       |        |       | region) or SS (static share memory region for |
|       |        |       | user). |
|       | *size* | Input | Indicate the size to be mapped. |

*wd_drv_unmap_qfr()* unmaps qfile region from user space.

qfrt means queue file region type. The details could be found in UACCE kernel 
patch set <https://lkml.org/lkml/2019/11/22/1728>.


### Extra Helper functions in NOSVA

***int wd_is_nosva(void);***

*wd_is_nosva* is in libwd.

Return 1 if it doesn't support SVA. Return 0 if it supports SVA.

Vendor driver needs to identify whether the current device is working in SVA 
scenario or NOSVA scenario.

Hardware always requires continuous address. Since physical address is 
allocated in kernel. Vendor driver needs libwd to provide an API to allocate 
memory in kernel space.

***void \*wd_reserve_mem(struct wd_chan \*ch, size_t size);***

| Layer | Parameter | Direction | Comments |
| :-- | :-- | :-- | :-- |
| libwd | *ch*   | Input | Indicate the working channel. |
|       | *size* | Input | Indicate the memory size that needs to be |
|       |        |       | allocated. |

Return virtual address if it succeeds to allocate physical continuous memory 
from libwd. Return NULL if it fails to allocate physical continuous memory.

*wd_reserve_mem()* not only allocates memory in kernel, but also maps the 
physical address to virtual address. Then it returns the virtual address if it 
succeeds. Actually the mapping is created by *wd_drv_mmap_qfr()*, and it's 
invoked in *wd_reserve_mem()* by default.

Since hardware accelerator needs to access memory, vendor driver needs to 
provide physical address to hardware accelerator. Libwd helps vendor driver 
to maintain the mapping between virtual address and physical address, and 
returns physical address.

***void \*wd_get_dma_from_va(struct wd_chan \*ch, void \*va);***

| Layer | Parameter | Direction | Comments |
| :-- | :-- | :-- | :-- |
| libwd | *ch* | Input | Indicate the working channel. |
|       | *va* | Input | Indicate the virtual address. |

Return physical address if it succeeds. Return NULL if the virtual address is 
invalid.


## Example

### Example in user application

Here's an example of compression in user application. User application just 
needs a few APIs to complete synchronous compression.

![comp_sync](./wd_comp_sync.png)

Synchoronous operation means polling hardware accelerator status of each 
operation. It costs too much CPU resources on polling and causes performance 
down. User application could divide the job into multiple parts. Then it 
could make use of asynchronous mechanism to save time on polling.

![comp_async](./wd_comp_async.png)

There's also a limitation on asynchronous operation in SVA scenario. Let's 
assume there're two output frames generated by accelerator, A frame and B 
frame. If the output is in fixed-length, then we can calculate the address of 
A and B frame in the output buffer of application. If the length of hardware 
accelerator output isn't fixed, we have to setup the temperary buffer to store 
A and B frame. Then a memory copy operation is required between temperary 
buffer and application buffer. So we use compression as a demo to explain 
asynchronous operation. It doesn't mean that we recommend to use asynchronous 
compression.


### Example in vendor driver

Here's an example of implementing vendor driver to support compression 
algorithm.

Vendor driver needs to implement the instance of *struct wd_alg_comp* and 
*struct wd_drv*. When user application creates the context, the vendor driver 
and device are both bound into context. Then *wd_alg_comp_alloc_ctx()* 
continues to call *ctx->init()* that defined in vendor driver. In this 
function tries to request channel and initialize hardware.

When user application invokes *wd_alg_compress()*, *ctx->deflate()* is called. 
It points to the implementation in vendor driver.

When user application invokes *wd_alg_comp_free_ctx()*, "wd_comp_ctx->exit()" 
is called. It also points to the implementation in vendor driver. It releases 
channel and free hardware.
