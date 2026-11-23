/* vmenu_veranda.c -- Veranda Calc's menus and start-up (OVL_MENU).
 *
 * Veranda's menus in GEOS's tables, in place of the X16 build's own menu
 * bar (menu_run(), which Veranda does without). The six menus and their
 * 37 items are the X16 build's (menu.c), each item the grid_key() code it
 * has always stood for: choosing one is that key (main_veranda.c,
 * vm_act). The tables are made here, in the overlay, into the resident
 * vm_buf: the bar once, a sub menu each time its title is chosen
 * (vmenu_veranda.h says why).
 *
 * Also here, because they run once: the heap's banks and the status line's
 * "... bytes free". */
#include "../x16sheet.h"
#include "vmenu_veranda.h"
#include "menu.h"
#include "grid.h"
#include "../platform/banked_ram.h"
#include <veranda.h>
#include <string.h>

#pragma code-name (push, "OVERLAY14")
#pragma rodata-name (push, "OVL14RO")
#pragma bss-name (push, "OVL14BSS")

static const char T_all[] = "Veranda Calc\0File\0Edit\0Layout\0Data\0Chart\0Sheet";

/* Each menu: its labels, each followed by its 0 and its code; an empty
 * label ends it. The codes are menu.c's tables'.
 *
 * Veranda Look 3 "Sunroom": a label that starts with "\x1f" (G) starts a
 * group (the kernel draws a dotted line above it; the byte is not drawn),
 * and a TAB puts the item's key at the list's right (the classic look
 * ignores the "\x1f" and shows the TAB as a space). The keys are the
 * program's own. */
#define G "\x1f"
static const char M_calc[] =
    "About Veranda Calc...\0"     "\x00"    /* 0: vm_act's About */
    G "Desk accessory...\0"       "\x80"    /* VM_DESK */
    "";
static const char M_file[] =
    "New\tF1\0"                   "\x85"
    "Open...\tF3\0"               "\x86"
    G "Save\tF4\0"                "\x8A"
    "Save as...\tF6\0"            "\x8B"
    G "Import...\tF7\0"           "\x88"
    "Export...\tF8\0"             "\x8C"
    G "Quit\tF9\0"                "\x10"
    "";
static const char M_edit[] =
    "Edit cell\tF2\0"             "\x89"
    G "Select\tCtrl A\0"          "\x01"
    "Copy\tCtrl C\0"              "\x03"
    "Paste\tCtrl V\0"             "\x16"
    G "Undo\tF5\0"                "\x87"
    "Clear\tDel\0"                "\x14"
    G "Find...\tCtrl F\0"         "\x06"
    "Replace...\0"                "\x9F"
    "";
static const char M_layout[] =
    "Insert row\0"                "\xF8"
    "Delete row\0"                "\xF9"
    "Insert column\0"             "\x1E"
    "Delete column\0"             "\x1F"
    "Column width...\0"           "\x9E"
    G "Bold\tCtrl B\0"            "\x9C"
    "Border\0"                    "\x9A"
    "Underline\0"                 "\x9B"
    G "Freeze\0"                  "\xFC"
    "";
static const char M_data[] =
    "Sort up\0"                   "\xF5"
    "Sort down\0"                 "\xF6"
    "Undo sort\0"                 "\xF7"
    G "Recalc now\0"              "\xFA"
    "Auto recalc\0"               "\xFB"
    "";
static const char M_chart[] =
    "Bars\0"                      "\xFD"
    "Line\0"                      "\xFE"
    "Pie\0"                       "\xFF"
    "";
static const char M_sheet[] =
    "Next\0"                      "\xF0"
    "Previous\0"                  "\xF1"
    G "Add...\0"                  "\xF2"
    "Rename...\0"                 "\xF4"
    "Delete\0"                    "\xF3"
    "";
static const char *const menus[VM_TITLES] = {
    M_calc, M_file, M_edit, M_layout, M_data, M_chart, M_sheet
};

/* a menu table's head: top, bottom, left, right, flags */
static void head(uint8_t *t, uint8_t top, uint8_t bot, uint16_t left,
                 uint16_t right, uint8_t flags)
{
    t[0] = top;
    t[1] = bot;
    t[2] = (uint8_t)left;
    t[3] = (uint8_t)(left >> 8);
    t[4] = (uint8_t)right;
    t[5] = (uint8_t)(right >> 8);
    t[6] = flags;
}

/* an item: its text, type, routine */
static void item(uint8_t *it, const char *text, uint8_t type, void (*f)(void))
{
    it[0] = (uint8_t)(uint16_t)text;
    it[1] = (uint8_t)((uint16_t)text >> 8);
    it[2] = type;
    it[3] = (uint8_t)(uint16_t)f;
    it[4] = (uint8_t)((uint16_t)f >> 8);
}

/* copy one label from *p to *s, both moved past it; its width in the
 * font now (the UI font: what the menus draw with, theme_menus); a
 * group's "\x1f" not counted, a TAB as the 24 pixels the kernel leaves
 * between the text and the key (right-aligned 8 pixels from the list's
 * edge: the list is 8 + this + 8 wide) */
static uint16_t label(const char **p, char **s)
{
    uint16_t w = 0;
    const char *a = *p;
    char *d = *s;
    uint8_t c;

    while ((c = (uint8_t)*a) != 0) {
        if (c == '\t')
            w += 24;
        else if (c != 0x1F)
            w += GetCharWidth(c);
        *d++ = *a++;
    }
    *d++ = 0;
    *p = a + 1;
    *s = d;
    return w;
}

/* The bar (theme_menus has set the look: platform/look_veranda.c). The
 * kernel places the titles itself (Veranda's look_menu.py): the first,
 * the program's name in bold (a pixel more a character), after the mark's
 * 24 pixels; each 6 pixels from its text on both sides, its right end on
 * a card. VM_X keeps each title's middle / 2 (the test's player clicks
 * there) and its left / 2, the sub menu's place. */
void vmenu_start(void)
{
    const char *t = T_all;
    char *s = (char *)vm_buf + VM_STR;
    uint16_t x = 0, e;
    uint8_t i;

    UseFont(FONT_UI);
    for (i = 0; i < VM_TITLES; ++i) {
        item(vm_buf + VM_BAR + 7 + 5 * i, s, DYN_SUB_MENU, vm_sub);
        e = label(&t, &s);
        e = i ? x + 6 + e : x + 24 + e + (uint16_t)(t - T_all - 1);
        e = (e + 13) & ~7;
        vm_buf[VM_X + i] = (uint8_t)(x >> 1);
        x = e;
    }
    vm_buf[VM_X + VM_TITLES] = (uint8_t)(x >> 1);
    UseSystemFont();
    head(vm_buf + VM_BAR, 0, VM_BAR_H - 1, 0, x + 64,
         HORIZONTAL | UN_CONSTRAINED | VM_TITLES);
    DoMenu(vm_buf + VM_BAR, 0);
}

void vmenu_fill(uint8_t m)
{
    const char *p = menus[m];
    char *s = (char *)vm_buf + VM_STR + 48;
    uint8_t n = 0;
    uint16_t w, wmax = 0, left = (uint16_t)vm_buf[VM_X + m] << 1;

    UseFont(FONT_UI);
    while (*p) {
        item(vm_buf + VM_SUB + 7 + 5 * n, s, MENU_ACTION, vm_act);
        w = label(&p, &s);
        vm_buf[VM_CODES + n] = (uint8_t)*p++;
        if (w > wmax)
            wmax = w;
        ++n;
    }
    UseSystemFont();
    head(vm_buf + VM_SUB, VM_BAR_H, (uint8_t)(VM_BAR_H + n * VM_ITEM_H),
         left, left + wmax + 16, VERTICAL | n);
    r0 = (uint16_t)(vm_buf + VM_SUB);
}

/* File > About: a dialog box (vdlg_veranda.h) */
#include "vdlg_veranda.h"

static const char S_about_t[] = "Veranda Calc " CC_VERSION;
static const char S_about_m[] = "Commander Calc on Veranda. MIT licence.";

void vmenu_about(void)
{
    vdlg(VD_MSG, S_about_t, S_about_m, 0, 0);
}

#pragma bss-name (pop)
#pragma rodata-name (pop)
#pragma code-name (pop)

/* --- start-up -------------------------------------------------------- */

/* In the start-up overlay (OVL_CHARTOUT), with the heap below: the menu
 * overlay is full (Look 3's menu groups and keys took its last bytes).
 * main_veranda.c maps it for screen_init, vr_heap and vr_status. */
#pragma code-name (push, "OVERLAY16")
#pragma rodata-name (push, "OVL16RO")
#pragma bss-name (push, "OVL16BSS")

extern uint8_t calc_heap_first, calc_heap_banks;
extern uint16_t bankmem_top;            /* bankmem_veranda.c */
extern char calc_status[19];

static const char S_free[] = " bytes free";

/* "<n> bytes free" into calc_status, the grid's status line */
void vr_status(void)
{
    uint32_t v = bank_bytes_free();
    char tmp[11];
    uint8_t n = 0, i = 0;

    if (v == 0)
        tmp[n++] = '0';
    while (v) {
        tmp[n++] = (char)('0' + (uint8_t)(v % 10));
        v /= 10;
    }
    while (n)
        calc_status[i++] = tmp[--n];
    strcpy(calc_status + i, S_free);
    grid_set_status(calc_status);
}

/* The heap, in the start-up overlay with screen_init (main_veranda.c maps
 * OVL_CHARTOUT for both: the chart's .BMP writer is in ui/chart.c on
 * Veranda, and the menu overlay is full) */

#define DA_BANKS 22


/* The heap: the largest run of banks (BankAlloc gives consecutive ones),
 * less DA_BANKS for a desk accessory (which puts the program aside in 22
 * banks) when that leaves 16. (The Open and Save dialog's bank, while it
 * is open, is lent by the screen's shadow: ui/filedlg.c.) */
err_t vr_heap(void)
{
    uint8_t n, first = 0;

    for (n = 200; n; --n)
        if ((first = BankAlloc(n)) != 0)
            break;
    if (!first)
        return ERR_NOMEM;
    if (n >= DA_BANKS + 16) {
        BankFree(first);
        n -= DA_BANKS;
        first = BankAlloc(n);
        if (!first)
            return ERR_NOMEM;
    }
    calc_heap_first = first;
    calc_heap_banks = n;
    bankmem_top = (uint16_t)first + n;
    return bank_heap_init(first, n);
}

/* The shell's document (dataLongName, its whole name: API 5, Veranda's
 * DECISIONS V82; in the folder curDirName): to its folder, and the key
 * that opens it -- File > Open (F3) for .X16S, else Import (F7) -- with
 * calc_doc set, so the file dialog takes the name (ui/filedlg.c); 0:
 * none. */
extern uint8_t calc_doc;

uint8_t vr_doc(void)
{
    uint8_t n = (uint8_t)strlen((char *)dataLongName);

    if (n == 0 || n > LONG_MAX)
        return 0;
    ChDir((char *)curDirName);
    calc_doc = 1;
    return (uint8_t)(n > 4 && dataLongName[n - 3] == '1' ? 0x86 : 0x88);
}

#pragma bss-name (pop)
#pragma rodata-name (pop)
#pragma code-name (pop)
