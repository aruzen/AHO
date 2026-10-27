"""外部フォントを再配布せず、metricsとkerningが既知のテスト用TTFを作る。"""
from pathlib import Path
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable
from fontTools.ttLib.tables._k_e_r_n import KernTable_format_0


def outline(*contours):
    pen = TTGlyphPen(None)
    for points in contours:
        pen.moveTo(points[0])
        for point in points[1:]:
            pen.lineTo(point)
        pen.closePath()
    return pen.glyph()


box = [(50, 0), (50, 700), (550, 700), (550, 0)]
glyphs = {
    ".notdef": outline(box),
    "space": outline(),
    "A": outline([(50, 0), (300, 700), (550, 0)], [(220, 200), (380, 200), (300, 450)]),
    "V": outline([(50, 700), (170, 700), (300, 180), (430, 700), (550, 700), (360, 0), (240, 0)]),
    "question": outline(box),
    "replacement": outline(box),
    "uni65E5": outline([(50, -100), (50, 700), (650, 700), (650, -100)],
                       [(150, 0), (550, 0), (550, 250), (150, 250)],
                       [(150, 350), (550, 350), (550, 600), (150, 600)]),
    "uni672C": outline([(50, 0), (350, 700), (650, 0)], [(250, 100), (450, 100), (350, 400)]),
    "uni8A9E": outline([(50, 0), (50, 700), (650, 700), (650, 0)], [(150, 100), (550, 100), (550, 600), (150, 600)]),
}
builder = FontBuilder(1000, isTTF=True)
builder.setupGlyphOrder(list(glyphs))
builder.setupCharacterMap({32: "space", 65: "A", 86: "V", 63: "question", 0xFFFD: "replacement",
                          0x65E5: "uni65E5", 0x672C: "uni672C", 0x8A9E: "uni8A9E"})
builder.setupGlyf(glyphs)
builder.setupHorizontalMetrics({name: (300 if name == "space" else 800 if name.startswith("uni") else 600,
                                        0 if name == "space" else 50) for name in glyphs})
builder.setupHorizontalHeader(ascent=800, descent=-200, lineGap=200)
builder.setupNameTable({"familyName": "AHO MSDF Test", "styleName": "Regular", "uniqueFontIdentifier": "AHO-MSDF-Test-1",
                        "fullName": "AHO MSDF Test Regular", "psName": "AHO-MSDF-Test-Regular"})
builder.setupOS2(sTypoAscender=800, sTypoDescender=-200, sTypoLineGap=200, usWinAscent=800, usWinDescent=200)
builder.setupPost(underlinePosition=-100, underlineThickness=50)
builder.setupMaxp()
kern = newTable("kern")
kern.version = 0
table = KernTable_format_0()
table.version, table.coverage = 0, 1
table.kernTable = {("A", "V"): -80}
kern.kernTables = [table]
builder.font["kern"] = kern
builder.font["head"].created = builder.font["head"].modified = 2082844800
builder.font.recalcTimestamp = False
builder.save(Path(__file__).with_name("TestFont.ttf"))
