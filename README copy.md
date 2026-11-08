# SAFTI Thesis Proposal LaTeX Template

## 中文说明

### 只需要修改这些文件

1. `config.tex`：填写论文题目、姓名、学号、导师姓名和日期。
2. `chapter/00-guideline.tex`：英文 Guideline，通常不需要修改。
3. `chapter/01-research-background.tex`：撰写研究目的、意义、价值、创新点、
   研究空白和国内外研究现状。
4. `chapter/02-research-design.tex`：撰写研究内容、步骤、方法、已有工作、困难与
   解决方案、可行性、工作量、论文进度和预期成果。
5. `chapter/03-writing-plan.tex`：填写十行论文写作时间表。
6. `references.bib`：删除或保留示例文献，并添加实际使用的 BibTeX 文献记录。

`main.tex` 只负责装配文件，`safti-proposal.cls` 负责版式和固定的参考文献格式。
正常写作时不要修改这两个文件，也不需要自己设置 reference style。

### 目录结构

```text
safti_thesis_proposal_template/
├── main.tex
├── config.tex
├── safti-proposal.cls
├── references.bib
├── README.md
├── chapter/
│   ├── 00-guideline.tex
│   ├── 01-research-background.tex
│   ├── 02-research-design.tex
│   └── 03-writing-plan.tex
└── image/
    ├── logo-audencia.png
    └── logo-safti.png
```

打开需要修改的 `.tex` 文件后，查找以下两类注释：

```tex
% [EN] Instructions shown only in the source file.
% [中文] 只在源文件中显示的修改说明。
```

注释下面是学生填写正文的位置。直接写英文正文即可；不要删除命令的反斜杠、
花括号或环境起止标记。

### 引用与参考文献

参考文献格式已经固定为作者—年份制，并按作者姓名排序。学生只需要在
`references.bib` 中添加条目，然后在章节正文中引用。模板预置三篇示例文献，
分别验证一位作者、两位作者以及三位及以上作者：

```tex
\textcite{fama1970}       % Fama (1970)，一位作者
\parencite{lee2018}       % (Lee and Shin, 2018)，两位作者
\textcite{gomber2017}     % Gomber et al. (2017)，三位及以上作者
```

`\textcite` 适合把作者作为句子的一部分；`\parencite` 适合句末括号引用。三位及
以上作者会自动使用 “et al.”，不要手工输入引用格式。

### 添加图片

把自己的图片放入 `image/`，在章节文件中使用：

```tex
\begin{figure}[htbp]
  \centering
  \includegraphics[width=0.75\textwidth]{image/your-figure.png}
  \caption{Descriptive caption in English.}
  \label{fig:example}
\end{figure}
```

请保留 `image/logo-audencia.png` 和 `image/logo-safti.png` 的文件名，否则封面和
页眉的学校标识将无法显示。

### 编译

在 Overleaf 中，将 Main document 设为 `main.tex`，Compiler 设为 **XeLaTeX**。
模板使用 Biber 自动处理参考文献。模板优先使用 Arial；系统没有 Arial 时会回退到
TeX Gyre Heros。

### 提交前检查

- `config.tex` 中的五项信息正确，封面与 Writing Plan 信息一致。
- Topic Selection Report 和 Writing Plan 已替换为学生自己的英文内容。
- `chapter/00-guideline.tex` 中附件三的英文说明保持完整。
- 每个正文引用在 `references.bib` 中都有对应条目。
- PDF 中没有中文修改说明或灰色填写提示。
- 逐页检查 Logo、页码、标题、表格、图片和参考文献是否完整。

## English Quick Start

Edit `config.tex`, `chapter/01-research-background.tex`,
`chapter/02-research-design.tex`, `chapter/03-writing-plan.tex`, and
`references.bib`. Normally leave `chapter/00-guideline.tex` unchanged because
it reproduces the official English guideline. Follow the bilingual `% [EN]` /
`% [中文]` source comments; they are invisible in the PDF. Add personal figures
to `image/` without renaming the two logo files.

Use `\textcite{key}` for narrative citations and `\parencite{key}` for
parenthetical citations. The class fixes the author–year reference style
automatically. The three sample entries demonstrate one author, two authors,
and three or more authors; replace them with the sources used in your thesis.

On Overleaf, choose `main.tex` as the main document and **XeLaTeX** as the
compiler. Do not edit `main.tex` or `safti-proposal.cls` during normal writing.
