#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""对 pandoc 产出的 docx 做版式收尾：分页、封面、表格线、页码、章节顺序。"""
import copy, sys
import docx
from docx.shared import Pt, RGBColor, Cm
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

SRC, DST = "out_raw.docx", "out_zh.docx"
NAVY = RGBColor(0x11, 0x34, 0x5B)
MAGENTA = RGBColor(0xA7, 0x2C, 0x72)

d = docx.Document(SRC)
body = d.element.body
W = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"


def ptext(p):
    el = p._element if hasattr(p, "_element") else p
    return "".join(t.text or "" for t in el.iter(qn("w:t")))


# ---------- 1. 分页标记 -> 真正的分页符 ----------
for p in d.paragraphs:
    if ptext(p).strip() == "PGBREAKMARKER":
        for r in list(p.runs):
            r._element.getparent().remove(r._element)
        run = p.add_run()
        br = OxmlElement("w:br")
        br.set(qn("w:type"), "page")
        run._element.append(br)
        p.paragraph_format.space_before = Pt(0)
        p.paragraph_format.space_after = Pt(0)
        p.paragraph_format.line_spacing = 1

# ---------- 2. 封面排版 ----------
cover_map = {
    "MSC FINANCIAL TECHNOLOGY & RISK CONTROL": dict(size=10.5, bold=True, color=NAVY),
    "Thesis Proposal": dict(size=30, bold=True, color=MAGENTA),
    "A股高频量化策略的大规模分层测试研究——面向日内T+0的因子、模型与执行评估":
        dict(size=16, bold=True, color=NAVY),
}
for p in d.paragraphs:
    cfg = cover_map.get(p.text.strip())
    if cfg:
        for r in p.runs:
            r.font.size = Pt(cfg["size"])
            r.font.bold = cfg["bold"]
            r.font.color.rgb = cfg["color"]
            r.font.name = "Times New Roman"
            r._element.rPr.rFonts.set(qn("w:eastAsia"), "黑体")
        p.paragraph_format.space_before = Pt(0)
        p.paragraph_format.line_spacing = 1.3
        if cfg["size"] == 30:
            p.paragraph_format.space_before = Pt(6)
    if p.text.strip().startswith("The main body of the thesis proposal"):
        p.paragraph_format.space_before = Pt(24)

# ---------- 3. 表格：边框、宽度、表头 ----------
def set_borders(tbl):
    tblPr = tbl._tbl.tblPr
    for old in tblPr.findall(qn("w:tblBorders")):
        tblPr.remove(old)
    borders = OxmlElement("w:tblBorders")
    for edge, sz in (("top", 12), ("bottom", 12), ("insideH", 4),
                     ("left", 4), ("right", 4), ("insideV", 4)):
        el = OxmlElement("w:%s" % edge)
        el.set(qn("w:val"), "single")
        el.set(qn("w:sz"), str(sz))
        el.set(qn("w:color"), "808080" if sz == 4 else "11345B")
        borders.append(el)
    tblPr.append(borders)


def shade(cell, hexcolor):
    tcPr = cell._tc.get_or_add_tcPr()
    for old in tcPr.findall(qn("w:shd")):
        tcPr.remove(old)
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:fill"), hexcolor)
    tcPr.append(shd)


for tbl in d.tables:
    set_borders(tbl)
    tbl.alignment = WD_TABLE_ALIGNMENT.CENTER
    ncol = len(tbl.columns)
    fs = 10 if ncol >= 3 else 10.5
    info = tbl.rows[0].cells[0].text.strip() == "Student name"  # 封面/写作计划信息表
    for i, row in enumerate(tbl.rows):
        if i == 0 and not info:
            trPr = row._tr.get_or_add_trPr()
            trPr.append(OxmlElement("w:tblHeader"))
        for j, cell in enumerate(row.cells):
            if i == 0 and not info:
                shade(cell, "E8EDF3")
            for p in cell.paragraphs:
                p.paragraph_format.space_before = Pt(3)
                p.paragraph_format.space_after = Pt(3)
                p.paragraph_format.line_spacing = 1.15
                p.alignment = WD_ALIGN_PARAGRAPH.CENTER if j == 0 else WD_ALIGN_PARAGRAPH.LEFT
                for r in p.runs:
                    r.font.size = Pt(fs)
                    if i == 0 and not info:
                        r.font.bold = True
                        r.font.color.rgb = NAVY
                    elif info and j == 0:
                        r.font.bold = True
                        r.font.color.rgb = NAVY

# ---------- 4. 把 Thesis Writing Plan 整段移到文末（参考文献之后） ----------
children = list(body)
h_start = h_end = None
for idx, el in enumerate(children):
    if el.tag != W + "p":
        continue
    txt = ptext(el).strip()
    if txt == "Thesis Writing Plan":
        h_start = idx
    if txt.startswith("备注：第 11 周"):
        h_end = idx
if h_start is not None and h_end is not None:
    # 把紧邻的分页段落一起带走
    if h_start > 0 and children[h_start - 1].tag == W + "p" \
            and not ptext(children[h_start - 1]).strip():
        h_start -= 1
    block = children[h_start:h_end + 1]
    sectPr = body.find(qn("w:sectPr"))
    for el in block:
        body.remove(el)
    for el in block:
        if sectPr is not None:
            sectPr.addprevious(el)
        else:
            body.append(el)
    print("moved writing-plan block:", len(block), "elements")

# ---------- 5. 页眉页脚：居中页眉 + 页码 ----------
sec = d.sections[0]
sec.different_first_page_header_footer = True
hp = sec.header.paragraphs[0]
hp.alignment = WD_ALIGN_PARAGRAPH.CENTER
hr = hp.add_run("MSC Financial Technology & Risk Control")
hr.font.size = Pt(8)
hr.font.color.rgb = NAVY
hr.font.name = "Times New Roman"
hp.paragraph_format.space_after = Pt(2)

fp = sec.footer.paragraphs[0]
fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
run = fp.add_run()
begin = OxmlElement("w:fldChar")
begin.set(qn("w:fldCharType"), "begin")
instr = OxmlElement("w:instrText")
instr.set(qn("xml:space"), "preserve")
instr.text = " PAGE "
end = OxmlElement("w:fldChar")
end.set(qn("w:fldCharType"), "end")
for el in (begin, instr, end):
    run._element.append(el)
run.font.size = Pt(9)

cp = d.core_properties
cp.title = "A股高频量化策略的大规模分层测试研究——面向日内T+0的因子、模型与执行评估"
cp.subject = "MSC Financial Technology & Risk Control Thesis Proposal"
cp.author = "莫卓儒"
cp.comments = "由 LaTeX 源 main.tex 转换生成"

d.save(DST)
print("saved", DST)
