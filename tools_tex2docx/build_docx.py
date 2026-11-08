#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把中文开题报告 LaTeX 源转换成 pandoc 可读的扁平 tex（去掉自定义类命令）。"""
import re, os

ROOT = "/home/intern2/rachel/overleaf_project"
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "src_cn_flat.tex")
PAGEBREAK = "PGBREAKMARKER"

DROP_LINE = re.compile(
    r"^\s*\\(begingroup|endgroup|setstretch\{[^}]*\}|setlength\{[^}]*\}\{[^}]*\}|vspace\*?\{[^}]*\}|"
    r"centering|small|label\{[^}]*\}|itemsep[^\n]*)\s*$"
)

CJK = re.compile(r"[\u3000-\u303f\u3400-\u4dbf\u4e00-\u9fff\uff00-\uffef]")


def fix_tabular_spec(m):
    spec = re.sub(r"@\{[^}]*\}", "", m.group(1))
    spec = re.sub(r"[pmb]\{[^}]*\}", "p", spec)
    n = len(re.findall(r"[lcrp]", spec))
    return "\\begin{tabular}{%s}" % ("l" * max(n, 1))


def join_lines(text):
    """LaTeX 源码里换行即空格；中文段落里这样会插入多余空格，这里合并。"""
    out = []
    for ln in text.split("\n"):
        cur = ln.strip()
        prev = out[-1].rstrip() if out else ""
        joined_cmd = cur.startswith(("\\textbf{", "\\emph{", "\\citep{", "\\citet{", "\\textit{"))
        joinable = (
            prev and cur
            and not prev.lstrip().startswith("%")
            and not prev.endswith("\\\\")
            and (joined_cmd or not cur.startswith("\\"))
            and not prev.startswith(("\\begin{table}", "\\end{table}"))
            and not prev.lstrip().startswith(("\\toprule", "\\midrule", "\\bottomrule", "\\hline"))
        )
        if joinable:
            if CJK.search(prev[-1]) or CJK.search(cur[0]):
                out[-1] = prev + cur
            else:
                out[-1] = prev + " " + cur
        else:
            out.append(ln)
    return "\n".join(out)


def clean(text):
    lines = [ln for ln in text.splitlines()
             if not DROP_LINE.match(ln) and not ln.strip().startswith("%")]
    t = "\n".join(lines)
    t = re.sub(r"\\proposalchapter\{([^}]*)\}", r"\\section*{\1}", t)
    t = re.sub(r"\\parencite(?:\[[^\]]*\])?\{([^}]*)\}", r"\\citep{\1}", t)
    t = re.sub(r"\\textcite(?:\[[^\]]*\])?\{([^}]*)\}", r"\\citet{\1}", t)
    t = re.sub(r"\\begin\{tabular\}\{([^\n{}]*(?:\{[^}]*\}[^\n{}]*)*)\}", fix_tabular_spec, t)
    t = re.sub(r"\\begin\{(table|enumerate|itemize)\}\[[^\]]*\]", r"\\begin{\1}", t)
    t = t.replace("\\newpage", PAGEBREAK)
    return join_lines(t)


def tabular(labels, content, header=True):
    if not header:
        return ("\\begin{table}\n\\begin{tabular}{%s}\n%s\n\\end{tabular}\n\\end{table}\n") % (
            "l" * len(labels), content)
    return ("\\begin{table}\n\\begin{tabular}{%s}\n\\toprule\n%s \\\\\n\\midrule\n%s\n"
            "\\bottomrule\n\\end{tabular}\n\\end{table}\n") % (
        "l" * len(labels), " & ".join(labels), content)


def writing_plan_section():
    raw = open(os.path.join(ROOT, "chapter/03-writing-plan.tex"), encoding="utf-8").read()
    rows = []
    for ln in raw.splitlines():
        m = re.match(r"^(\d+)\s*&\s*(.*?)\s*&\s*(.*?)\s*\\\\.*$", ln.strip())
        if m:
            rows.append("%s & %s & %s \\\\" % m.groups())
    assert len(rows) == 10, len(rows)
    return """\n\n%s\n\n
\\section*{Thesis Writing Plan}

%s

\\vspace{1em}

%s

\\noindent\\textbf{备注：}第 11 周为 L0 冻结节点；未达验收则按降级方案收缩实验范围。
""" % (
        PAGEBREAK,
        tabular(["Student name", "Date"],
                "Student name & 莫卓儒 \\\\\nDate & November 2026 \\\\\n"
                "Thesis title & A股高频量化策略的大规模分层测试研究——面向日内 T+0 的"
                "因子、模型与执行评估 \\\\", header=False),
        tabular(["Order", "Research Content and Expected Goal", "Start and End Time"],
                "\n".join(rows)),
    )


COVER = r"""
\begin{center}
\includegraphics[width=5.2cm]{logo-audencia.png}\hspace{1.5cm}%
\includegraphics[width=4.2cm]{logo-safti.png}
\end{center}

\vspace{1.5cm}

\begin{flushleft}
MSC FINANCIAL TECHNOLOGY \& RISK CONTROL
\end{flushleft}

\vspace{0.4cm}

{\LARGE\bfseries Thesis Proposal}

\vspace{0.8cm}

{\large\bfseries A股高频量化策略的大规模分层测试研究——面向日内T+0的因子、模型与执行评估}

\vspace{2cm}

@@TABLE@@

@@PAGEBREAK@@
"""


def main():
    ch = {n: clean(open(os.path.join(ROOT, "chapter/%s.tex" % n), encoding="utf-8").read())
          for n in ("01-research-background", "02-research-design")}

    ch00 = clean(open(os.path.join(ROOT, "chapter/00-guideline.tex"), encoding="utf-8").read())
    ch00 = "\\section*{MSC Financial Technology \\& Risk Control Research Proposal Guideline}\n" + ch00

    cover = (COVER.replace("@@TABLE@@", tabular(
        ["Student name", "内容"],
        "Student name & 莫卓儒 \\\\\nStudent ID & 2568511008 \\\\\n"
        "Supervisor & 朱晓天\\ 高级讲师、研究生导师 \\\\\nDate & November 2026 \\\\",
        header=False))
        .replace("@@PAGEBREAK@@", PAGEBREAK))

    body = ("\\documentclass[11pt,a4paper]{article}\n\\usepackage{graphicx}\n\\begin{document}\n"
            + cover
            + ch00
            + "\n\n%s\n\n" % PAGEBREAK
            + ch["01-research-background"]
            + "\n\n%s\n\n" % PAGEBREAK
            + ch["02-research-design"]
            + "\n\n%s\n\n" % PAGEBREAK
            + "\\section*{参考文献}\n"
            + writing_plan_section()
            + "\n\\end{document}\n")
    open(OUT, "w", encoding="utf-8").write(body)
    print("wrote", OUT, len(body), "chars")


if __name__ == "__main__":
    main()
