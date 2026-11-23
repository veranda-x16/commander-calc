/* vrdir_veranda.c -- the folders on Veranda: where the program works, the
 * file chooser's path, the names a file command opens (OVL_FAR, called
 * through ovl_far(): the file dialog's overlay, OVL_FILEDLG, is full).
 *
 * The program works in a folder that curDirName names from the top folder
 * ("/", "/Household Accounts 2024"), as Veranda's shell keeps it and
 * Archive does. The Open and Save dialog (the SDK's vr_file: ui/
 * filedlg.c) starts there; a file chosen leaves the program in its folder
 * (the overlays are in RAM banks, nothing is loaded by name) and the next
 * Open starts there, as the shell does when the program quits. A file command's name becomes a path from the top (file_name in
 * ui/grid.c, kept as the workbook's name), the program goes to its folder
 * and the command opens the name's own part (file_base) there: every name
 * the DOS sees is a plain one in the current folder, as the .X16S writer's
 * temporary file and its rename need. Bytes 62 and 63 of curDirName are
 * the shell's.
 *
 * The arguments and results are in vr_arg (resident). */
#include "file_io.h"
#include <veranda.h>
#include <string.h>
#include "vaddr.h"

vr_arg_t vr_arg;
uint8_t file_base;                      /* ui/grid.c: file_name's own name */

#pragma code-name (push, "OVERLAY16")
#pragma rodata-name (push, "OVL16RO")
#pragma bss-name (push, "OVL16BSS")

#define DIR_MAX 62                      /* curDirName's path, its 0 included */
#define here    ((char *)curDirName)

/* curDirName a path, or the top folder (the program was not started by
 * the shell: an AUTOSTART); made the current folder: since Veranda's V71
 * the shell starts the program from /VERANDA/APPS with curDirName
 * /DOCUMENTS, its working folder, so the folder it was loaded from is
 * not that one */
void vr_here(void)
{
    uint8_t i;

    if (here[0] == '/')
        for (i = 1; i < DIR_MAX; ++i)
            if (!here[i]) {
                ChDir(here);
                return;
            }
    here[0] = '/';
    here[1] = '\0';
    ChDir(here);
}

/* a file command's name, vr_arg.s (file_name: X16S_DOC_MAX), as a path
 * from the top, the program in its folder, file_base where its own name
 * starts; a path too long to keep stays as it is (file_base 0) */
void vr_place(void)
{
    char t[X16S_DOC_MAX];
    char *name = vr_arg.s, *b;
    uint8_t n;

    file_base = 0;
    vr_here();
    if (name[0] != '/') {               /* the current folder's */
        n = (uint8_t)strlen(here);
        memcpy(t, here, n);
        if (n > 1)
            t[n++] = '/';
        if (n + strlen(name) >= X16S_DOC_MAX - 1)
            return;
        strcpy(t + n, name);
        strcpy(name, t);
    }
    b = strrchr(name, '/');
    n = (uint8_t)(b - name);            /* the folder's length; 0: the top */
    if (n >= DIR_MAX)
        return;
    if (n == 0)
        n = 1;
    memcpy(t, name, n);
    t[n] = '\0';
    if (ChDir(t) != 0)
        return;
    strcpy(here, t);
    file_base = (uint8_t)(b + 1 - name);
}

#pragma bss-name (pop)
#pragma rodata-name (pop)
#pragma code-name (pop)
