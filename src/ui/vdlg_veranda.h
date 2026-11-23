/* vdlg_veranda.h -- Veranda Calc's dialog boxes, for the overlays.
 *
 * #included by the overlays that ask something (menu.c, filedlg.c) on
 * Veranda only: one static copy in each, as file_io_x16.c is. The X16
 * build draws its own boxes and reads keys in a loop; here the questions
 * are Veranda's dialog boxes (DoDlgBox), which run Veranda's main loop
 * until a button closes them and put back what they covered.
 *
 * A dialog table and every string it shows must be below $A000 (the
 * kernel maps its own banks there), and these overlays run at $A000: so
 * the table and copies of the title and the message are built on the C
 * stack ($8600-$87FF), about 100 bytes, for the length of the call. The
 * field's buffer (VD_ASK) must be below $A000 too: the callers' are on the
 * C stack or in the resident image.
 *
 * The box: 320 x 96 pixels in the middle of the screen, (160, 192) to
 * (479, 287) (its own position: the default one is GEOS's 192 pixels), the title in bold on its first
 * line, the message on its second, the field on its third, the buttons
 * along the bottom. tools/veranda_script.py clicks them at these places. */
#ifndef VDLG_VERANDA_H
#define VDLG_VERANDA_H
#include <veranda.h>

#define VD_MSG   0      /* OK                                       -> 1   */
#define VD_YESNO 1      /* YES, NO                                  -> 1 yes */
#define VD_ASK   2      /* the field, OK, CANCEL (Return is OK)     -> 1 ok */
#define VD_REPL  3      /* YES, NO, OK (all the rest), CANCEL -> 'Y' 'N' 'A' 27 */

#define VD_TOP   192                    /* (480 - 96) / 2 */
#define VD_BOT   287
#define VD_LEFT  160
#define VD_RIGHT 479
#define VD_BY    72     /* the buttons' row (DBI_Y_2) */
#define VD_X0    1      /* their columns, in cards from the box's left */
#define VD_X1    9
#define VD_X2    17
#define VD_X3    33     /* the last: CANCEL or NO */

static const uint8_t vd_head[] = {
    SET_DB_POS | WIDE_DB | 1,           /* its own place, y in words */
    VD_TOP & 255, VD_TOP >> 8, VD_BOT & 255, VD_BOT >> 8,
    VD_LEFT & 255, VD_LEFT >> 8, VD_RIGHT & 255, VD_RIGHT >> 8,
    DBVARSTR, TXT_LN_X, TXT_LN_1_Y, 0x0C,       /* r5: the title */
    DBVARSTR, TXT_LN_X, TXT_LN_2_Y, 0x0E        /* r6: the message */
};
/* after the head: each kind's items, 0 ends them */
static const uint8_t vd_msg[] = { OK, VD_X0, VD_BY, 0 };
static const uint8_t vd_yesno[] = { YES, VD_X0, VD_BY, NO, VD_X3, VD_BY, 0 };
static const uint8_t vd_ask[] = {
    DBGETSTRING, TXT_LN_X, TXT_LN_3_Y, 0x10, 0,  /* r7: the field; max below */
    OK, VD_X0, VD_BY, CANCEL, VD_X3, VD_BY, 0
};
static const uint8_t vd_repl[] = {
    YES, VD_X0, VD_BY, NO, VD_X1, VD_BY, OK, VD_X2, VD_BY,
    CANCEL, VD_X3, VD_BY, 0
};
static const uint8_t *const vd_kind[] = { vd_msg, vd_yesno, vd_ask, vd_repl };
static const uint8_t vd_len[] = { sizeof vd_msg, sizeof vd_yesno, sizeof vd_ask,
                                  sizeof vd_repl };

#define VD_TMAX 40
#define VD_MMAX 48

/* at most n characters of s into d, ended */
static void vd_copy(char *d, const char *s, uint8_t n)
{
    while (n-- && *s)
        *d++ = *s++;
    *d = 0;
}

/* Ask. `buf` and `max` (characters, not counting the 0) for VD_ASK only. */
static uint8_t vdlg(uint8_t kind, const char *title, const char *msg,
                    char *buf, uint8_t max)
{
    struct {
        uint8_t t[sizeof vd_head + sizeof vd_repl];
        char a[VD_TMAX + 2];
        char b[VD_MMAX + 1];
    } d;
    uint8_t r;

    memcpy(d.t, vd_head, sizeof vd_head);
    memcpy(d.t + sizeof vd_head, vd_kind[kind], vd_len[kind]);
    if (kind == VD_ASK)
        d.t[sizeof vd_head + 4] = max;
    d.a[0] = BOLDON;
    vd_copy(d.a + 1, title, VD_TMAX);
    vd_copy(d.b, msg, VD_MMAX);
    r5 = (uint16_t)d.a;
    r6 = (uint16_t)d.b;
    r7 = (uint16_t)buf;
    r = DoDlgBox(d.t);
    switch (kind) {
    case VD_MSG:   return 1;
    case VD_YESNO: return (uint8_t)(r == YES);
    case VD_ASK:   return (uint8_t)(r == OK || r == DBGETSTRING);
    }
    return (uint8_t)(r == YES ? 'Y' : r == NO ? 'N' : r == OK ? 'A' : 27);
}

#endif /* VDLG_VERANDA_H */
