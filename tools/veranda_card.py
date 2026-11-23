#!/usr/bin/env python3
"""veranda_card.py -- an SD card laid out as a real one is, for the file
chooser's gate (tools/gate_veranda3.sh, part 2; tools/veranda_script.py c8).

    veranda_card.py make <dir>     the card's folders and files under <dir>
                                   (Veranda's own files are the gate's to add:
                                   <dir>/VERANDA/ is made empty)

A user's card, not a test folder: Veranda in /VERANDA (Veranda's
layout, its DECISIONS V71: the applications in /VERANDA/APPS, the user's
files in /DOCUMENTS, where the shell opens), and in the top
folder some 35 folders and 30 files of everything else -- names long and
short, with spaces, upper and lower case, the hidden ones other machines
leave (".Spotlight-V100", ".Trash-1000", "._name"), "System Volume
Information". The workbooks are in "Household Accounts 2024" (23
characters, spaces) and one level further down, under names of up to 36
characters; one is in the top folder. A folder's name of 47 characters,
the most a row keeps, is too wide for the list: it is cut, "..." at its end.

The chooser (the SDK's Open and Save dialog, vrchoose.c, with the
command's type filter: ui/filedlg.c) lists folders first, then the files
with the command's extensions, each group sorted by name with case
ignored, and leaves out names that start with '.'; rows() gives the same
list, which the session uses to know where a name is.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

HOME = 'Household Accounts 2024'
OLDER = 'Older years'
BOOK = 'Monthly family budget version 2.x16s'        # 36 characters
OLD_BOOK = 'Budget 2019.X16S'
TOP_BOOK = 'Quick sums.X16S'
COPY_NAME = 'Budget copy with a long name'          # Save as (the session)
COPY = COPY_NAME + '.X16S'                          # 33 characters: Calc adds .X16S

ROOT_DIRS = [
    '101rush', 'APPS', 'BASIC-DEMOS', 'GAMES', 'c64', '.Spotlight-V100',
    'System Volume Information', '.Trash-1000', '.fseventsd', 'DEMOS', 'MUSIC',
    'PETSCII art', 'prog8-examples', 'assembly', 'cc65 projects', 'x16-docs',
    'zsound', 'TOOLS', 'UTILS', 'Old backups from the other card', 'PHOTOS',
    'WORK', 'tmp', 'ZSMKIT', 'chiptunes', 'Emulators', 'fonts', 'GRAPHICS',
    'Sprites', 'TEXT', 'x16edit', 'BANKS', 'SOUNDS', 'basload', 'Paint files',
    'VERANDA', 'DOCUMENTS', HOME, 'A folder with a much longer name than fits here',
]
ROOT_FILES = [
    'FSCK0000.REC', 'FSCK0001.REC', 'petsciipit.bas', '._petsciipit.bas',
    'README.TXT', 'chess.prg', 'CRYSTAL.PRG', 'notes from the meeting.txt',
    'budget.csv', TOP_BOOK, 'BOOT.BAS', 'x16edit-0.7.6.prg', 'ROM-R49.BIN',
    'mandelbrot.bas', 'MAZE.PRG', 'invaders.prg', 'tetris.prg',
    'A list of things to do this week.txt', 'IndexerVolumeGuid', '.DS_Store',
    'helloworld.prg', 'SNAKE.PRG', 'piano.prg', 'demo.zsm', 'title.bin',
    'palette.pal', 'sprites.bin', 'CONFIG.TXT', 'wifi.txt', 'Desktop.ini',
]


def x16s(path):
    import make_x16s as m
    b = m.Book()
    m.budget(b)
    b.write(path)


def make(top):
    for d in ROOT_DIRS:
        os.makedirs(os.path.join(top, d), exist_ok=True)
    for i, f in enumerate(ROOT_FILES):
        if f == TOP_BOOK:
            x16s(os.path.join(top, f))
        else:
            with open(os.path.join(top, f), 'wb') as o:
                o.write(bytes((i * 7 + k) & 255 for k in range(100 + 37 * i)))
    for d in ROOT_DIRS:                 # a file or two in the others
        if d not in ('VERANDA', 'DOCUMENTS', HOME):
            with open(os.path.join(top, d, 'file.txt'), 'wb') as o:
                o.write(b'x' * 10)
    h = os.path.join(top, HOME)
    os.makedirs(os.path.join(h, OLDER), exist_ok=True)
    x16s(os.path.join(h, BOOK))
    x16s(os.path.join(h, OLDER, OLD_BOOK))
    with open(os.path.join(h, 'Receipts.txt'), 'wb') as o:
        o.write(b'receipts\r\n')


def rows(names, dirs, exts, top):
    """the chooser's rows: ".." unless at the top, the folders, then the
    files with one of exts, each group by name, case ignored; not the
    names that start with '.'"""
    key = lambda s: s.lower().encode()
    out = [] if top else ['..']
    out += sorted((d for d in dirs if not d.startswith('.')), key=key)
    out += sorted((f for f in names if not f.startswith('.')
                   and os.path.splitext(f)[1][1:].upper() in exts), key=key)
    return out


def root_rows(exts):
    return rows(ROOT_FILES, ROOT_DIRS, exts, True)


def home_rows(exts, copy=False):
    """copy: after the session's Save as (COPY)"""
    return rows([BOOK, 'Receipts.txt'] + ([COPY] if copy else []), [OLDER], exts, False)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == 'make':
        make(sys.argv[2])
    else:
        sys.exit(__doc__)
