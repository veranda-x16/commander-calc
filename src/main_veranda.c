/* main_veranda.c -- Veranda Calc: start-up, the keys, the menus, the
 * pointer (make veranda).
 *
 * Veranda runs the main loop; a program sets itself up, returns, and is
 * called back. main() here is main.c's start-up turned inside out:
 *   - keys: keyVector, with keyData and keyDataMods (API level 2)
 *     translated back to the codes grid.c has always had (the X16 ROM's
 *     ISO codes)
 *   - menus: Veranda's (DoMenu), made by the menu overlay
 *     (ui/vmenu_veranda.c); an item is the grid_key() code it stands for
 *   - the pointer: otherPressVec gives grid_mouse() each press and release
 *     off the menus; appMain the wheel
 * The questions the commands ask (find, replace, column width, the
 * sheet's name, confirmations) are Veranda's dialog boxes (ui/vdlg_veranda.h);
 * Open, Import, Save as and Export the SDK's Open and Save dialog
 * (ui/filedlg.c, overlay 22).
 *
 * Veranda Look 2 (platform/look_veranda.c): Look 2's palette, the kernel's
 * menu bar (theme_menus: "Veranda Calc" first, its About and desk
 * accessories), the title band with the document's name, the status bar;
 * the grid's colours mapped onto the palette (shadow_veranda.s).
 *
 * Started by the shell with a document (r0L & ST_LD_DATA: a double click
 * on a .X16S, .XLSX, .ODS or .CSV; cfg/veranda.hdr's documents), the
 * program goes to the document's folder (curDirName) and opens it (.X16S)
 * or imports it by its whole name (dataLongName, API 5), as File > Open or
 * Import would without the chooser (calc_doc: ui/filedlg.c). The right button edits the cell under it.
 *
 * Memory: overlays in banks of their own (overlay_veranda.c); the heap
 * the largest run of banks Veranda gives, less 22 for a desk accessory
 * (ui/vmenu_veranda.c, vr_heap). */
#include "x16sheet.h"
#include "ui/grid.h"
#include "platform/keyboard.h"
#include "platform/bankmem.h"
#include "platform/banked_ram.h"
#include "platform/overlay.h"
#include "workbook/workbook.h"
#include "util/number_prim.h"
#include "ui/vmenu_veranda.h"
void look_desk(void);                   /* platform/look_veranda.c */
#include "platform/mouse.h"
#include <veranda.h>
#include <vr.h>
#include <string.h>

extern uint8_t ovl_init(void);          /* overlay_veranda.c */
extern uint8_t *const stack_floor;      /* stackfloor_x16.s */
#define STACK_FLOOR (stack_floor[0])
#define CANARY 0x5A

grid_action_t clip_key(uint8_t k);      /* platform/clip_veranda.c */

err_t vr_heap(void);                    /* OVL_MENU: vmenu_veranda.c */
uint8_t vr_doc(void);
void vr_status(void);

#ifdef CALC_PLAY
void play_start(void);                  /* tools/veranda_play.c */
#endif
extern uint8_t calc_idle;               /* keyboard_veranda.c */
#ifdef CALC_PLAY2
extern uint8_t play_log;                /* tools/veranda_play2.s */
void play2_start(void);                 /* tools/veranda_play2.c */
void play2_log(void);
#endif

void look_start(void);                  /* platform/look_veranda.c (OVL_CLIP) */
void look_cd(void);

static const char S_stack[] = "STACK!";

/* Veranda Look 2 (platform/look_veranda.c): the title band to draw again
 * (grid_render_all); the folder vr_cd made current */
uint8_t look_dirty;
uint8_t look_cd_arg;
#pragma bss-name (push, "VRFBSS")       /* (the dialog's: cfg/veranda.cfg's */
char look_cd_path[18];                  /* VRFB; only it calls vr_cd) */
#pragma bss-name (pop)

/* the SDK's vr_cd (vrpath.h), for the Open and Save dialog's pop-up
 * (overlay 22): its body in overlay 21 (look_cd), the path here */
const char *vr_cd(unsigned char where)
{
    look_cd_arg = where;
    ovl_far21(look_cd);
    return look_cd_path;
}
char calc_status[19];
uint8_t calc_doc;                       /* 1: filedlg_ask takes dataFileName */
static uint8_t rdown;                   /* the right button, last pass */
uint8_t calc_heap_first, calc_heap_banks;
uint8_t vm_buf[VM_SIZE];                /* the menus: vmenu_veranda.h */

/* The key's translation (platform/keymap_veranda.c), in OVL_CHARTOUT's
 * bank: on_key calls it through ovl_far, keyData and keyDataMods in
 * ck_k and ck_m, the grid's code back in ck_k. */
uint8_t ck_k, ck_m;
void calc_key_far(void);

#ifdef CALC_PLAY2
void play_mark0(void);                  /* veranda_play2.s: the clock, for 'X' */
void play_mark1(void);
/* a command, timed (EMU_CLOCK): the test builds' gate veranda6 */
static grid_action_t timed(uint8_t k)
{
    grid_action_t a;

    play_mark0();
    a = grid_key(k);
    play_mark1();
    return a;
}
#  define grid_key timed
#endif

/* After anything the grid did: Quit, and the stack's canary. */
static void after(grid_action_t a)
{
    if (a == GRID_QUIT) {
#ifdef CALC_PLAY2
        mouseSource = 0;                /* (the player goes with the program) */
#endif
        EnterDeskTop();
    }
    if (STACK_FLOOR != CANARY) {
        grid_set_status(S_stack);
        STACK_FLOOR = CANARY;
    }
}

/* a command: Copy and Paste also use Veranda's clipboard (clip_veranda.c;
 * clip_key passes the other keys to grid_key, but the timed test builds
 * time grid_key's) */
#ifdef CALC_PLAY2
static grid_action_t command(uint8_t k)
{
    return k == 0x03 || k == 0x16 ? clip_key(k) : grid_key(k);
}
#else
#  define command clip_key
#endif

static void on_key(void)
{
    /* ovl-loads: OVL_CHARTOUT (calc_key_far, through ovl_far) */
    ck_k = keyData;
    ck_m = keyDataMods;
    ovl_far(calc_key_far);
    if (ck_k)
        after(command(ck_k));
}

/* The menus (vmenu_veranda.h): a title chosen, then an item */
void vm_sub(void)
{
    uint8_t t = r8L;

    ovl_require(OVL_MENU);
    vmenu_fill(t);
}

void vm_act(void)
{
    uint8_t k = vm_buf[VM_CODES + r8L];

    GotoFirstMenu();
    if (k == 0) {                       /* Veranda Calc > About */
        ovl_require(OVL_MENU);
        vmenu_about();
        return;
    }
    if (k == VM_DESK) {                 /* Veranda Calc > Desk accessory */
        ovl_far21(look_desk);
        return;
    }
    after(command(k));
}

/* The pointer: a press or a release off the menus (otherPressVec), in
 * the grid's units (its row y is card row y + 3: screen_veranda.c). The
 * menu bar's and the title band's card rows (0-3) are not the grid's. */
static void at_mouse(uint8_t b)
{
    uint16_t x, y;

    do {
        x = mouseXPos;
        y = mouseYPosW;
    } while (x != mouseXPos || y != mouseYPosW);
    if (y >= VM_GRID_Y)
        after(grid_mouse(x, y - (VM_GRID_Y - 8), b, 0));
}

static void on_press(void)
{
    at_mouse((mouseData & 0x80) ? 0 : MOUSE_LEFT);
}

/* The wheel (appMain: every pass of the main loop) */
static void on_main(void)
{
    int8_t w;

    __asm__("sei");
    w = (int8_t)mouseWheel;
    mouseWheel = 0;
    __asm__("cli");
    if (w)
        after(grid_mouse(0, 0, (uint8_t)(mouseButtons & MOUSE_LEFT), w));
    /* the right button: a click on the cell, then Edit cell (F2) */
    w = (int8_t)(mouseButtons & MOUSE_RIGHT);
    if (w && !rdown) {
        at_mouse(MOUSE_LEFT);
        at_mouse(0);
        after(grid_key(0x89));
    }
    rdown = (uint8_t)w;
#ifdef CALC_PLAY2
    if (play_log) {
        ovl_require(OVL_PLAY);
        play2_log();
    }
#endif
    calc_idle = 1;                      /* idle (a test's script may go on) */
}

#define say(s) LogText((char *)(s))     /* (resident RODATA: below $A000) */

int main(void)
{
    uint8_t doc = (uint8_t)(r0L & ST_LD_DATA);  /* (before anything uses r0) */

    golden_init();
    if (ovl_init() == 0) {
        say("CALC NO OVERLAYS");
        return 0;
    }
    ovl_require(OVL_MENU);              /* the probe and start-up are in it */
    say(snprim_zp_probe() == 0 ? "ZP OK" : "ZP CHANGED");
    ovl_require(OVL_CHARTOUT);          /* start-up's other half */
    if (screen_init() != ERR_OK || vr_heap() != ERR_OK
        || wb_init() != ERR_OK || grid_init() != ERR_OK) {
        say("CALC START FAILED");
        EnterDeskTop();
    }
    STACK_FLOOR = CANARY;
    ovl_require(OVL_CHARTOUT);          /* (wb_init, grid_init may map others) */
    vr_status();                        /* start-up's: "<n> bytes free", the status */
    ovl_far21(look_start);              /* Veranda Look 2: before the menus */
    ovl_require(OVL_MENU);
    vmenu_start();
    keyVector = VR_FN(on_key);
    otherPressVec = VR_FN(on_press);
    appMain = VR_FN(on_main);
#if defined(CALC_PLAY)
    ovl_require(OVL_PLAY);              /* the player is in overlay 20 */
    play_start();                       /* its own status, the first render */
#elif defined(CALC_PLAY2)
    ovl_require(OVL_PLAY);
    play2_start();
#else
    grid_render_all();
#endif
    say("CALC RUNNING");
    if (doc) {                          /* a document from the shell */
        ovl_require(OVL_CHARTOUT);      /* (start-up's overlay) */
        doc = vr_doc();
        if (doc)
            after(grid_key(doc));
    }
    return 0;
}
