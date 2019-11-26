Warpdrive architecture design
=============================

Revision
--------
- Revision 0.1 2019.11.26 Sherlock init

1. Introduction
---------------
 Warpdrive is an accelerator software architecture to help vendors to use their
 hardware accelerator in user space easily and efficiently. It includes a kernel
 driver named UACCE and a user space libary named libwd.

 libwd provides a wrapper of basic UACCE user space interfaces, they will be
 a set of help functions. And libwd offers a set of APIs based on specific
 algorithms to users, who could use this set of APIs to do specific task without
 known low level implementations. libwd offers a register interface to let
 hardware vendors to register their own user space driver, which could use above
 help functions to do UACCE related work.

 The key point of Warpdrive is that it is based on Linux SVA technology, which
 make device in user space to share a same virtual address space with CPU.

 This document focuses on the design of libwd.

2. UACCE user space API
-----------------------

 As the kernel driver of warpdrive, UACCE offers a set of API between kernel
 and user space.

 As UACCE driver is still under upstreaming, latest APIs of UACCE can be found
 in: https://lkml.org/lkml/2019/11/22/1728. Documents "uacce.rst" and
 "sysfs-driver-uacce" are introduced in this patchset. And a UACCE design
 document also can be found in:
 https://github.com/hisilicon/dev-docs/tree/master/warpdrive/wd_design.rst
 (private currently)

 This basic idea of is UACCE driver is that a char device will be created for
 an accelerator device, which attributes will be showed in syfs, like
 /sys/class/uacce/<device>/<attr_files>. After openning this char device once,
 user will get a channel to access the resource of this accelerator device.
 User can configure above channel by ioctl of this openned fd, and mmap hardware
 resource, like MMIO or queue to user space.

3. Overall Design
-----------------
```
           +--------------+
           | wd alg layer |
           +--------------+
                   ^
                   |
           +----------------+       +----------------------------+
           | vendor drivers |------>| possible vendor driver lib |
           +----------------+       +----------------------------+
                |       |
                |       v
                |   +-------------------+
                |   | wd help functions |
                |   +-------------------+
                |       |
                v       v
           +------------------------------+
           | memory, mmio, fd, sysfs, dev |
           +------------------------------+
```

 1. memory type

   - SVA

   - No SVA(not long term support)

 2. operation type

   - synchronous operation

   - asynchronous operation

4. North API
------------

 1. general alg

    (to do: ...)

 2. compression/decompression

   - wd_alg_alloc_comp_ctx()

   - wd_alg_free_comp_ctx()

   - wd_alg_comp()

   - wd_alg_decomp()

   - wd_alg_acomp()

   - wd_alg_adecomp()

   - wd_alg_alloc_acomp_req()

   - wd_alg_free_acomp_req()

   - wd_alg_set_acomp_cb()

     (fix me: leave to user?)

5. South API
------------

 1. compression/decompression

   - wd_alg_comp_register()

   - wd_alg_comp_unregister()

   - struct wd_alg_comp {
             int (*init)();
             void (*exit)();
             int (*compress)();
             int (*decompress)();
	     struct wd_alg base;
     };

6. wd help functions
--------------------

 - wd_request_chan()

 - wd_release_chan()

7. Implementation
-----------------

8. Example
----------

 - HiSlicon KunPeng920 ZIP engine (to do: ...)

Appendix
========
