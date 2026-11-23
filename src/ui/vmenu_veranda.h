/* vmenu_veranda.h -- Veranda Calc's menus: what the resident part
 * (main_veranda.c) and the menu overlay (vmenu_veranda.c) share.
 *
 * Veranda reads a menu table whenever a menu is open, with its own banks
 * at $A000, so the tables must be in the resident image. All 37 items
 * would take about 600 bytes there; instead the bar is a table of six
 * DYN_SUB_MENU items, and the routine Veranda calls when one is chosen
 * (vm_sub) has the overlay write that one sub menu into vm_buf. One sub
 * menu is open at a time, so one buffer serves all six. */
#ifndef VMENU_VERANDA_H
#define VMENU_VERANDA_H
#include "../x16sheet.h"

#define VM_TITLES 7                     /* Veranda Calc, File ... Sheet */
#define VM_ITEMS  9                     /* the longest sub menu (Layout) */

#define VM_BAR    0                     /* the bar's table: 7 + 7 x 5 */
#define VM_SUB    (VM_BAR + 7 + 5 * VM_TITLES)  /* the open sub menu's */
#define VM_CODES  (VM_SUB + 7 + 5 * VM_ITEMS)   /* its items' grid_key codes */
#define VM_X      (VM_CODES + VM_ITEMS)         /* each title's left edge / 2 */
#define VM_STR    (VM_X + VM_TITLES + 1)        /* the bar's titles, then the sub's labels */
#define VM_SIZE   (VM_STR + 48 + 104)       /* (Layout's labels: 104 exactly, with Look 3's groups and keys) */

/* The Veranda look (theme_menus; Veranda's DECISIONS V74, V79): the
 * kernel lays the bar out itself -- the Veranda mark, the program's name
 * in bold, each title 6 pixels from its text, on whole cards -- and opens
 * a sub menu under its title, 16 lines an item from line 16; the tables
 * give it the texts and the sub menus' widths (in the UI font). */
#define VM_BAR_H  16                    /* the bar: y 0-15 (card rows 0-1) */
#define VM_ITEM_H 16                    /* a sub menu's item */
#define VM_GRID_Y 32                    /* the grid from here: the title band
                                           is card rows 2-3 */

#define VM_DESK   0x80                  /* Veranda Calc > Desk accessory: no
                                           key's code (main_veranda.c) */

extern uint8_t vm_buf[VM_SIZE];         /* main_veranda.c */
void vm_sub(void);                      /* the bar's routine (resident) */
void vm_act(void);                      /* every item's (resident) */

/* OVL_MENU (vmenu_veranda.c) */
void vmenu_start(void);                 /* the bar: fill vm_buf, DoMenu */
void vmenu_fill(uint8_t title);         /* sub menu `title` into vm_buf; r0 */
void vmenu_about(void);                 /* Veranda Calc > About (its code 0) */

#endif
