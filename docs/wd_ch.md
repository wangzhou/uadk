api

1 Overview 
1.1目的 
Warpdrive是一个通用的加速器用户态解决方案。这个方案基于uacce内核模块，以及Linux的SVA技 术。Warpdrive提供一套用户态库，需要使用硬件加速的用户调用相关API完成所需要的功能。本文是这 些API是使用手册。

1.2海思Hi16xx加速器抽象模型 
Hi16xx SoC把硬件加速能力实现到了几个PCI设备中，他们是ZIP, SEC, HPRE控制器。每个控制器有一个PF和若干VF。 每个PF、VF上有若干硬件队列，这些队列是软硬件交互的接口。软件封装相应的硬件加速请 求，发送给硬件；软件通过硬件队列接收完成的请求。

各个加速器PF、VF通 过自身驱动，以及uacce驱动暴露到用户态。一个PF或者VF对 应一个字符设备（例如：/dev/hisi_zip-0），以及相关的属性文件（例如：/sys/class/uacce/hisi_zip-0/*）。

为了方便用户使用，我们提供 一组用户态库，这组用户态封装字符设备，属性文件，以及具体算法业务的细节。用户通过用户态库的API使用硬件加速器。这一组库包 括，libwd，libhisi_qm, libwd_comp, libwd_cipher, libwd_digest, libwd_aead, libwd_dh, libwd_rsa, libwd_ecc。除了libhisi_qm的接口在内部使用，其他库的API均是最终用户接口。Libwd的API和 通道操作相关，和具体的算法业务无关。其他算法库接口是具体算法的API。

用户可以直接使用libwd API编写业务驱动，使用加速器。 用户也可以使用算法库接口使用加速器，算法库接口只支持SVA。

##Libwd Helper Functions

相关API均在warpdrive/include/wd.h中体现

加速器在UACCE的框架下，注册为字符设备。用户在通过libwd使用加速器时，需要直接提供该字符设备的设备路径，以此来申请和使用加速器的硬件资源。libwd_comp之类的算法库是实现对某种算法的抽象，用户在使用这类接口时，无须再提供具体的字符设备路径；这些操作已被集成至算法接口中。


context是和加速器硬件通讯的上下文。用户通过mmap映射加速器硬件的地址空间，来
进行对加速器硬件的访问。

通常加速器会提供多个可供通讯的context。通过libwd，用户可以申请硬件资源，获
得的context就是对该硬件资源的抽象描述。从软件上看，context也是一种数据结构。
libwd的目的是减少用户的负担，因此context的具体定义实现，并不需要暴露给用户
，用户使用handle来表示context。

```
type unsigned long long int	handle_t
```

handle就是属于handle_t类型。


###申请和释放加速器设备的通讯上下文


函数原型
```
handle_t wd_request_ctx(char *node_path);
```

函数功能
获取加速器设备上的通迅上下文

输入说明
node_path：加速器PF/VF对应的字符设备路径

输出说明
N/A

返回值说明
0：申请通讯上下文失败；
其他值：申请通讯上下文成功，获得该context的句柄。

使用说明
wd_requst_ctx()支持多线程操作。由于context是一种数目有限的硬件资源，在多线
程的场景下，该context会成为多线程的共享硬件资源，context内部存在使用计数。



函数原型
```
void wd_release_ctx(handle_t h_ctx);
```

函数功能
释放加速器设备的通讯上下文

输入说明
加速器设备的上下文句柄

输出说明
N/A

返回值说明
N/A

使用说明
wd_release_ctx()支持多线程操作。当context内部使用计数清零时，其将会被释放。



###获取通道mmio，dus地址 


函数原型
```
void *wd_drv_mmap_qfr(handle_t h_ctx, enum uacce_qfrt qfrt);
```

函数功能

获取通道mmio, dus地址 

输入说明
h_ctx: 通道上下文句柄
qfrt：通道相关的操作区域的类型 

输出说明
通道相关的操作区域的基地址

返回值说明
通道可以关联寄存器或者队列地址。这些地址又内核驱动配置给一个通道。Uacce里一
般有MMIO和DUS类型的通道。这个函数或者相关类型区域的基地址。

使用说明
enum uacce_qfrt的定义在uacce的内核uapi头文件中：include/uapi/misc/uacce/uacce.h

注意事项


2.8 释放通道mmio，dus地址映射 




函数原型
void wd_drv_unmap_qfr(handle_t h_ctx, enum uacce_qfrt qfrt);

函数功能
释放通道mmio, dus地址

输入说明
h_ctx: 通道上下文句柄；qfrt：通道 相关的操作区域的类型

输出说明
N/A

返回值说明
NA

使用说明
NA

注意事项


###开启加速器设备的通迅上下文 


函数原型
```
int wd_ctx_start(handle_t h_ctx);
```

函数功能
启动加速器设备的通讯上下文。在获得context的mmio，dbus等资源后，即可启动
context操作。

输入说明
通迅上下文句柄

输出说明
N/A

返回值说明
0：SUCCESS
负值：启动context失败

使用说明
N/A


函数原型
```
int wd_ctx_stop(handle_t h_ctx);
```

函数功能
停止通道传输，而且释放通道对应的硬件资源

输入说明
通道上下文句柄

输出说明
返回成功失败指示

返回值说明
0：SUCCESS；
其他负值：FAIL。

使用说明
这个函数可以立即释放硬件队列资源，避免在内核fd release流程中延迟释放硬件队列资源

注意事项


###获取通道api字符串 


函数原型
```
char *wd_ctx_get_api(handle_t h_ctx);
```

函数功能
获取通道API字符串

输入说明
通道上下文句柄

输出说明
通道API字符串

返回值说明
通道API字符串

使用说明
这个API得到的是sysfs中api的字符串，这个字符串由内核设备驱动添加，用来表示
注册到uacce子系统上的外设的版本号

注意事项


###获取加速器设备列表 


函数原型
```
struct uacce_dev_list *wd_list_accels(char *alg_name);
```

函数功能
获取加速器设备列表

输入说明
alg_name：加速器支持的算法名称

输出说明
N/A

返回值说明
struct uacce_dev_list表示获取的加速器设备链表；若是没有找到符合要求的设备，
返回NULL。

使用说明
NA

注意事项


2.11 获取加速器设备的算法名 


函数原型
char *wd_get_accel_name(char *node_path, int no_apdx);

函数功能
获取加速器设备的算法名

输入说明
node_path: 加速器字符设备路径，no_apdx: ??

输出说明
算法名字符串

返回值说明
如上输出说明

使用说明
NA

注意事项


2.12 清除设备掩码中的设备 




函数原型
int wd_clear_mask(wd_dev_mask_t *dev_mask, int idx);

函数功能
清楚设备掩码中的设备bit

输入说明
dev_mask: 设备掩码，idx：要删 除的设备对应的bit

输出说明
返回成功失败指示

返回值说明
0：SUCCESS；
其他负值：FAIL

使用说明
NA

注意事项


2.13 获取加速器设备可用ctx个数 


函数原型
int wd_ctx_get_avail_ctx(char *node_path);

函数功能
获取加速器可以用ctx个数

输入说明
加速器字符设备

输出说明
返回加速器可用ctx个数

返回值说明
同上

使用说明
NA

注意事项


2.14 获取ctx用户私有数据的指针 




函数原型
void *wd_ctx_get_priv(handle_t h_ctx);

函数功能
获取ctx用户私有数据的指针

输入说明
通道上下文句柄

输出说明
用户私有数据的指针

返回值说明
同上

使用说明
用户可以通过如下：wd_ctx_set_priv(handle_t h_ctx, void *priv)接口设置ctx内部的指针。用户可以通过该API获取这个指针。

注意事项


2.15 设置ctx用户私有数据的指针 




函数原型
void wd_ctx_set_priv(handle_t h_ctx, void *priv);

函数功能
设置ctx用户私有数据的指针

输入说明
h_ctx：通道上下文句柄；
priv：用户 数据指针

输出说明
NA

返回值说明
NA

使用说明
NA

注意事项


2.16 向ctx发送用户自定义的IO命令 




函数原型
int wd_ctx_set_io_cmd(handle_t h_ctx, int cmd, void *arg);

函数功能

向ctx发送用户自定义的IO命令 
输入说明
h_ctx：通道上下文句柄；cmd：要发 送的命令；arg: cmd命令的返回值

输出说明
命令返回的数据存在arg指向的内存里。

返回值说明
函数返回值表示IO命令执行的成功或者失败。
0：SUCCESS；其他负值：FAIL

使用说明
该API封装ctx对应的fd的ioctl操作。cmd就是对应的ioctlcommand，arg是ioctl从内核读数据要填入的地址。

注意事项


2.17 释放加速器设备列表 




函数原型
void wd_free_list_accels(struct uacce_dev_list *list);

函数功能
释放加速器设备列表 
输入说明
加速器设备列表

输出说明
NA

返回值说明
NA

使用说明
调用wd_list_accels(wd_dev_mask_t *dev_mask)得到的加速器设备列表可以用该函数释放。

注意事项


3 Libhisi_qm API描述
 3.1队列申请接口 




函数原型
handle_t  hisi_qm_alloc_qp(char *node_path, struct hisi_qp_cfg *cfg)

函数功能
3.2同步队列申请接口 
输入说明
node_path: 加速器字符设备
cfg: qp的配置项：
struct hisi_qp_cfg {
  __u16 sqe_size;   // sqe的个数，大于0
  __u16 op_type;   // 操作类型：0or1 
enum hisi_qm_mode mode; 
};

enum hisi_qm_mode {
  QP_SYNC,
  QP_ASYN,
};

输出说明
返回申请到的qp的句柄h_qp

返回值说明
正常如上，NULL：该函数执行失败

使用说明
NA

注意事项


3.2队列释放接口 




函数原型
void hisi_qm_free_qp(handle_t h_qp)

函数功能
释放同步队列 
输入说明
要释放的同步队列 的句柄

输出说明
NA

返回值说明
NA

使用说明
NA

注意事项


3.3同步发送接口 




函数原型
int hisi_qm_send(handle_t h_qp, void *req, int num) 

函数功能
向对应的qp发送num个请求 
输入说明
qp: 同步队列指针；req: 请求指针；num：请求 个数

输出说明
返回成功发送请求的个数

返回值说明
同上

使用说明
NA

注意事项


3.4同步接收接口 




函数原型
int hisi_qm_recv(handle_t h_qp, void *rsp, int num)

函数功能
从qp上接收一个完成请求 
输入说明
qp: 同步队列指针；rsp: 接收响应的指针， num：读取 响应的个数

输出说明
NA

返回值说明
NA

使用说明
NA

注意事项


3.5异步发送接口 




函数原型
int hisi_qm_send_async(handle_t h_qp, void *req, hisi_qm_async_data *data)

函数功能
向qp发送一个异步请求 
输入说明
qp: 同步队列指针；req: 请求指针；callback：用户回调函数
struct hisi_qm_async_data {
  void (*callback)(void *para, void *rsp);
  void *para;  /* 算法驱动自定义参数 */
  __u16 len;   /* 算法驱动自定义参数的长度 */
  void *rsp;   /* 从sqe收 到的rsp消息指针 */
};

输出说明
NA

返回值说明
发送成功返回0， 如果SQ满了，返回-EBUSY

使用说明
算法驱动在调用 时，需要告诉QM其数据处理的回 调函数，在异步队列轮询到相应的数据后使用回调。其中para参数，qm会保存数据在qm本地，在polling回调时作为入参使 用，回调处理完成后，释放相关para内存。

注意事项


3.6异步队列轮询接口 




函数原型
int hisi_qm_poll_async_qp(handle_t h_qp, __u32 num)

函数功能

轮询异步队列里完成的请求，并执行请求的回调函 数 
输入说明
qp：异步队列；num：需要 轮询的完成请求个数

输出说明
返回实际轮询到的请求的个数

返回值说明
如上

使用说明
如果硬件实际处 理的请求个数 <  num, 则按实际的请求 个数处理

注意事项


3.7Hisi SGL硬件描述符池创建、销毁接口 
3.8获取、释放Hisi SGL硬件描述符接口 

4算法库API描述

4.1Wd_comp API描述

wd_comp库通过接口获得硬件资源，但是并不直接使用context，因为算法可能不局限
于在一个硬件资源上。

算法库使用的接口是session，在一个session内含有一个或多个context信息，同时
session内还包含有用户驱动的信息。暴露给用户的是session的handle，而不是
session本身。

wd_comp算法库可以帮用户节省了对context的操作，但是并不完全把context对用户屏
蔽。用户可以感知context，分配不同的任务给不同的context。这时就需要用户提供
所需context的数目，并且以掩码的形式告知用户；用户在使用后继的接口操作时，也
同样以session加context掩码为参数，这样用户程序可以感知到具体context，而且免
除了对context的具体操作。

```
struct wd_context_set {
	unsigned char *set;
	int len;		// set的字节长度
	unsigned int magic;	// 对set的一个保护，防止用户无意送入非法的set
};

typedef struct wd_context_set    wd_ctx_set_t;
```

在这里设定所有的硬件资源从0开始按照增序排序，set中的每一个bit对应一个硬件资
源。该bit为1时，表示硬件资源可用。


```
struct uacce_dev_info {
	/* sysfs node content */
	int          flags;
	char        api[WD_NAME_SIZE];
	char        algs[MAX_ATTR_STR_SIZE];
	unsigned long qfrs_offs[UACCE_QFRT_MAX];

	char        name[WD_NAME_SIZE];
	char        alg_path[PATH_STR_SIZE];
	char        dev_root[PATH_STR_SIZE];

	int          node_id;
};
```

struct uacce_dev_info表示一个加速器设备，其中的各个域段是这个设备的属性。
flags：sysfs flags为1表 示系统工作在SVA下；
api：sysfs api，表示对应硬件的版本号；
algs：sysfs algorithms，表示该设备支持的算法种类；
qfrs_offs: sysfs qfrs_offs, 表示该设备队列的queue file region的大小

name：？
alg_path：？
dev_root：字符设备的路径
node_id：字符设备的编号。比如，hisi_zip-0就是0。

```
struct uacce_dev_list {
	struct uacce_dev_info    *info;
	struct uacce_dev_list      *next;
};
```
表 示加速器字符设备链表中的一个设备节点，用这样的数据结构表示系统中的加速器字符设备链表。

4.1.1 相关数据结构 

enum wd_comp_alg_type {
  WD_ZLIB,            /* ZLIB alg */
  WD_GZIP,            /* GZIP alg */
WD_DEFLATE_RAW,        /* deflate raw alg */
};

enum wd_comp_op_type {
  WD_SYNC,          /* Synchronous Compression session */
  WD_ ASYNC,   /* Asynchronous Compression session */
};

enum wd_comp_dir_type {
  WD_DIR_COMPRESS,         /* session for compression */
  WD_ DIR_DECOMPRESS,  /* session for decompression */
};

enum wd_comp_win_type {
  WD_COMP_WS_4K， /* 4k bytes window size */
WD_COMP_WS_8K， /* 8k bytes window size */
WD_COMP_WS_16K，   /* 16k bytes window size */ 
WD_COMP_WS_24K，   /* 24k bytes window size */
WD_COMP_WS_32K，   /* 32k bytes window size */
};

enum wd_comp_result {
  WD_COMP_OK = 0,
WD_COMP_PARMS = -1,
  WD_DATA_ERR = -2,
  WD_COMP_OUT _NOSPACE = -3,
};

struct wd_comp_arg {
  void               *src;
  size_t             src_len;       /* src_size */
  void               *dst;
  size_t          *dst_len;
  wd_alg_comp_cb_t *cb;      /* only for async mode*/
  void               *cb_param;     /* only for async mode*/
};
描述一个压缩请求。其中src是待压缩、解压数据的地址，src_len是待压缩、解压数据的长度，dst是输出数据地址，dst_len是输出数据buf的长度

struct wd_comp_strm {
  void               *src;
  size_t             *src_len;   /* src_size, after call denote bytes consumed by the operation */
  void               *dst;
  size_t          *dst_len;   /* out_space_size, after call denote bytes output by the operation */
size_t        last;        /* 1: no more data, end;  0: more data */
};
流模式压缩解压时，用来存储一条流每一次压缩解压缩的数据,包含请求及实际消耗。

创建sess， 需要用户输入的setup参数。
  Struct wd_comp_sess_setup {
  __u8 alg_type;       /* wd_comp_alg_type */
  __u8 op_type;        /* wd_comp_op_type */
__u8 dir_type;       /* wd_comp_dir_type */
  __u8 comp_lv;       /* compression level：1 .. 22 */
  __u16 win_size;     /* compression window size */
__u16 data_fmt;     /* buffer format：buffer or sgl */
}; 


4.1.2 获取一个压缩解压缩会话上下文 




函数原型
void *wd_comp_alloc_sess(struct wd_comp_sess_setup *setup, wd_dev_mask_t *dev_mask);

函数功能
获取一个压缩解压会话上下文

输入说明
setup: 申请上下文所需要的参数；dev_mask: 加速器设备掩码

输出说明
申请到的压缩解压会话上下文；NULL表示FAIL

返回值说明
如上

使用说明
支持多线程调用。setup里的op_type表示申请一个同步sess还是异 步sess。

注意事项


4.1.3 释放压缩解压缩会话上下文 




函数原型
void wd_comp_free_sess(void *sess);

函数功能
释放压缩解压缩会话上下文 
输入说明
sess: 压缩解压会话上下文

输出说明
NA

返回值说明
NA

使用说明
支持多线程调用

注意事项


4.1.4 发送一个块压缩任务 




函数原型
int wd_compress(void *sess, struct wd_comp_arg *arg);

函数功能
发送一个块压缩任务 
输入说明
sess: 压缩解压会话上下文；arg: 描述一个压缩请求

输出说明
0：SUCCESS；其他负值：FAIL

返回值说明
如上

使用说明
支持多线程调用。支持>0任意size的buffer压缩。

注意事项


4.1.5 发送一个块解压缩任务 




函数原型
int wd_decompress(void *sess, struct wd_comp_arg *arg);

函数功能
发送一个块解压缩任务 
输入说明
sess: 压缩解压会话上下文；arg: 描述一个解压请求

输出说明
0：SUCCESS；其他负值：FAIL

返回值说明
如上

使用说明
支持多线程调用. 支持>0任意size的buffer解压缩。

注意事项


4.1.6 发送一个流压缩任务 




函数原型
int wd_strm_compress(void *sess, struct wd_comp_strm *strm);

函数功能
发送一个流压缩任务 
输入说明
sess: 压缩解压会话上下文；stm：描述 一个流模式的请求

输出说明
0：SUCCESS；其他负值：FAIL

返回值说明
如上

使用说明
支持多线程调用

注意事项


4.1.7 发送一个流解压缩任务 




函数原型
int wd_strm_decompress(void *sess, struct wd_comp_strm *strm);

函数功能
发送一个流解压缩任务

输入说明
sess: 压缩解压会话上下文；stm：描述 一个流模式的请求

输出说明
0：SUCCESS；其他负值：FAIL

返回值说明
如上

使用说明
支持多线程调用

注意事项


4.1.8 轮询异步压缩解压任务 




函数原型
```
void wd_comp_poll(void \*sess, __u32 count); 
```

函数功能
异步模式下，任务轮询

输入说明
会话上下文句柄。Poll请求个数

输出说明
NULL

返回值说明
NULL

使用说明
轮询一个进程里所有的队列，执行完成任务的回调函数

注意事项


###Poll Example

这里的例子是所有异步发送的数据包，都放在一个线程上进行polling。

```
	Example: POLL-1

	/* poll all packets on single context */
	for (i = 0, async_cached = 0; i < nums; i++) {
		ret = wd_comp_async(h_sess, input, callback, ...);
		if (ret == -EBUSY) {
			ret = wd_comp_poll(async_cached, timeout, ...);
			if (ret < 0)
				return ret;
			async_cached = 0;
		} else if (ret < 0) {
			/* error case */
			return ret;
		} else if (ret == 0) {
			async_cached++;
		}
	}
	if (async_cached) {
		ret = wd_comp_poll(async_cached, timeout, ...);
		if (ret < 0)
			return ret;
	}
```

在POLL-1的例子，*wd_comp_async()*是被直接调用，并且不等加速器处理完成，直接返回
然后继续发送下一个数据包。这些数据包在算法层被放置在缓存中，然后依次下发到加速
器硬件通道中。这样的一个缓存队列，可以让加速器完成一次完成一组包的操作，能尽可
能的加大硬件的数据吞吐量来提高性能。

在POLL-1的例子中也存在一些问题。虽然是异步接口，但是用户程序是按照同步的方法在
使用。当缓存队列满后，用户程序只能使用*wd_comp_poll()*来从加速器上取数据包，这
种busy-loop的方式，将会导致加速器的I/O性能不够好，没有充分发挥出异步接口的特点。

```
	Example: POLL-2

	#define POLL_MAX		20

	/* send_async_usr() runs on Thread A.
	 * Thread A needs to inform Thread C that *nums* packets are sending.
	 */
	send_async_usr(handle_t h_sess, void (*callback)())
	{
		/* *nums* and *input* may be input parameters of
		 * send_async_usr(), or be generated in send_async_usr().
		 * It depends on user.
		 */
		for (i = 0; i < nums; i++) {
			ret = wd_comp_async(h_sess, input, callback, ...);
			if (ret == -EBUSY) {
				wd_comp_wait(h_sess);
			}
		}
	}

	/* poll_usr() runs on Thread C
	 * Parameter *nums* is coming from Thread A.
	 * Returns *nums* if SUCCEED.
	 * Returns negative value if FAIL.
	 */
	int poll_usr(int nums, unsigned int timeout)
	{
		retry = 0;
		left = nums;

		while ((left) && (retry < POLL_MAX)) {
			ret = wd_comp_poll(left, timeout, ...);
			if (ret > 0) {
				left = left - ret;
				retry = 0;
				wd_comp_wakeup(h_sess);
			} else if (ret == 0) {
				/* Can't poll any packet. */
				usleep(10);
				ret = -EFAULT;
				retry++;
			} else if (ret == -ETIME) {
				/* Timeout */
				usleep(10);
				retry++;
			} else {
				break;
			}
		}
		if ((retry == POLL_MAX) && ((ret < 0) && (ret != -ETIME))) {
			printf("Failed to poll any packet from sess.\n");
			return ret;
		}
		return nums;
	}
```

在POLL-2的例子中，一共有两个线程，分别是线程A和线程C。线程A会不断地往加速器上
放数据包。由于算法库提供的缓存有限，当缓存满后，线程A无法进一步放置更多的数据
包，只有等加速器处理完这些数据包，并且用户程序从加速器中取出这些数据包的结果，
加速器的缓存才能重新开始接受新的数据包。为了防止缓存满后，用户使用busy-loop的
方式来poll，这里在线程A上引入了*wd_wait()*。*wd_wait()*将让线程A进入睡眠模式，
只有线程C通过*wd_comp_poll()*将数据包从加速器中取出时，通过*wd_comp_wakeup()*
来唤醒线程A，才能继续让线程A发送数据。当*wd_poll()*没有获取到处理过后的数据包
，只能使用*usleep()*来等待硬件，尽最大可能减小因为busy-loop导致的性能损耗。

当用户程序需要使用多线程向加速器发送数据包时，为了节省加速器的通道资源，就会让
多个session公用一个加速器的通道。如果多个session使用的数据包大小大约一致时，无
论哪个线程的数据包先被处理，都不会影响总体性能。当线程A使用大的数据包，线程B使
用小数据包时，如果大包在缓存队列的前面，小包在缓存队列后面，那么线程B的性能会
因为线程A而受到影响。因此用户可以使用多个通道（context）资源，来降低对性能的影
响。

```
	Example: POLL-3

	/* send_async_usr() runs on Thread A.
	 * Thread A needs to inform Thread C that *nums* packets are sending.
	 */
	send_async_usr(handle_t h_sess, void (*callback)(), wd_ctx_set_t set)
	{
		/* *nums* and *input* may be input parameters of
		 * send_async_usr(), or be generated in send_async_usr().
		 * It depends on user.
		 */
		for (i = 0; i < nums; i++) {
			ret = wd_comp_async(h_sess, input, callback, set);
			if (ret == -EBUSY) {
				wd_comp_wait(h_sess);
			}
		}
	}

	/* poll_usr() runs on Thread C
	 * Parameter *nums* is coming from Thread A and Thread B.
	 * Returns *nums* if SUCCEED.
	 * Returns negative value if FAIL.
	 */
	int poll_usr(int nums, unsigned int timeout, wd_ctx_set_t set_c)
	{
		int retry = 0, ret = 0;
		int left = nums;

		while ((left) && (retry < POLL_MAX)) {
			ret = wd_comp_poll(left, timeout, set_c);
			if (ret > 0) {
				left = left - ret;
				retry = 0;
				wd_comp_wakeup(set_c);
			} else if (ret == 0) {
				/* Can't poll any packet. */
				usleep(10);
				ret = -EFAULT;
				retry++;
			} else if (ret == -ETIME) {
				/* Timeout */
				usleep(10);
				retry++;
			} else {
				break;
			}
		}
		if ((retry == POLL_MAX) && ((ret < 0) && (ret != -ETIME))) {
			printf("Failed to poll any packet from sess.\n");
			return ret;
		}
		return nums;
	}

	/* Thread A: send_async_usr(h_sess_a, input, callback, set_a);
	 * Thread B: send_async_usr(h_sess_b, input, callback, set_b);
	 * Thread C: poll_usr(nums, timeout, set_c);
	 * set_c equals to set_a OR set_b.
	 */
```
