/* file_buf_veranda.c -- the file layer's buffers on Veranda, shared by
 * every copy of it (file_io_veranda.c) and below $A000: what the kernel
 * reads and writes must be (the bank gate maps a kernel bank there). */
#include "file_io.h"

char fio_name1[X16S_PATH_MAX], fio_name2[X16S_PATH_MAX];
char fio_seek[X16S_PATH_MAX];           /* the stream that seeks: its name */
uint8_t fio_bounce[128];
uint8_t fio_entry[72];
uint8_t dir_status;
