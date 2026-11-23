#!/usr/bin/env python3
"""Build .xlsx fixtures for the importer tests.

Written with python's zipfile rather than a spreadsheet, so the parts are
exactly the shapes Excel and LibreOffice emit — including the ones that
differ between them, which is where importers break.
"""
import base64, sys, zipfile

out = sys.argv[1] if len(sys.argv) > 1 else "build/host/sd"

RELS = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'

def book(sheets, extra=""):
    s = "".join(
        f'<sheet name="{n}" sheetId="{i+1}" r:id="rId{i+1}"{st}/>'
        for i, (n, st) in enumerate(sheets))
    return (RELS + '<workbook xmlns:r="http://schemas.openxmlformats.org/'
            'officeDocument/2006/relationships">'
            f'<fileVersion appName="xl"/><sheets>{s}</sheets>{extra}'
            '</workbook>')

def rels(n, extra=""):
    r = "".join(
        f'<Relationship Id="rId{i+1}" Type="http://schemas.openxmlformats.org'
        f'/officeDocument/2006/relationships/worksheet" '
        f'Target="worksheets/sheet{i+1}.xml"/>' for i in range(n))
    return RELS + f'<Relationships>{r}{extra}</Relationships>'

def shared(items):
    body = "".join(items)
    return (RELS + f'<sst count="{len(items)}" uniqueCount="{len(items)}">'
            f'{body}</sst>')

def sheet(rows=""):
    return RELS + f'<worksheet><sheetData>{rows}</sheetData></worksheet>'

# --- a normal three-sheet workbook, the way Excel writes one -----------
with zipfile.ZipFile(f"{out}/BOOK.XLSX", "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("[Content_Types].xml", RELS + "<Types/>")
    z.writestr("_rels/.rels", RELS + '<Relationships><Relationship Id="rId1"'
               ' Type="http://schemas.openxmlformats.org/officeDocument/2006'
               '/relationships/officeDocument" Target="xl/workbook.xml"/>'
               '</Relationships>')
    z.writestr("xl/workbook.xml",
               book([("Budget", ""), ("Inventory", ""), ("Summary", "")],
                    '<definedNames/><calcPr calcId="1"/>'))
    z.writestr("xl/_rels/workbook.xml.rels",
               rels(3, '<Relationship Id="rId4" Type="http://schemas.'
                       'openxmlformats.org/officeDocument/2006/relationships'
                       '/sharedStrings" Target="sharedStrings.xml"/>'))
    z.writestr("xl/sharedStrings.xml", shared([
        "<si><t>Name</t></si>",
        "<si><t>Quantity</t></si>",
        # rich text: several runs, which must concatenate
        "<si><r><rPr><b/></rPr><t>Hello</t></r><r><t> world</t></r></si>",
        # the entity and unicode paths
        "<si><t>Smith &amp; Co</t></si>",
        "<si><t>café €5</t></si>",
        # whitespace that Excel marks as significant
        '<si><t xml:space="preserve"> padded </t></si>',
    ]))
    for i in range(1, 4):
        z.writestr(f"xl/worksheets/sheet{i}.xml", sheet())

# --- a hidden sheet, and a target spelled absolutely ------------------
with zipfile.ZipFile(f"{out}/ODD.XLSX", "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("xl/workbook.xml",
               book([("Visible", ""), ("Gone", ' state="hidden"')]))
    z.writestr("xl/_rels/workbook.xml.rels", RELS + '<Relationships>'
               '<Relationship Id="rId1" Type="x/worksheet" '
               'Target="/xl/worksheets/sheet1.xml"/>'
               '<Relationship Id="rId2" Type="x/worksheet" '
               'Target="worksheets/sheet2.xml"/></Relationships>')
    z.writestr("xl/sharedStrings.xml", shared([]))

# A workbook LibreOffice wrote, kept whole rather than rebuilt: borders in
# an .xlsx are a <borders> list in styles.xml that <cellXfs> entries index
# by number, and this is a real example of that arrangement.
LO_BOX_XLSX = (
    "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"
    "SwUGAAAAAAoACgCAAgAApRMAAAAA"
)


def write_box(out):
    with open("%s/BOX.XLSX" % out, "wb") as f:
        f.write(base64.b64decode(LO_BOX_XLSX))


write_box(out)
print("wrote BOOK.XLSX, ODD.XLSX and BOX.XLSX to", out)

