#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""基于 pandoc 默认 reference.docx 生成中文样式模板。"""
import docx
from docx.shared import Pt, RGBColor, Cm
from docx.oxml.ns import qn
from docx.enum.text import WD_ALIGN_PARAGRAPH

SRC = "reference_default.docx"
DST = "reference_zh.docx"
NAVY = RGBColor(0x11, 0x34, 0x5B)
MAGENTA = RGBColor(0xA7, 0x2C, 0x72)

d = docx.Document(SRC)


def set_ea(style, latin, ea, size=None, bold=None, color=None, spacing=None):
    f = style.font
    f.name = latin
    if size:
        f.size = Pt(size)
    if bold is not None:
        f.bold = bold
    if color is not None:
        f.color.rgb = color
    rpr = style.element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = rpr.makeelement(qn("w:rFonts"), {})
        rpr.insert(0, rfonts)
    rfonts.set(qn("w:ascii"), latin)
    rfonts.set(qn("w:hAnsi"), latin)
    rfonts.set(qn("w:eastAsia"), ea)
    rfonts.set(qn("w:cs"), latin)
    if spacing is not None:
        pf = style.paragraph_format
        pf.line_spacing = spacing


# 正文
for name in ("Normal", "Body Text", "First Paragraph", "Compact"):
    st = d.styles[name]
    set_ea(st, "Times New Roman", "宋体", 11, spacing=1.5)
    st.paragraph_format.space_before = Pt(0)
    st.paragraph_format.space_after = Pt(6)
    st.paragraph_format.first_line_indent = Pt(0)

# 标题
h1 = d.styles["Heading 1"]
set_ea(h1, "Times New Roman", "黑体", 17, True, NAVY)
h1.paragraph_format.space_before = Pt(18)
h1.paragraph_format.space_after = Pt(10)
h1.paragraph_format.line_spacing = 1.2

h2 = d.styles["Heading 2"]
set_ea(h2, "Times New Roman", "黑体", 13.5, True, NAVY)
h2.paragraph_format.space_before = Pt(12)
h2.paragraph_format.space_after = Pt(6)
h2.paragraph_format.line_spacing = 1.2

for nm in ("Heading 3", "Heading 4"):
    set_ea(d.styles[nm], "Times New Roman", "黑体", 12, True, NAVY)

t = d.styles["Title"]
set_ea(t, "Times New Roman", "黑体", 26, True, MAGENTA)
t.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER

sub = d.styles["Subtitle"]
set_ea(sub, "Times New Roman", "黑体", 14, True, NAVY)

for nm in ("Caption", "Table Caption", "Image Caption"):
    try:
        st = d.styles[nm]
        set_ea(st, "Times New Roman", "宋体", 9.5, False, RGBColor(0x44, 0x44, 0x44))
        st.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
        st.paragraph_format.space_before = Pt(6)
        st.paragraph_format.space_after = Pt(4)
    except KeyError:
        pass

bib = d.styles["Bibliography"]
set_ea(bib, "Times New Roman", "宋体", 10.5, spacing=1.15)
bib.paragraph_format.space_after = Pt(3)
bib.paragraph_format.left_indent = Pt(18)
bib.paragraph_format.first_line_indent = Pt(-18)

# 页边距 A4 + 2.54cm
for s in d.sections:
    s.page_height = Cm(29.7)
    s.page_width = Cm(21.0)
    s.top_margin = s.bottom_margin = Cm(2.54)
    s.left_margin = s.right_margin = Cm(2.54)

d.save(DST)
print("saved", DST)
