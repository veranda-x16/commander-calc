/* filedir_veranda.c -- the directory listing on Veranda (file_io.h's
 * dir_*), in the file dialog's overlay as filedir_x16.c is: DirOpen,
 * DirNext and ChDir, whose entries are +0 the type ('D' a directory), +1
 * the size in blocks, +3 the name. */
#include "file_io.h"
#include <veranda.h>

#ifdef __CC65__
#  pragma code-name (push, "OVERLAY1")
#  pragma rodata-name (push, "OVL1RO")
#endif

extern char fio_name1[];                /* file_buf_veranda.c */
extern uint8_t fio_entry[];

static char *low(char *d, const char *s)
{
    uint8_t i = 0;

    while (s[i] && i < X16S_PATH_MAX - 1) {
        d[i] = s[i];
        ++i;
    }
    d[i] = '\0';
    return d;
}

static const char all[] = "*";

err_t dir_chdir(const char *name)
{
    uint8_t e = ChDir(low(fio_name1, name));

    dir_status = dosStatus;
    return e == 0 ? ERR_OK : ERR_IO;
}

err_t dir_open(void)
{
    return DirOpen(low(fio_name1, all)) == 0 ? ERR_OK : ERR_IO;
}

/* A name longer than the list's rows comes back empty: cut short, it
 * would name nothing the DOS can open. */
uint8_t dir_next(file_entry_t *e)
{
    const char *s = (const char *)fio_entry + 3;
    uint8_t i = 0;

    if (DirNext(fio_entry) != 0)
        return 0;
    while (s[i] && i < FILE_LIST_MAX - 1) {
        e->name[i] = s[i];
        ++i;
    }
    e->name[s[i] ? 0 : i] = '\0';
    e->blocks = fio_entry[1] | (uint16_t)fio_entry[2] << 8;
    e->is_dir = fio_entry[0] == 'D';
    return 1;
}

void dir_close(void) { }

#ifdef __CC65__
#  pragma rodata-name (pop)
#  pragma code-name (pop)
#endif
