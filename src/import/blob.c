#include "blob.h"
#include "../platform/banked_ram.h"
#include "../platform/bankmem.h"
#include <string.h>

/* The heap's top (platform/banked_ram.h, HEAP_TOP: Veranda and the host).
 * BLOB_TOP_ALLOC: blob_alloc_top, in the copy that stages a part (the ZIP
 * reader's: a stored entry, or the room a deflated one expands into). BLOB_RELEASE:
 * a reader gives each bank of a blob at the top back as it finishes it, in
 * the parsers' copies (every one of them reads its part once, from the
 * start). The host has one copy, and all of it. */
#ifdef HEAP_TOP
#  if !defined(__CC65__) || defined(BLOB_OWNER_ZIP)
#    define BLOB_TOP_ALLOC
#  endif
#  if !defined(__CC65__) || defined(BLOB_OWNER_XLSX) || defined(BLOB_OWNER_STYLE) \
      || defined(BLOB_OWNER_SHEET) || defined(BLOB_OWNER_ODS)
#    define BLOB_RELEASE
#  endif
/* the bank the heap's top is at: a blob's bank there is above the bump
 * allocator's reach, so it goes back by raising the top over it */
#  define AT_TOP(k) ((uint16_t)(k) == (uint16_t)(first_bank + bank_count))
#endif

/* An overlay that only consumes a blob — the XLSX parsers read one and never
 * build one — leaves out everything that makes one. That is most of this
 * file, and the difference between its copy fitting and not. */
#ifndef BLOB_READER_ONLY

/* Whole banks, one at a time, in whatever order the allocator gives them. */
err_t blob_alloc(blob_t *b, uint32_t bytes)
{
    uint8_t want, i;
    handle_t h;

    blob_free(b);

    if (bytes == 0 || bytes > BLOB_MAX_BYTES)
        return ERR_LIMIT;

    want = (uint8_t)((bytes + BANK_SIZE - 1) / BANK_SIZE);

    for (i = 0; i < want; ++i) {
        h = bank_alloc(BANK_SIZE);
        if (h == H_NULL) {
            /* Give back what was taken. A partial reservation held onto
             * after a refusal is a leak that only shows up on the next
             * import. */
            b->banks = i;
            blob_free(b);
#ifdef HEAP_TOP
            return ERR_NOMEM;           /* the heap's, not the part's limit:
                                           the out-of-memory box */
#else
            return ERR_LIMIT;
#endif
        }
        b->bank[i] = H_BANK(h);
    }

    b->banks = want;
    b->len = 0;
    b->pos = 0;
    return ERR_OK;
}

#ifdef BLOB_TOP_ALLOC
/* As blob_alloc, in the banks just under the heap's top, which comes down
 * over them: the bump allocator cannot reach them, and a reader built with
 * BLOB_RELEASE gives them back one by one, lowest first, as it reads them
 * (the list is in ascending order). One such blob at a time: if the top is
 * already down, or the banks between the bump pointer and the top are too
 * few, an ordinary blob. */
err_t blob_alloc_top(blob_t *b, uint32_t bytes)
{
    uint8_t want, i;

    blob_free(b);
    if (bytes == 0 || bytes > BLOB_MAX_BYTES)
        return ERR_LIMIT;
    want = (uint8_t)((bytes + BANK_SIZE - 1) / BANK_SIZE);
    if ((uint16_t)(first_bank + bank_count) != bankmem_bank_count()
        || (uint16_t)(cur_bank - first_bank) + 1 + want > bank_count)
        return blob_alloc(b, bytes);
    bank_count -= want;
    for (i = 0; i < want; ++i)
        b->bank[i] = (uint8_t)(first_bank + bank_count + i);
    b->banks = want;
    b->len = 0;
    b->pos = 0;
    return ERR_OK;
}
#endif

#endif /* !BLOB_READER_ONLY */

/* Available to a reader-only copy as well, for an overlay that is handed a
 * blob and is the last to use it. See BLOB_NEED_FREE in blob_ovl.h. */
#if !defined(BLOB_READER_ONLY) || defined(BLOB_NEED_FREE)
void blob_free(blob_t *b)
{
    /* Really free them. An import stages several parts in turn — workbook,
     * relationships, shared strings, then every worksheet — and the cells
     * being built live in the same heap, so resetting it between phases is
     * not available. Leaking a blob's banks runs a real workbook out of
     * memory partway through, which is exactly what a large one would do
     * and a small one would not. */
    uint8_t i;

    for (i = 0; i < b->banks; ++i)
#ifdef HEAP_TOP
    {
        uint8_t k = b->bank[i];

        if (k == 0)
            continue;                   /* given back as it was read */
        if (AT_TOP(k))
            ++bank_count;               /* (ascending: each is the next) */
        else
            bank_free_whole(k);
    }
#else
        bank_free_whole(b->bank[i]);
#endif
    memset(b, 0, sizeof *b);
}
#endif

#ifndef BLOB_READER_ONLY
void blob_reset_write(blob_t *b) { b->len = 0; }
#endif

void blob_reset_read(blob_t *b)  { b->pos = 0; }

/* Bank and offset from a position. The bank size is a power of two, so this
 * is a shift and a mask rather than a divide — which matters, because it
 * happens on every chunk of every entry. */
#define BLOB_BANK(b, p) ((b)->bank[(uint8_t)((p) >> 13)])
#define BLOB_OFF(p)     ((uint16_t)((p) & 0x1FFF))

#ifndef BLOB_READER_ONLY

uint8_t blob_write(blob_t *b, const void *data, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t cap = (uint32_t)b->banks * BANK_SIZE;

    while (len) {
        uint16_t off, room, n;

        if (b->len >= cap)
            return 0;                   /* full */

        off  = BLOB_OFF(b->len);
        room = (uint16_t)(BANK_SIZE - off);
        n    = len < room ? len : room; /* never cross a bank in one copy */

        bankmem_write(BLOB_BANK(b, b->len), off, p, n);
        b->len += n;
        p      += n;
        len     = (uint16_t)(len - n);
    }
    return 1;
}

#endif /* !BLOB_READER_ONLY */

uint16_t blob_read(blob_t *b, void *out, uint16_t len)
{
    uint8_t *p = (uint8_t *)out;
    uint16_t done = 0;

    while (len) {
        uint16_t off, room, n;
        uint32_t left = b->len - b->pos;

        if (b->pos >= b->len)
            break;

        off  = BLOB_OFF(b->pos);
        room = (uint16_t)(BANK_SIZE - off);
        n    = len < room ? len : room;
        if ((uint32_t)n > left)
            n = (uint16_t)left;

        bankmem_read(BLOB_BANK(b, b->pos), off, p, n);
        b->pos += n;
#ifdef BLOB_RELEASE
        if (BLOB_OFF(b->pos) == 0) {    /* a bank read to its end */
            uint8_t *k = &b->bank[(uint8_t)(b->pos >> 13) - 1];

            if (AT_TOP(*k)) {
                ++bank_count;
                *k = 0;
            }
        }
#endif
        p      += n;
        done    = (uint16_t)(done + n);
        len     = (uint16_t)(len - n);
    }
    return done;
}

uint16_t blob_feed(void *ctx, void *out, uint16_t len)
{
    return blob_read((blob_t *)ctx, out, len);
}
