/* look_veranda.c -- Veranda Look 2 in Veranda Calc (make veranda).
 *
 * Veranda's own look for its native programs (Veranda's DECISIONS V74,
 * V77, V79; the SDK's theme.h, vrwin.h):
 *   - the palette: Look 2's sixteen entries, light or dark as the user's
 *     settings say (theme_palette); the grid's colours are the X16's,
 *     mapped onto it by shadow_veranda.s's scr_paper;
 *   - the menu bar: the kernel's (theme_menus: the Veranda mark, the
 *     program's name first, the clock, the speaker, open menus in the
 *     selection colour, menus and dialog boxes in the UI font);
 *   - the window's chrome: the title band under the menu bar (card rows
 *     2-3, the theme's accent) with the document's long name (the SDK's
 *     vrwin.c, compiled into this overlay); the formula bar is the
 *     toolbar, the sheets' tabs and the status line the status bar, whose
 *     second card row is coloured here (the grid draws the rest).
 * Also the Veranda Calc menu's desk accessories, and the folders the
 * Open and Save dialog's pop-up goes to (vr_cd).
 *
 * In OVL_CLIP's bank (overlay 21), called through ovl_far21: the
 * resident image has no room for it. What the kernel reads (names, the
 * paths) is copied below $A000 first.
 *
 * Memory: the SDK's theme.c is linked as it is (theme_load, theme_menus,
 * the palette's table); its themedraw.c, themecov.c and vrpath.c are not
 * -- a program short of main RAM may define their calls itself: theme_
 * palette and theme_rect are here (in the overlay), theme_map_on and
 * theme_cover_rect in theme_veranda.s (resident, for the dialog's
 * overlay), vr_cd's resident end in main_veranda.c. They do what the
 * SDK's do. */
#include "../x16sheet.h"
#include <veranda.h>
#include <theme.h>
#include <vrwin.h>
#include <vrpath.h>
#include <string.h>

/* resident (main_veranda.c) */
extern uint8_t look_dirty;              /* the chrome to draw again */
extern char cur_name[];                 /* ui/grid.c: the document's path; "": none */
extern uint8_t look_cd_arg;             /* vr_cd's folder ... */
extern char look_cd_path[18];           /* ... and its path */

/* the SDK's theme.c (veranda.lib): Look 2's palette, and its flag that
 * the program's palette is Look 2's (theme_look2) */
extern const unsigned int theme_look_[2][16];
extern unsigned char theme_lk2;

#pragma code-name (push, "OVERLAY21")
#pragma rodata-name (push, "OVL21RO")
#pragma bss-name (push, "OVL21BSS")

#define VCTRL  (*(volatile unsigned char *)0x9F25)
#define VADDRL (*(volatile unsigned char *)0x9F20)
#define VADDRM (*(volatile unsigned char *)0x9F21)
#define VADDRH (*(volatile unsigned char *)0x9F22)
#define VDATA0 (*(volatile unsigned char *)0x9F23)

/* the SDK's theme_palette (themedraw.c): entries 2-15 Look 2's, 0 and 1
 * the settings' paper and ink (dark mode: its own) */
void theme_palette(void)
{
    const unsigned int *c = theme_look_[theme.dark ? 1 : 0];
    unsigned char i;
    unsigned int v;

    theme_lk2 = 1;
    VCTRL = 0;
    VADDRL = 0;
    VADDRM = 0xFA;
    VADDRH = 0x11;                      /* $1FA00, increment 1 */
    for (i = 0; i < 16; ++i) {
        v = i > 1 || theme.dark ? c[i] : i ? theme.ink : theme.paper;
        VDATA0 = v & 255;
        VDATA0 = v >> 8;
    }
}

/* the SDK's theme_rect (themedraw.c), for vrwin.c */
void theme_rect(unsigned char x, unsigned char w, unsigned char y,
                unsigned char h, unsigned char c)
{
    if (c || theme_map_on())
        ColorRect(x, w, y, h, c);
}

/* the window: the whole screen under the menu bar, no toolbar of
 * vrwin's own (the formula bar is the grid's row), a status bar of two
 * card rows */
static const struct vrwin win = { 0, 2, 80, 58, 0, 2, 0 };
static const char S_untitled[] = "Untitled";
static const char S_title[] = "CALC TITLE ";
extern char scr_line[];                 /* platform/screen_veranda.c */

/* the title band (the document's own name, cut to fit) and the status
 * bar's second card row */
void look_chrome(void)
{
    const char *n = cur_name, *p = strrchr(n, '/');

    if (p)
        n = p + 1;
    if (!*n)
        n = S_untitled;
    vr_win_title(&win, n);
    theme_rect(0, 80, 59, 1, CHROME);
    look_dirty = 0;
    /* the console (the gates): "CALC TITLE <name>", built in scr_line,
     * below $A000 (vrwin.c's BSS, which shares it, is done with) */
    strcpy(scr_line, S_title);
    strncat(scr_line, n, 64);
    LogText(scr_line);
}

extern uint8_t scr_papers[16];          /* shadow_veranda.s: the X16's colours' entries */

/* at the start, before the menus: the theme, the palette, the bar; the
 * grid's selection colour (the cursor, the headers' marks: SELECT) the
 * theme's highlight, as the open menu's and the Open dialog's (Veranda
 * Look 3, Sunroom: its sky) */
void look_start(void)
{
    uint8_t i;

    theme_load();
    if (theme.hilite)
        for (i = 0; i < 16; ++i)
            if (scr_papers[i] == SELECT)
                scr_papers[i] = theme.hilite;
    theme_palette();
    theme_menus();
    look_chrome();
}

/* --- vr_cd (the SDK's vrpath.c, for the dialog's folder pop-up) ------- */

static const char S_sys[] = VR_SYSDIR;
static const char S_apps[] = VR_APPSDIR;
static const char S_docs[] = VR_DOCSDIR;
static const char S_data[] = VR_DATADIR;
static const char S_root[] = "/";

static char *put(const char *s)
{
    return strcpy(look_cd_path, s);
}

static uint8_t cd(const char *s)
{
    return ChDir(put(s));
}

/* look_cd_arg's folder made current, its path in look_cd_path */
void look_cd(void)
{
    uint8_t w = look_cd_arg;
    const char *p = w == VR_APPS ? S_apps : w == VR_DOCS ? S_docs
                  : w == VR_DATA ? S_data : S_sys;

    if (cd(p) == 0)
        return;
    if (w == VR_SYS || cd(S_sys)) {     /* no /VERANDA: the top folder */
        cd(S_root);
        return;
    }
    if (w == VR_APPS) {                 /* not set up: the applications here */
        put(S_sys);
        return;
    }
    cd(S_root);                         /* made, from the top */
    MkDir(put(p) + 1);
    if (cd(p) == 0)
        return;
    if (w == VR_DOCS) {
        cd(S_root);
        return;
    }
    cd(S_sys);
}

/* --- Veranda Calc > Desk accessory... -------------------------------- */

#include "../ui/vdlg_veranda.h"

static const uint8_t da_box[] = {
    DEF_DB_POS | 1,
    DBGETFILES, 4, 4,
    OPEN, DBI_X_2, DBI_Y_0,
    CANCEL, DBI_X_2, DBI_Y_2,
    0
};
static const char S_noda1[] = "No desk accessories";
static const char S_noda2[] = "with the applications.";
static const char S_cannot[] = "Cannot open";

/* the applications' folder's desk accessories (DBGETFILES), the one
 * chosen run over Calc; back in Calc's folder after */
void look_desk(void)
{
    struct {
        uint8_t t[sizeof da_box];
        char da[17];
    } d;                                /* (the C stack: below $A000) */
    uint8_t r;

    memcpy(d.t, da_box, sizeof d.t);
    strcpy(d.da, S_apps);               /* the applications' folder, else */
    if (ChDir(d.da)) {                  /* Veranda's, else the top (vr_cd's */
        strcpy(d.da, S_sys);            /* look_cd_path is the dialog's) */
        if (ChDir(d.da))
            ChDir(strcpy(d.da, S_root));
    }
    d.da[0] = 0;
    r5 = (unsigned)d.da;
    r7L = DESK_ACC;
    r10 = 0;
    r = DoDlgBox(d.t);
    if (r == OPEN && d.da[0]) {
        if (GetFile(d.da, 0, 0, 0, 0))
            vdlg(VD_MSG, S_cannot, d.da, 0, 0);
    } else if (r == DBGETFILES)
        vdlg(VD_MSG, S_noda1, S_noda2, 0, 0);
    ChDir((char *)curDirName);
}

#pragma bss-name (pop)
#pragma rodata-name (pop)
#pragma code-name (pop)
