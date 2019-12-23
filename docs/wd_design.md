
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


## UACCE user space API

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

Vendor driver must know which hardware accelerator should be accessed. From 
the view of vendor driver, the device is represented by device node path. So 
it should be recorded in *struct wd_chan*, and it's the key parameter to 
allocate a channel.

Libwd defines APIs to allocate channels.

***struct wd_chan \*wd_request_channel(char *node_path);***

***void wd_release_channel(struct wd_chan \*ch);***


### mmap

With a channel, memory could be shared between CPU and hardware accelerator. 
Libwd provides API to create the mapping between virtual address and physical 
address.

***void *wd_drv_mmap_qfr(struct wd_chan \*ch, enum uacce_qfrt qfrt, 
size_t size);*** maps qfile region to user space. It's just fill "q->fd" 
and "q->qfrs_offset[qfrt]" into mmap().

***void wd_drv_unmap_qfr(struct wd_chan \*ch, void \*addr, 
enum uacce_qfrt qfrt, size_t size);*** destroys the qfile region by unmap().

qfrt means queue file region type. The details could be found in UACCE kernel 
patch set <https://lkml.org/lkml/2019/11/22/1728>.


### Extra Helper functions in NOSVA

***int wd_is_nosva(struct wd_chan \*chan);***

Vendor driver needs to identify whether the current device is working in SVA 
scenario or NOSVA scenario.

Hardware always requires continuous address. Since physical address is 
allocated in kernel. Vendor driver needs libwd to provide an API to allocate 
memory in kernel space.

***void \*wd_reserve_mem(struct wd_chan \*ch, size_t size);***

*wd_reserve_mem()* not only allocates memory in kernel, but also maps the 
physical address to virtual address. Then it returns the virtual address if it 
succeeds. Actually the mapping is created by *wd_drv_mmap_qfr()*, and it's 
invoked in *wd_reserve_mem()* by default.

Since hardware accelerator needs to access memory, vendor driver needs to 
provide physical address to hardware accelerator. Libwd helps vendor driver 
to maintain the mapping between virtual address and physical address, and 
returns physical address.

***void \*wd_get_dma_from_va(struct wd_chan \*ch, void \*va);***


### Register Vendor Driver

Hardware accelerator registers in UACCE as a char dev. At the same time, 
hardware informations of accelerators are also exported in sysfs node. For 
example, the file path of char dev is */dev/misc/[Accel]* and hardware 
informations are in */sys/class/uacce/[Accel]/*. The same name is shared in 
both devfs and sysfs. The *Accel* is comprised of name, dash and id.

```
    struct wd_drv {
        char     drv_name[MAX_DEV_NAME_LEN];
        int      (\*alloc_chan)(struct wd_chan \*chan);
        void     (\*free_chan)(struct wd_chan \*chan);
    };
```

Each vendor driver implements an instance of *struct wd_drv*. *drv_name* 
field should match *Accel name* in devfs or sysfs without dash and id. And 
all the instances are exposed to libwd and are stored in the list named 
*wd_drv_list*. The type of entry in *wd_drv_list* is *struct wd_drv*.

```
    struct wd_drv wd_drv_list[] = {
        &Accel_A,
        &Accel_B,
        ...
    };
```

When algorithm libraries want to make hardware accelerator working, they send 
requests to libwd first. Libwd matches the algorithm type name, and gets a 
matched device list.

```
    struct wd_dev_list {
        char                dev_name[MAX_DEV_NAME_LEN];
        char                node_path[];
        char                algo_name[];
        int                 avail_chans;
        int                 mask;
        int                 id;     // ID in UACCE subsystem
        struct wd_dev_list  *list;
    };
```

***static struct wd_dev_list \*query_dev_list(char \*alg_name)***

So *query_dev_list()* is an internal function of libwd that returns the matched 
device list. And the list should be stored in the *context* of algorithm 
library. *context* will be illustrated in other section.

Channel is the common hardware resource of each hardware accelerator. Channels 
are managed by channel manager. When there're multiple channel managers in the 
system, different hardware accelerators may bind to different channel managers. 
*avail_chans* could denote how much channels could be accessed by the 
accelerator. When multiple accelerators share the same channel manager, 
*avail_chans* is same for these accelerators. The device list is a priority 
list that is sorted by the number of *avail_chans* in descending order.

When a device is selected by algorithm library, the related driver could be 
matched from *wd_drv_list* in libwd. Both of the device and the driver will 
be recorded in the *context* of algorithm library. It'll be illustrated in 
other sections.

From now on, channel could be allocated from libwd. When the *context* is 
created, vendor driver is also involved to request channel.


## Libaffinity

### Libaffinity APIs

Although algorithm library could get the matched device list by 
*query_dev_list()*, user application may still want to bind a specific device. 
Because the user application may understand the system well. There should be 
an optimization interface to choose right device with higher priority.

```
    typedef wd_dev_mask_t   unsigned long long int;
    wd_dev_mask_t           dev_mask;
```

When hardware accelerator is registered into UACCE, each of them gets an ID 
starting from 0. So a new type named wd_dev_mask_t is defined to account all 
accelerators in UACCE subsystem. All set bits mean preferred devices. All clear 
bits mean disabled devices.

***int wd_get_affinity(int pid, wd_dev_mask_t \*dev_mask);***

Parameter *dev_mask* is the output parameter of *wd_get_affinity()*. This 
function returns all available devices for specific algorithm.

***int wd_set_affinity(int pid, wd_dev_mask_t \*dev_mask);***

Parameter *dev_mask* is the input parameter of *wd_set_affinity()*. This 
function sets all preferred devices for specific algorithm.

If libaffinity is used by user application, only device is in both matched 
list and affinity mask could be selected. The first device with affinity in 
matched list is valid. If libaffinity isn't used by user application, the first 
device in matched device list is valid. Affinity is also stored in *context* 
that will be illustrated in Section *context*. Parameter *pid* isn't used. It's 
reserved for future that is illustred in Section *Extension on libaffinity*.

Libaffinity is focus on selecting right devices. It could add more policies 
to adjust affinity on hardware accelerators. User application calls libaffinity 
directly.


## Algorithm Libraries

WarpDrive is a framework that supports multiple algorithm interfaces. We'll 
discuss compression algorithm interface in below. And crypto algorithm 
interface will be added later.


### Compression Algorithm Interface

WarpDrive compression algorithm interface is between user application and 
vendor driver.


#### Context

The hardware accelerator is shared in the system. When compression is ongoing, 
WarpDrive needs to track the current task. So a context is necessary to record 
key informations. User application needs to request a context before 
compression or decompression.

```
    struct wd_algo_comp {
        char *algo_name;
        char *drv_name;
        int (*init)(...);
        void (*exit)(...);
        int (*deflate)(...);
        int (*inflate)(...);
    };

    struct algo_comp_ctx {
        char                *algo_name;    /* zlib or gzip */
        char                node_path[];
        struct wd_drv       *drv;
        struct wd_dev_list  *dev_list;
        wd_dev_mask_t       affinity;
        
        int                 running_num; /* number of async running task */
        
        struct wd_algo_comp *hw;
        algo_comp_cb_t      *cb;
        void                *priv;       /* vendor specific structure */
    };

    struct algo_comp_tag {
        int                 tag_id;
    };
    
    typedef void *algo_comp_cb_t(struct algo_comp_tag *tag);
```

***struct algo_comp_ctx \*wd_algo_comp_alloc_ctx(char \*algo_name, 
algo_comp_cb_t \*cb, wd_dev_mask_t affinity)***

User application calls *wd_algo_comp_alloc_ctx()* to create context. In the 
meantime, related device and driver is choosed and stored in the context. 
Parameter *cb* is optional, and it saved in *cb* field. This parameter means 
the callback of asychronous operation. If appliction only needs synchronous 
operation, parameter *cb* could be **NULL**. Parameter *affinity_name_list* 
is optional, and it saved in *affinity* field.

When a device is chosen, the device node path is updated in 
*struct algo_comp_ctx*. And the node path is used by *wd_request_chan()*.

Hooks are provided in "struct wd_algo_comp". When a vendor driver is choosed, 
*hw* hooks should be filled at the same time.

When asychronous operation is running, *tag_id* in *struct algo_comp_tag* is 
used to mark the sequence. Vendor driver could merge and copy all these pieces 
into output buffer that provided by application.

```
    struct priv_vendor_ctx {
        void            *buf_in;
        void            *buf_out;
        size_t          in_len;
        size_t          out_len;
        __u64           phys_buf_in;
        __u64           phys_buf_out;
        struct wd_chan  *chan;
        ...
    };
```

*struct priv_vendor_ctx* is an example of private structure attached in *priv*
field of *struct wd_comp_ctx*. Vendor driver could define the structure by 
itself.


***void wd_algo_comp_free_ctx(struct wd_comp_ctx \*ctx)*** releases a context.


#### Compression & Decompression


```
    struct wd_comp_arg {
        void         *src;
        size_t       src_len;
        void         *dst;
        size_t       dst_len;
    };
```

*struct wd_comp_arg* stores the information about source and destination 
buffers. This structure is the input parameter of compression and decompression.

If user application needs the synchronous compression or decompression, 
functions are used in below.

***wd_algo_compress(struct wd_comp_ctx \*ctx, struct wd_comp_arg \*arg, 
void \*(\*callback)(struct wd_comp_arg \*arg))***

***wd_algo_decompress(struct wd_comp_ctx \*ctx, struct wd_comp_arg \*arg, 
void \*(\*callback)(struct wd_comp_arg \*arg))***

*callback* is the callback function of user application. And it's optional.
When it's synchronous operation, it should be always NULL.

When it's asynchronous operation, *callback* function of user application is 
called. When the above two functions are invoked, user application is blocked 
until the hardware operation is done.

If user application wants to handle more tasks in parallel, asynchronous 
operations are used in below.

***wd_algo_comp_poll(struct wd_comp_ctx \*ctx)***

In asnychronous mode, user application sends multiple compression requests 
to hardware accelerators by one channel. Data are pipelined to hardware 
accelerator, application only needs to poll the status of hardware accelerator 
after all requests sent. It saves the time of polling status between two 
requests. It only needs to poll status after all requests sent.

Because *wd_algo_comp_poll()* is a polling function. Suggest user application 
invokes the polling for multiple times. Otherwise, it may miss the event of
hardware accelerator completion.


#### Register Compression Algorithm

When user application executes compression or decompression, the hardware 
implementation that defined in "hw" field of the context is invoked.

Compression algorithm library maintains a list of *struct wd_algo_comp*. 

When user application creates context by *wd_algo_comp_alloc_context()*, 
these operations will be done. Both the device and the driver are chosen 
and updated into context. At the same time, algorithm library searches the 
list of *struct wd_algo_comp* with *drv_name*. When it's done, implementation 
of *struct wd_algo_comp* is assigned to *hw* hooks in context. Everything 
is integrated into context now.


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

Vendor driver needs to implement the instance of *struct wd_algo_comp* and 
*struct wd_drv*. When user application creates the context, the vendor driver 
and device are both bound into context. Then *wd_algo_comp_alloc_ctx()* 
continues to call *ctx->init()* that defined in vendor driver. In this 
function tries to request channel and initialize hardware.

When user application invokes *wd_algo_compress()*, *ctx->deflate()* is called. 
It points to the implementation in vendor driver.

When user application invokes *wd_algo_comp_free_ctx()*, "wd_comp_ctx->exit()" 
is called. It also points to the implementation in vendor driver. It releases 
channel and free hardware.
