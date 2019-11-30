#ifndef __ION_UTILS_H
#define __ION_UTILS_H

#include "include/ion.h"

#define ION_DEVICE "/dev/ion"

#define ION_BUFFER_LEN	4096
#define MAX_HEAP_COUNT	ION_HEAP_TYPE_CUSTOM

struct ion_buffer_info {
	int ionfd;
	int buffd;
	unsigned int heap_type;
	unsigned int flag_type;
	unsigned long heap_size;
	unsigned long buflen;
	unsigned char *buffer;
};


/* This is used to create an ION buffer FD for the kernel buffer
 * So you can export this same buffer to others in the form of FD
 */
int ion_export_buffer_fd(struct ion_buffer_info *ion_info);

/* This is used to import or map an exported FD.
 * So we point to same buffer without making a copy. Hence zero-copy.
 */
int ion_import_buffer_fd(struct ion_buffer_info *ion_info);

/* This is used to close all references for the ION client */
void ion_close_buffer_fd(struct ion_buffer_info *ion_info);

#endif
