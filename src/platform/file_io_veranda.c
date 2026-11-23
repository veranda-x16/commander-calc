/* file_io_veranda.c -- file_io.h over Veranda's file streams.
 *
 * Veranda owns the DOS's command channel; a program opens files with FOpen
 * and asks for a size with FSize (API 2). Not compiled on its own:
 * file_io_x16.c includes it on Veranda, so it lands where the X16 build's
 * file layer does -- OVL2, and a renamed copy in each overlay that has one
 * (file_io_*_x16.c, file_io_ovl.h), with the same FILEIO_* choices. Its
 * buffers are shared and resident (file_buf_veranda.c): they must be below
 * $A000.
 *
 * Every pointer handed to the kernel must be below $A000 (the bank gate
 * maps a kernel bank there), and an overlay's buffers are above it: names
 * are copied, and data goes through a small buffer when it has to. */
#include "file_io.h"
#include <veranda.h>
#include <string.h>

#if defined(__CC65__) && !defined(FILEIO_NO_SEGMENT)
#  pragma code-name (push, "OVERLAY2")
#  pragma rodata-name (push, "OVL2RO")
#  define FILEIO_POP_SEGMENT
#endif

#define BOUNCE 128
extern char fio_name1[], fio_name2[];   /* file_buf_veranda.c */
#ifdef FILEIO_WANT_SEEK
extern char fio_seek[];                 /* one stream seeks: the ZIP reader's */
#endif
extern uint8_t fio_bounce[];
#define name1  fio_name1
#define name2  fio_name2
#define bounce fio_bounce

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

static err_t opened(fstream_t *f, uint8_t h, uint8_t writing)
{
    if (h < 2)
        return dosStatus == 62 ? ERR_NOTFOUND : ERR_IO;
    f->lfn = h;
    f->dev = 8;
    f->eof = 0;
    f->writing = writing;
    return ERR_OK;
}

err_t file_open_read(fstream_t *f, const char *name)
{
#ifdef FILEIO_WANT_SEEK
    low(fio_seek, name);                /* a seek reopens it (file_seek) */
#endif
    return opened(f, FOpen(low(name1, name), 'r'), 0);
}

err_t file_open_write(fstream_t *f, const char *name)
{
    return opened(f, FOpen(low(name1, name), 'w'), 1);
}

uint16_t file_read(fstream_t *f, void *buf, uint16_t len)
{
    uint8_t *d = (uint8_t *)buf;
    uint16_t done = 0, n, got;

    if (f->eof)
        return 0;
    if ((uint16_t)d + len <= 0xA000) {
        done = FRead(f->lfn, d, len);
    } else {
        while (done < len) {
            n = len - done < BOUNCE ? len - done : BOUNCE;
            got = FRead(f->lfn, bounce, n);
            memcpy(d + done, bounce, got);
            done += got;
            if (got < n)
                break;
        }
    }
    if (done < len)
        f->eof = 1;
    return done;
}

uint16_t file_write(fstream_t *f, const void *buf, uint16_t len)
{
    const uint8_t *s = (const uint8_t *)buf;
    uint16_t done = 0, n;

    if ((uint16_t)s + len <= 0xA000)
        return FWrite(f->lfn, (void *)s, len) == 0 ? len : 0;
    while (done < len) {
        n = len - done < BOUNCE ? len - done : BOUNCE;
        memcpy(bounce, s + done, n);
        if (FWrite(f->lfn, bounce, n) != 0)
            break;
        done += n;
    }
    return done;
}

uint8_t file_eof(const fstream_t *f) { return f->eof; }

err_t file_close(fstream_t *f)
{
    return FClose(f->lfn) == 0 ? ERR_OK : ERR_IO;
}

#ifdef FILEIO_WANT_SEEK
/* The X16 build's two rules (file_io_x16.c, docs/design/files.md) hold
 * here too: a stream read to its end stays at its end (the KERNAL latches
 * end-of-file per channel), so the file is closed and opened again first;
 * and the DOS's status after POSITION may be the previous command's, so
 * it is not believed -- the ZIP reader checks what it reads. Measured:
 * the emulator's host folder refuses a POSITION after a read to the end. */
err_t file_seek(fstream_t *f, uint32_t pos)
{
    uint8_t h;

    FClose(f->lfn);
    h = FOpen(fio_seek, 'r');
    if (h < 2)
        return ERR_IO;
    f->lfn = h;
    f->eof = 0;
    FSeek(h, (uint16_t)pos, (uint16_t)(pos >> 16));
    return ERR_OK;
}

err_t file_size(fstream_t *f, uint32_t *out)
{
    uint16_t lo = FSize(f->lfn);

    if (dosStatus != 7)
        return ERR_IO;
    *out = ((uint32_t)r1 << 16) | lo;
    return ERR_OK;
}

#endif /* FILEIO_WANT_SEEK */

#ifndef FILEIO_STREAM_ONLY
err_t file_remove(const char *name)
{
    return FDelete(low(name1, name)) == 0 ? ERR_OK : ERR_IO;
}

err_t file_rename(const char *from, const char *to)
{
    return FRename(low(name1, from), low(name2, to)) == 0 ? ERR_OK : ERR_IO;
}
#endif /* !FILEIO_STREAM_ONLY */

#undef name1
#undef name2
#undef bounce
#ifdef FILEIO_POP_SEGMENT
#  pragma rodata-name (pop)
#  pragma code-name (pop)
#  undef FILEIO_POP_SEGMENT
#endif
