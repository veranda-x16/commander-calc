#!/usr/bin/env python3
"""Write the .ods fixtures the host tests read.

The XML follows what LibreOffice writes rather than what the specification
allows: the standard namespace prefixes, `office:value` carrying the number
while `<text:p>` carries its formatted form, dates as ISO strings, and the
repeat counts that stand in for the empty part of every sheet.

Packaged as a ZIP with `mimetype` stored first and uncompressed, as the ODF
package specification requires.
"""
import base64
import sys
import zipfile
import zlib

NS = (
    'xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" '
    'xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0" '
    'xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" '
    'xmlns:calcext="urn:org:documentfoundation:names:experimental:calc:'
    'xmlns:calcext:1.0"'
)

STYLE_NS = (
    'xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" '
    'xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"'
)

MIME = "application/vnd.oasis.opendocument.spreadsheet"


def cell(vtype=None, value=None, text=None, formula=None, repeat=None,
         extra=""):
    a = ""
    if formula:
        a += ' table:formula="%s"' % formula
    if repeat:
        a += ' table:number-columns-repeated="%d"' % repeat
    if vtype:
        a += ' office:value-type="%s"' % vtype
    if value is not None:
        attr = {"date": "office:date-value", "time": "office:time-value",
                "boolean": "office:boolean-value"}.get(vtype, "office:value")
        a += ' %s="%s"' % (attr, value)
    a += extra
    if text is None:
        return "<table:table-cell%s/>" % a
    return "<table:table-cell%s><text:p>%s</text:p></table:table-cell>" % (
        a, text)


def row(cells, repeat=None):
    a = ' table:number-rows-repeated="%d"' % repeat if repeat else ""
    return "<table:table-row%s>%s</table:table-row>" % (a, "".join(cells))


def table(name, rows):
    return '<table:table table:name="%s">%s</table:table>' % (name,
                                                              "".join(rows))


def content(tables):
    return ('<?xml version="1.0" encoding="UTF-8"?>'
            '<office:document-content %s office:version="1.3">'
            '<office:body><office:spreadsheet>%s'
            '<table:named-expressions/>'
            '</office:spreadsheet></office:body>'
            '</office:document-content>' % (NS, "".join(tables)))


def write_ods(path, xml):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        # Stored, first, and uncompressed: the package specification.
        z.writestr(zipfile.ZipInfo("mimetype"), MIME,
                   compress_type=zipfile.ZIP_STORED)
        z.writestr("META-INF/manifest.xml",
                   '<?xml version="1.0" encoding="UTF-8"?>'
                   '<manifest:manifest xmlns:manifest="urn:oasis:names:tc:'
                   'opendocument:xmlns:manifest:1.0" '
                   'manifest:version="1.3">'
                   '<manifest:file-entry manifest:full-path="/" '
                   'manifest:media-type="%s"/>'
                   '<manifest:file-entry manifest:full-path="content.xml" '
                   'manifest:media-type="text/xml"/>'
                   '</manifest:manifest>' % MIME)
        z.writestr("content.xml", xml)


def bordered():
    """Borders, both ways round: named on the cell, and defaulted from the
    column. The style names are the ones Calc generates."""
    styles = (
        '<style:style style:name="cAll" style:family="table-cell">'
        '<style:table-cell-properties fo:border="0.06pt solid #000000"/>'
        '</style:style>'
        '<style:style style:name="cBot" style:family="table-cell">'
        '<style:table-cell-properties fo:border-bottom="0.06pt solid #000000"'
        ' fo:border-left="none" fo:border-right="none"'
        ' fo:border-top="none"/></style:style>'
        '<style:style style:name="cTL" style:family="table-cell">'
        '<style:table-cell-properties'
        ' fo:border-left="0.06pt solid #000000"'
        ' fo:border-top="0.06pt solid #000000"/></style:style>'
        '<style:style style:name="cTR" style:family="table-cell">'
        '<style:table-cell-properties'
        ' fo:border-right="0.06pt solid #000000"'
        ' fo:border-top="0.06pt solid #000000"/></style:style>')
    cols = ('<table:table-column table:default-cell-style-name="cBot"/>'
            '<table:table-column table:number-columns-repeated="2"/>')
    rows = [
        # Column A defaults to an underline; B and C say nothing.
        row([cell("string", text="under"), cell("string", text="plain"),
             cell("string", text="plain")]),
        # ...and this one opts out of the column default by name.
        row([cell("string", text="opted out", extra=' table:style-name="Default"'),
             cell("float", "2", "2", extra=' table:style-name="cTL"'),
             cell("float", "3", "3", extra=' table:style-name="cTR"')]),
        # A box on all four sides, and an EMPTY cell that is only a border.
        row([cell("string", text="boxed", extra=' table:style-name="cAll"'),
             cell(extra=' table:style-name="cAll"'),
             cell("float", "9", "9")]),
    ]
    return ('<?xml version="1.0" encoding="UTF-8"?>'
            '<office:document-content %s %s office:version="1.3">'
            '<office:automatic-styles>%s</office:automatic-styles>'
            '<office:body><office:spreadsheet>'
            '<table:table table:name="Bord">%s%s</table:table>'
            '</office:spreadsheet></office:body>'
            '</office:document-content>'
            % (NS, STYLE_NS, styles, cols, "".join(rows)))


def rich():
    """Every value type, a formula of each shape, and the repeats."""
    first = [
        row([cell("string", text="Name"), cell("string", text="Qty"),
             cell("string", text="Price"), cell("string", text="Total")]),
        row([cell("string", text="Cable &amp; Plug"),
             cell("float", "4", "4"), cell("float", "8.5", "8.5"),
             cell("float", "34", "34", formula="of:=[.B2]*[.C2]")]),
        row([cell("string", text="Adapter"), cell("float", "2", "2"),
             cell("float", "15", "15"),
             cell("float", "30", "30",
                  formula="of:=ROUND([.B3]*[.C3];2)")]),
        # The gap Calc writes as a repeat, then a range formula.
        row([cell("string", text="Total"), cell(repeat=2),
             cell("float", "64", "64",
                  formula="of:=SUM([.D2:.D3])")]),
        # The typed cells, and the trailing run that fills the sheet out to
        # the right-hand edge of the address space.
        row([cell("percentage", "0.175", "17.50%"),
             cell("currency", "1234.5", "$1,234.50",
                  extra=' office:currency="USD"'),
             cell("date", "2024-03-15", "2024-03-15"),
             cell("boolean", "true", "TRUE"),
             cell("time", "PT10H30M00S", "10:30:00"),
             cell(repeat=16379)]),
        # A blank row written once, standing for a thousand of them.
        row([cell(repeat=16384)], repeat=1000),
        # ...and something after it, so the row count has to be right.
        row([cell("string", text="After the gap")]),
    ]
    second = [
        row([cell("string", text="Second sheet"),
             cell("float", "5", "5", formula="of:=[First.B2]+1"),
             cell("float", "9", "9", formula="of:=[$First.$B$2]*[.B1]")]),
    ]
    return content([table("First", first), table("Second", second)])


def odd():
    """The awkward ones: an annotation, a repeated row with content in it,
    a covered cell from a merge, and text split across spans."""
    rows = [
        row([cell("string", text="plain"),
             # An annotation carries text:p elements of its own, which must
             # not end up in the cell.
             ('<table:table-cell office:value-type="string">'
              '<office:annotation><text:p>a note</text:p>'
              '</office:annotation>'
              '<text:p>kept</text:p></table:table-cell>'),
             # Rich text: one paragraph, several spans.
             ('<table:table-cell office:value-type="string">'
              '<text:p>a<text:span>b</text:span>c</text:p>'
              '</table:table-cell>')]),
        # A merged cell: the anchor spans two columns and the one it covers
        # is written as covered, so the column count stays honest.
        row([('<table:table-cell table:number-columns-spanned="2" '
              'table:number-rows-spanned="1" office:value-type="string">'
              '<text:p>merged</text:p></table:table-cell>'),
             '<table:covered-table-cell/>',
             cell("float", "7", "7")]),
        # Three identical rows written once.
        row([cell("string", text="three"), cell("float", "1", "1")],
            repeat=3),
        row([cell("string", text="end")]),
    ]
    return content([table("Odd", rows)])


LO_BOX = (
    "eNrlWdtu4zYQfe9XCFqgbzTtONm1VduLbooCbZMC3aRAX2lxZBNLiQJJ3/6+pCjJtCI5Sh2k"
    "LZIHBxqemTlzITmyZ5/3KQ+2IBUT2TwcDYZhAFksKMtW8/DPx5/RJPy8+G4mkoTFEFERb1LI"
    "NIpFps3/wGhnKsolKPNEdGFkI7NIEMVUlJEUVKTjSOSQVbrRU52o8OvksVJjPQ/XWucRxrvd"
    "brAbD4Rc4cev2K4hDXuNK/RKUsrb0FfD4RivMCWaoC2D3YdKY7/WaavGaDqd4mK1hirWYXqE"
    "/7q/e4jXkBLEMqVJFsNRiz6vVYMTIVPVgb/CbrkCU5F2WjYIBFuT0BqtYsly3bccDu0Xwrru"
    "q22xvm5K9LojxxN8bxaLj/u7Cu/aq6+3shk9f0KI2p1VcIgqO9fYPR8j6+tprzhKhOn2NDeN"
    "uuRNp7uzXneSaZB1qTnLvnU3nl2tCy3J7mxIoyG2GI9JfJZJTHhcGz9C843kBYjGGDjYwBUe"
    "DUb19rJ6ZruV6ZKr+gBIxCajbu+6/ME+B8nsEuGFWnRiwc+b0gfeu9YF2NfWRz7PKuuGZ5H0"
    "bzGjd1X7JKb053tsiguQX8G+vizWHLc+USrHtL/6mPq6MtdneN5gCbmQ2t/l+9JVDW6rqRD2"
    "iBE0MQee6WqRd2x9tV31ru121bG14jWRvatcgE9axFaid4+Qhu8UNOmrbLG+Lhf/YLeUOfcs"
    "+CazTboE2bsZzH33ZMskDDjtV2OBUtUssNV25kpNb1y4DhfVbOBuEIVrQWJmBJSQGBCFmKvF"
    "zBGrxYF7tjzm4R0zURYpCh5IZi4x0xwVNGX8MA+/J7lQPzRwThgGJ6YtHq0gM4GZs07tmFIn"
    "iJzp2FxPWyJZsWfxi6iNXpPbQWlILyH3u9Ci8Bbc/vpb8HDbSa4JfDV2uKvcpZxstDDzAItR"
    "Yafug+LzJJZYjGpnJfdid5pDgm/SLKw0fSHKTaeC1AxUkIhoKYF8Q0swp5IxaF1XFkv4jlE7"
    "mgwHk8mUZQV/j043N9nFTYpdg5iR+KzckhWuga3W2joffZoY5+cJbxQgkWuWEo58bS030J+3"
    "Ju28K2FqjguQKCcrQE7jJ0jIhutGUF5A7sSkTOWcHEo+pTU78ZirDKWCGktcIr3sTzX+kfOO"
    "+sNxJSfSvoAUD2cZW6VmdwhJ7Vk6HAw/5jpQgjMafBgWfy/g+UXot+CJlkJrO/K30vVwHBLT"
    "F5nIwJdK1y5NsRZ5Kewf8OPdm8bbpOziey4L5+K9sOCPX/8D8XfU97m0vEb8cPO++h0+/i/O"
    "Ifj0vrclTN7FtsSdg1S5sBT0cBzDczNRULUG0IuZu6qLz/Ladin4IvZhKfAzY4eFE6VyyGqB"
    "FrNaabJ4QymhCpn3SyAazAvHuEJQl3GX4xMr5mTBpx7NqNPizo5fDWbGVv1CQvgGkD7kBqm0"
    "NBNIGFRfPLSsGUN2JV/8YsbbGS4fZrhp/vUd/qEPb+rvVih91iFupP6CWjwFVhvt0ijMnMmy"
    "l+WtpWHtadlCJOGCNCjOw6t2Yg5b87q6nNOkP6dxL07jf6/cxbV5YamXYg/00rSe67vW1F73"
    "Su31m/O66cXr5oUl9yUVb0uQItjbX2XsdzvKe7E/uVHwyZ2DO34XWvwNR7tqeA=="
)

# --- files LibreOffice wrote -------------------------------------------
#
# Their content.xml, zlib-compressed and base64'd so the tests run without
# LibreOffice installed. They check the hand-built fixtures above against
# real output.
#
# LOSHEET: a CSV imported and saved, with formulas.
# LOTYPES: dates, percentages, currency and quoted text, typed by Calc.
# LOBOX:   borders, including the heading-row underline Calc writes as a
#          column default with table:style-name="Default" on the cells
#          below to opt them out.

LO_SHEET = (
    "eNrVWW1vIjcQ/t5fgbZS1VYyZtnNFWjIqU3UD21yapucVCnKB7P2gnXe9cr2Qvj3He8bC2GJ"
    "uQu094UI+5mZZ148HpPL98+J6C2Z0lymU8/vD7weSyNJeTqfeh8ffkMj7/3VN5cyjnnEJlRG"
    "ecJSgyKZGvjbA+lUTzLFNHwjplCSq3QiieZ6kpKE6YmJJjJjaS07eSkzKeyW65HWgZl6C2Oy"
    "Ccar1aq/CvpSzfHD39juIcOeDa7Rc0Wp2IceDgYBnmNKDEFLzlbf1hLPC5PslfDH4zEudhuo"
    "5h2qffzP3e19tGAJQTzVhqQR20jR16UacCxVojvwQ1xu12Aqk07NgEBsCQFt0DpSPDOu6SjR"
    "7URY067SFtuWTYhZdMR4hO9gs/i4u63xZXm5WquKsWVPStmYswIloo5OiMvvG89cLT1rgWIJ"
    "1Z5kUKgzsWt0ddDqSnHDVJNqwdNP3YVnd5tEK7I66JI/wBbTYhIdZBIRETXKN9AsV6IA0Qgz"
    "wazjGvt9vzleVg6OWxUuNW8aQCzzlJZnt4wfe86Y4naLiEJssqWhHTdt1sI51wW4LW02fF4V"
    "NjuWZexeYiA3bGwSSP3hGhvjAtTOoKsti4V22yZKVUDdxQPallWZOcDzAiuWSWXap/y5MtWA"
    "9+VUSttiJI2h4UFVy6zj6Ovl3Dm3y3nH0YoWRDlnuQBvlYjNhHONkB3bCTPEVdhi27JCfsZp"
    "qWLe0tBWmebJjCnnYoD77sWRiTkT1C3HEiV6N8FWulRXSbbGhdC7qmeD8gbRuFmIYUZAMYkY"
    "oiwS+uqyJNYs98rvlsfUu+XgZRGi3j1J4RKD4qihCRfrqfcdyaT+eQdXLnq9LdUWj+YsBceg"
    "1+kV13oLkXETwfW0JIoXZxYfRc1/S25rbVjyJeQ+SCMLa73r3//o3V93ktsFvhk73JXuap3k"
    "RsI8wCNU6GnqoPjc8iWSfmOs4l6cTmgSIk9Sr5ZsL6IMKpUpw5nuxXIyU4x8QjMGXQkUWtO1"
    "xgq+4tSOJoP+u0FwwdPCgRafQ+SG5yMXhO+OJBecj1wYDsfHkQvPSC7wjyR3cT5yo9FR3FTX"
    "eVBytUMMVtqsyi27uGB8vjDWuP/TCIwfJpxrhmRmeEIEaksblTN33obs510vJnBFMYUyMmeo"
    "lLhhMcmF2XGq5VB5S1OuM0HWFZ9Km52yYXxCiaSgSShkZi+p4s5mVG3MJF1vrjJ4nBKqF4yZ"
    "q8vStJ1jc1E0d6SZsRZrVhudMU8pEmTGBDzpYiI0kCwxNrCKzUGDQnDlwtvX3qD7UCsuaEQU"
    "1bWX5VaaC4HWjMAk4I8vBtbDcqP4rEFFMAvetVzhJtrkZUusquc90KIVV0Evc4MiBgzamDpp"
    "2Fnn8AQ6gxPoDD9XJ5yZPQrtOd6xDfqaaYqInCGzzmzqjILC8nr1q2nPHiiyO9nVB9B9iasv"
    "l3hX/dsb/Musz2rvTxhCzuvhg4Qp+KBFvJPt/y7911bTF0cnFpIYb2tn6oX7GZTYhkB4GuOj"
    "/oWTecAdR6BcsM9V6MJTD17408f+r8OnHx/718Mnz5lg4BaeIPxKCukXSjK4jk+TzaFTrIan"
    "Me67VZL/JoUUFIUUHFNIA7dCGnw1HUmfqCH5bnk8UQmP3Wp4/BZVFBZVFB5RRW9D7/9TRa/f"
    "wR2hLH8oq8Y6DdN2xohh1PYg7Bb/+4933z/2b4aT/k349IN7CsaBUwrGwZEpaK/UHtjQ0/YT"
    "ovULzNazBW89bHDHP/Cu/gU77w9S"
)

LO_TYPES = (
    "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"
    "LMcd//Ph+j+beOuv"
)

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    write_ods("%s/RICH.ODS" % out, rich())
    write_ods("%s/ODD.ODS" % out, odd())
    write_ods("%s/LOSHEET.ODS" % out,
              zlib.decompress(base64.b64decode(LO_SHEET)).decode("utf-8"))
    write_ods("%s/LOTYPES.ODS" % out,
              zlib.decompress(base64.b64decode(LO_TYPES)).decode("utf-8"))
    write_ods("%s/BORDER.ODS" % out, bordered())
    write_ods("%s/LOBOX.ODS" % out,
              zlib.decompress(base64.b64decode(LO_BOX)).decode("utf-8"))
    print("wrote RICH, ODD, BORDER, LOSHEET, LOTYPES and LOBOX .ODS to %s"
          % out)


if __name__ == "__main__":
    main()
