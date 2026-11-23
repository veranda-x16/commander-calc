#!/usr/bin/env python3
"""Writes tools/veranda_session2.h and veranda_session3.h: the sessions of
tools/gate_veranda2.sh (Veranda Calc, phase C2: the mouse, the menus, the
dialog boxes) and tools/gate_veranda3.sh (C3: the files).

usage: veranda_script.py [c2|c3|c4|c5|c6|c7|c8 [out.h]]

One session, two ways of playing it:
  - play_script[] for Veranda Calc's test build (src/tools/veranda_play2.s):
    the pointer, its button, the wheel and the keys, as a user gives them
    -- clicks on cells and tabs, on the menu bar's titles and items, on the
    dialog boxes' buttons, keys typed into their fields;
  - host_script[] for the host build (tools/veranda_golden2.c): the same
    session as the X16 build's code sees it -- grid_mouse() for a click,
    grid_key() with the item's code for a menu item, the keys of the X16
    build's own prompts (kbd_host_push) for what was typed into a dialog.
The gate compares the two programs' screens and cells afterwards.

The geometry is Veranda Calc's (Veranda Look 2): the grid's row y at card
row y + 3 (rows 0-1 are the menu bar, 2-3 the title band), so a cell
(r, c) of an unscrolled sheet is on card row 6 + r; columns from card 6,
ten cards wide unless changed; the tabs on card row 57, ten cards each.
The menus are the kernel's Veranda look (the player finds them: vm_buf's
VM_X). The dialog box (src/ui/vdlg_veranda.h): 320 x 96 at (160, 192), its
buttons on y 264-279 at cards 21, 29, 37, 53. The Open and Save dialog is
the SDK's (vrchoose.c): keys alone -- the arrows, Return (into a folder,
or the default button), Esc (Cancel), letters and Backspace into the
name when saving.
"""
import os
import sys

P = []      # Veranda: 5-byte records
H = []      # host: 5-byte records
shots = []
widths = {}

RET, BS = 0x0D, 0x14
# the menus (src/ui/vmenu_veranda.c): title, items
CALC, FILE, EDIT, LAYOUT, DATA, CHART, SHEET = range(7)
M = {
    'bold': (LAYOUT, 5, 0x9C), 'colw': (LAYOUT, 4, 0x9E), 'delrow': (LAYOUT, 1, 0xF9),
    'copy': (EDIT, 2, 0x03), 'paste': (EDIT, 3, 0x16), 'find': (EDIT, 6, 0x06),
    'replace': (EDIT, 7, 0x9F), 'rename': (SHEET, 3, 0xF4), 'add': (SHEET, 2, 0xF2),
    'new': (FILE, 0, 0x85), 'open': (FILE, 1, 0x86), 'save': (FILE, 2, 0x8A), 'saveas': (FILE, 3, 0x8B),
    'import': (FILE, 4, 0x88), 'export': (FILE, 5, 0x8C),
    'border': (LAYOUT, 6, 0x9A), 'select': (EDIT, 1, 0x01),
    'quit': (FILE, 6, 0x10), 'about': (CALC, 0, 0), 'desk': (CALC, 1, 0x80), 'recalc': (DATA, 3, 0xFA),
    'bars': (CHART, 0, 0xFD), 'line': (CHART, 1, 0xFE), 'pie': (CHART, 2, 0xFF),
}
# dialog buttons (vdlg_veranda.h): x of each column's middle, y of the row
BTN_Y = 192 + 72 + 8
ESC = 0x1B          # the Open and Save dialog's Cancel (the SDK's vrchoose.c)
BTN = {'ok': 21 * 8 + 24, 'yes': 21 * 8 + 24, 'no1': 29 * 8 + 24, 'all': 37 * 8 + 24,
       'cancel': 53 * 8 + 24, 'no': 53 * 8 + 24}


def rec(lst, op, *b):
    b = list(b) + [0] * (4 - len(b))
    lst.append([ord(op)] + [v & 255 for v in b])


def p_click(x, y):
    rec(P, 'P', x & 255, x >> 8, y & 255, y >> 8)
    rec(P, 'D')
    rec(P, 'U')


def col_x(c):
    x = 6
    for i in range(c):
        x += widths.get(i, 10)
    return x


def cell(r, c):
    """click the cell (r, c) of the unscrolled sheet"""
    x, y = (col_x(c) + 2) * 8 + 4, (6 + r) * 8 + 4
    p_click(x, y)
    rec(H, 'C', x & 255, x >> 8, y & 255, y >> 8)


def tab(i):
    x, y = (10 * i + 3) * 8, 57 * 8 + 4
    p_click(x, y)
    rec(H, 'C', x & 255, x >> 8, y & 255, y >> 8)


def keys(s):
    """typed into the grid"""
    for ch in s:
        k = ord(ch) if isinstance(ch, str) else ch
        rec(P, 'K', k)
        rec(H, 'K', k)


def menu(name, shot=None):
    """a menu item: the title, then the item; the host: its code"""
    t, i, code = M[name]
    rec(P, 'T', t)
    rec(P, 'D')
    rec(P, 'U')
    rec(P, 'W', 20)
    if shot:
        rec(P, 'S', shot)
    rec(P, 'I', t, i)
    rec(P, 'D')
    rec(P, 'U')
    rec(P, 'W', 40)                     # the flash, then the action
    return code


def dialog(typed, button=None, shot=None, timed=False):
    """keys typed into the open dialog box's field (or none), then a
    button, or RETURN when button is None; the host: the same keys into
    the X16 build's prompt, RETURN in place of the button"""
    for ch in typed:
        k = ord(ch) if isinstance(ch, str) else ch
        rec(P, 'K', k)
        rec(H, 'Q', k)
    if timed:
        rec(P, 'Z')
    if shot:
        rec(P, 'W', 10)
        rec(P, 'S', shot)
    if button:
        p_click(BTN[button], BTN_Y)
    else:
        rec(P, 'K', RET)
    rec(H, 'Q', RET)
    rec(P, 'W', 30)


def confirm(button, host_key):
    p_click(BTN[button], BTN_Y)
    rec(P, 'W', 30)
    rec(H, 'Q', host_key)


def run(code):
    rec(H, 'K', code)


def chooser(downs, timed=False):
    """the file chooser (src/ui/filedlg.c: Veranda's a dialog box, the host's
    the X16 build's in its own key loop): down `downs` times, then RETURN;
    timed: the clock starts at the RETURN (veranda_play2.s 'Z')"""
    for k in [0x11] * downs:
        rec(P, 'K', k)
        rec(H, 'Q', k)
    if timed:
        rec(P, 'Z')
    rec(P, 'K', RET)
    rec(H, 'Q', RET)
    rec(P, 'W', 60)


def took(c):
    """Veranda: "TIME c <cycles>" since the 'Z' (veranda_play2.s 'Y')"""
    rec(P, 'Y', ord(c))


def new_discard():
    """File > New on a workbook that counts as changed (edited, or
    imported): "discard unsaved changes?" Yes"""
    c = menu('new')
    confirm('yes', ord('y'))
    run(c)


def log():
    """both programs log the session's cells now"""
    rec(P, 'L', 1)
    rec(P, 'W', 4)
    rec(H, 'L', 1)


def session2():
    """phase C2: the pointer, the menus, the dialog boxes"""
    keys('12\r30\r=SUM(A1:A2)*2\r')            # A1 A2 A3
    cell(0, 1)
    keys('apple\rpear\rapple pie\r')            # B1 B2 B3
    cell(4, 2)
    keys('7\r8\r')                              # C5 C6
    cell(2, 0)                                  # A3: bold
    run(menu('bold', shot=1))
    run(menu('copy'))                           # A3 to D3
    cell(2, 3)
    run(menu('paste'))
    # Edit > Find: "pix", backspace, "e" -> B3
    c = menu('find')
    dialog(['p', 'i', 'x', BS, 'e'])
    run(c)
    # Layout > Column width: B (the cursor's) to 14
    c = menu('colw')
    dialog([BS, BS, '1', '4'], 'ok', shot=2)
    run(c)
    widths[1] = 14
    # Sheet > Rename: Data; Sheet > Add: Two; to Two by its tab, back
    c = menu('rename')
    dialog([BS] * 6 + list('Data'))
    run(c)
    c = menu('add')
    dialog(list('Two'), 'ok')
    run(c)
    tab(1)
    tab(0)
    # Edit > Replace: apple -> kiwi, yes for B1, no for B3
    c = menu('replace')
    dialog([BS] * 3 + list('apple'))
    dialog(list('kiwi'))
    confirm('yes', ord('Y'))
    confirm('no1', ord('N'))
    run(c)
    # Layout > Delete row on C5: yes (C6 moves up)
    cell(4, 2)
    c = menu('delrow')
    confirm('yes', ord('y'))
    run(c)
    # the wheel: two notches down, one up (the cursor 3 rows a notch): C8
    for n in (1, 1, 0xFF):
        rec(P, 'H', n)
        rec(P, 'W', 10)
        rec(H, 'H', n)
    return [(0, 0), (1, 0), (2, 0), (0, 1), (1, 1), (2, 1), (4, 2), (5, 2), (2, 3)]


def session3():
    """phase C3: the files. The folder holds BUDGET.XLSX and a folder SUB
    with RICH.ODS in it (tools/make_examples.py, tools/make_ods_fixtures.py);
    the session writes T1.X16S (Save as, then Save), T1.CSV and T1.XLSX,
    which the gate compares with the host build's. The file chooser lists
    folders first: SUB, then the files."""
    keys('12\r30\r=SUM(A1:A2)*2\r')            # A1 A2 A3
    cell(0, 1)
    keys('apple\r3.5\r')                        # B1 B2
    cell(0, 2)
    keys('x,y\r')                               # C1: a comma for CSV to quote
    c = menu('saveas')                          # File > Save as: T1.X16S
    dialog(list('T1.X16S'))
    run(c)
    cell(3, 0)
    keys('7\r')                                 # A4, then File > Save: T1.X16S
    run(menu('save'))
    rec(P, 'W', 30)
    c = menu('export')                          # File > Export: T1.CSV
    dialog([BS] * 4 + list('CSV'))
    run(c)
    c = menu('export')                          # File > Export: T1.XLSX
    dialog([BS] * 4 + list('XLSX'))
    run(c)
    rec(P, 'W', 30)
    log()                                       # 1: as typed
    run(menu('new'))
    rec(P, 'S', 1)
    c = menu('open')                            # File > Open: T1.X16S
    rec(P, 'W', 30)
    rec(P, 'S', 2)
    chooser(1)                                  # SUB, T1.X16S
    rec(P, 'W', 200)
    rec(P, 'S', 3)
    run(c)
    log()                                       # 2: read back
    run(menu('new'))                            # (just opened: no question)
    c = menu('import')                          # File > Import: T1.CSV
    rec(P, 'W', 30)
    rec(P, 'S', 6)
    chooser(2)                                  # SUB, BUDGET.XLSX, T1.CSV, T1.XLSX
    rec(P, 'W', 60)
    rec(P, 'S', 4)
    confirm('ok', RET)                          # "Imported: 3 rows, ..."
    run(c)
    log()                                       # 3: the CSV read back
    new_discard()
    c = menu('import')                          # File > Import: BUDGET.XLSX
    chooser(1)
    rec(P, 'W', 60)
    rec(P, 'S', 5)
    confirm('ok', RET)
    run(c)
    log()                                       # 4: the .xlsx
    new_discard()
    c = menu('import')                          # File > Import: SUB/RICH.ODS
    chooser(0)                                  # into SUB
    rec(P, 'W', 30)
    rec(P, 'S', 7)
    chooser(1)                                  # .., RICH.ODS
    confirm('ok', RET)
    run(c)
    # a large workbook, timed: ZBIG.CSV (the gate writes it: BIG_ROWS rows
    # of a number, a fraction, a text and a formula)
    log()                                       # 5: RICH.ODS
    new_discard()
    c = menu('import')                          # Import ZBIG.CSV
    rec(P, 'K', RET)                            # Veranda: the chooser is in SUB,
    rec(P, 'W', 60)                             # where the last file was: ".."
    chooser(4, timed=True)                      # SUB BUDGET T1.CSV T1.XLSX ZBIG.CSV
    rec(P, 'W', 30)
    took('I')
    confirm('ok', RET)
    run(c)
    c = menu('saveas')                          # Save as ZBIG.X16S
    dialog([BS] * 3 + list('X16S'), timed=True)
    run(c)
    took('S')
    c = menu('export')                          # Export ZBIG.XLSX
    dialog([BS] * 4 + list('XLSX'), timed=True)
    run(c)
    took('X')
    run(menu('new'))                            # (saved: no question)
    c = menu('open')                            # Open ZBIG.X16S
    chooser(2, timed=True)                      # SUB T1.X16S ZBIG.X16S
    run(c)
    took('O')
    log()                                       # 6: ZBIG.X16S
    run(menu('new'))
    c = menu('import')                          # Import ZBIG.XLSX
    chooser(5, timed=True)
    rec(P, 'W', 30)
    took('M')
    confirm('ok', RET)
    run(c)
    return [(r, c) for r in range(6) for c in range(4)]


def session4():
    """phase C4: charts and borders. Months in A, numbers in B (one
    negative); Chart > Bars, Line and Pie of B from B1 (stop points 1, 2,
    3: the gate checks each picture), S on the pie writes CHART.BMP; then
    a box (Layout > Border) round A2:B4 and bold B1."""
    keys('Jan\rFeb\rMar\rApr\rMay\r')
    cell(0, 1)
    keys('12\r30\r-5\r22\r17\r')
    cell(0, 1)
    for name, stop, key in (('bars', 1, ' '), ('line', 2, ' '), ('pie', 3, 'S')):
        c = menu(name)
        rec(P, 'W', 60)
        rec(P, 'S', stop)
        rec(P, 'K', ord(key))
        rec(H, 'Q', ord(key))
        rec(P, 'W', 60)
        run(c)
    cell(1, 0)                                  # A2, a block to B4
    run(menu('select'))                         # (Ctrl+A: the menu's, as the
    keys([0x11, 0x11, 0x1D])                    # ROM's $01 is F1 to Veranda)
    run(menu('border'))
    run(menu('select'))
    cell(0, 1)
    run(menu('bold'))
    log()
    return [(r, c) for r in range(6) for c in range(2)]


def session5():
    """phase C5: Veranda's shell opens a workbook with Calc (a double click,
    tools/gate_veranda5.sh): Calc starts with it open, not changed (stop
    point 1: the picture); the right button on A21 edits it (99); Veranda
    Calc > About (stop point 2); File > Quit, "discard?" Yes: back to the
    shell.
    The host: File > Open it (the folder's only workbook)."""
    rec(H, 'Q', RET)
    rec(H, 'K', 0x86)
    rec(P, 'W', 120)                            # (it opens the file itself)
    rec(P, 'S', 1)
    log()                                       # 1: the workbook, DIRTY 0
    x, y = (col_x(0) + 2) * 8 + 4, (6 + 20) * 8 + 4
    rec(P, 'P', x & 255, x >> 8, y & 255, y >> 8)
    rec(P, 'R')
    rec(P, 'U')
    rec(H, 'C', x & 255, x >> 8, y & 255, y >> 8)
    rec(H, 'K', 0x89)
    keys('99\r')
    log()                                       # 2: A21 = 99
    menu('about')
    rec(P, 'W', 30)
    rec(P, 'S', 2)
    p_click(BTN['ok'], BTN_Y)
    rec(P, 'W', 30)
    c = menu('quit')
    confirm('yes', ord('y'))
    run(c)
    return [(r, c) for r in range(8) for c in range(4)] + [(20, 0)]


def session6():
    """phase C6: speed. Imports BIG6.CSV (the gate writes it: 120 rows of
    eight columns, numbers and words); then, each timed by the program
    (main_veranda.c's timed, "KTIME c" on the console): E a full redraw of
    the empty sheet first, then R a full redraw
    (Data > Recalc now: no formulas, so only grid_render_all), D the cursor
    down a row, C right a column, P PgDn (the viewport moves a page, the
    cursor on the bottom row; then its cell boxed and bold, not timed), S
    down (the grid scrolls by a row: the screen as it leaves it is stop
    point 1's), U up a row, R again."""
    def timed(code, key=None, menu_name=None):
        if menu_name:
            run(menu(menu_name))
        else:
            rec(P, 'K', key)
            rec(H, 'K', key)
        rec(P, 'W', 10)
        rec(P, 'X', ord(code))
    timed('E', menu_name='recalc')              # the empty sheet
    c = menu('import')
    chooser(0)
    rec(P, 'W', 60)
    confirm('ok', RET)
    run(c)
    timed('R', menu_name='recalc')
    timed('D', 0x11)
    timed('C', 0x1D)
    timed('P', 0x02)
    run(menu('border'))                         # the cursor's cell boxed and bold:
    run(menu('bold'))                           # the scroll moves its borders too
    timed('S', 0x11)                            # (PgDn left it on the bottom row)
    log()                                       # the screen as the scroll left it:
    rec(P, 'S', 1)                              # the golden's screen1.txt, stop point 1
    timed('U', 0x91)
    timed('R', menu_name='recalc')
    log()
    return [(r, c) for r in range(4) for c in range(4)]


def session7():
    """Veranda's clipboard (src/platform/clip_veranda.c; tools/gate_veranda7.sh):
    the gate puts a Text Scrap in Veranda's system folder (another
    program's: "1 TAB 2 CR 3 TAB =A1+B1 CR"); Edit > Paste at A1 takes it
    (A1:B2, B2 a formula); A1:B2 selected (Edit > Select, right, down),
    Edit > Copy: Calc's clipboard and the Text Scrap (the cells as text,
    TABs and CRs); D1, Edit > Paste: now Calc's own clipboard (the scrap
    is the one Calc wrote): D2 the formula moved, =D1+E1. The host build
    has no Text Scrap: its cells are not compared (the gate checks them)."""
    run(menu('paste'))
    rec(P, 'W', 60)
    log()                                       # 1: A1:B2 from the Text Scrap
    run(menu('select'))
    keys([0x1D, 0x11])
    run(menu('copy'))
    rec(P, 'W', 60)
    keys([0x91, 0x1D, 0x1D])                    # up, right twice: D1
    run(menu('paste'))
    rec(P, 'W', 60)
    log()                                       # 2: D1:E2, Calc's own paste
    # Veranda Calc > Desk accessory: none with the applications here (the
    # gate's folder has no /VERANDA/APPS): the box that says so, OK
    menu('desk')
    rec(P, 'W', 30)
    rec(P, 'S', 1)                              # stop point 1: the box
    p_click(BTN['ok'], BTN_Y)
    rec(P, 'W', 30)
    return [(r, c) for r in range(2) for c in range(5)]


def session8():
    """the file chooser on a card laid out as a user's is
    (tools/veranda_card.py; tools/gate_veranda3.sh, part 2): Veranda's
    shell starts Calc (a pin: /VERANDA/APPS) in /DOCUMENTS, its first
    Open's folder (Veranda's V71); File > Open lists it, ".." first (stop
    point 1); up to the top folder (2: its folders, then Quick sums.X16S);
    into "Household Accounts 2024" (3) and open "Monthly family budget
    version 2.x16s" (log 1); File > Open again starts there (4); up, into
    VERANDA (5), Cancel. File > Save writes the workbook back where it came
    from; File > New, Open: there again, the budget as saved (log 2);
    Save as "Budget copy with a long name" (Calc adds .X16S: 33
    characters), New, Open: the copy (log 3); New, Open: into "Older
    years", Budget 2019.X16S (log 4); New, Open: up twice, Quick
    sums.X16S in the top folder (log 5); New, Open: the top folder again
    (6), Cancel. The five are the same workbook."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import veranda_card as vc
    ext = ('X16S',)
    root = vc.root_rows(ext)
    home = vc.home_rows(ext)
    older = ['..', vc.OLD_BOOK]

    def pick(lst, name, start=0):
        downs = lst.index(name) - start
        for k in [0x11] * downs:
            rec(P, 'K', k)
        rec(P, 'K', RET)
        rec(P, 'W', 60)

    def opened():
        rec(P, 'W', 200)
        log()

    menu('open')
    rec(P, 'W', 30)
    rec(P, 'S', 1)                              # in /DOCUMENTS: ".." only
    pick(['..'], '..')
    rec(P, 'S', 2)                              # the top folder
    pick(root, vc.HOME)
    rec(P, 'S', 3)                              # Household Accounts 2024
    pick(home, vc.BOOK)
    opened()                                    # 1: the budget
    menu('open')
    rec(P, 'W', 30)
    rec(P, 'S', 4)                              # where the last one was
    pick(home, '..')
    pick(root, 'VERANDA')
    rec(P, 'S', 5)
    rec(P, 'K', ESC)                            # Cancel
    rec(P, 'W', 30)
    menu('save')
    rec(P, 'W', 120)
    menu('new')
    rec(P, 'W', 30)
    menu('open')
    rec(P, 'W', 30)
    pick(home, vc.BOOK)
    opened()                                    # 2: the budget as saved
    menu('saveas')                              # Save as a long name, in its
    rec(P, 'W', 30)                             # folder (the field: the name)
    for k in [BS] * len(vc.BOOK) + [ord(ch) for ch in vc.COPY_NAME]:
        rec(P, 'K', k)
    rec(P, 'K', RET)
    rec(P, 'W', 120)
    menu('new')
    rec(P, 'W', 30)
    menu('open')
    rec(P, 'W', 30)
    home = vc.home_rows(ext, copy=True)
    pick(home, vc.COPY)
    opened()                                    # 3: the copy, by its long name
    menu('new')
    rec(P, 'W', 30)
    menu('open')
    rec(P, 'W', 30)
    pick(home, vc.OLDER)
    pick(older, vc.OLD_BOOK)
    opened()                                    # 4: Budget 2019
    menu('new')
    rec(P, 'W', 30)
    menu('open')
    rec(P, 'W', 30)
    pick(older, '..')
    pick(home, '..')
    pick(root, vc.TOP_BOOK)
    opened()                                    # 5: Quick sums
    menu('new')
    rec(P, 'W', 30)
    menu('open')
    rec(P, 'W', 30)
    rec(P, 'S', 6)                              # the top folder
    rec(P, 'K', ESC)                            # Cancel
    rec(P, 'W', 30)
    return [(r, c) for r in range(12) for c in range(4)]


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else 'c2'
    here = os.path.dirname(__file__)
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, 'veranda_session%s.h' % name[1:])
    cells = {'c2': session2, 'c3': session3, 'c4': session4, 'c5': session5,
             'c6': session6, 'c7': session7, 'c8': session8}[name]()
    rec(P, 'W', 20)
    rec(P, 'E')
    rec(H, 'E')
    with open(out, 'w') as o:
        o.write('/* tools/veranda_session%s.h: gate veranda%s\'s session, written by\n'
                ' * tools/veranda_script.py %s (5-byte records: src/tools/veranda_play2.s,\n'
                ' * tools/veranda_golden2.c). */\n' % (name[1:], name[1:], name))
        o.write('/* the cells both programs log: row, column */\n'
                'static const unsigned char play2_cells[] = {\n    %s\n};\n'
                % ', '.join('%d, %d' % rc for rc in cells))
        o.write('#ifdef PLAY2_VERANDA\nconst unsigned char play_script[] = {\n')
        for r in P:
            o.write('    %s,\n' % ', '.join('0x%02X' % v for v in r))
        o.write('};\n#else\nstatic const unsigned char host_script[] = {\n')
        for r in H:
            o.write('    %s,\n' % ', '.join('0x%02X' % v for v in r))
        o.write('};\n#endif\n')
    print('veranda_script %s: %d records for Veranda, %d for the host' % (name, len(P), len(H)))


if __name__ == '__main__':
    main()
