#!/bin/bash
# 一键重建中文版 docx：LaTeX 源 -> 扁平 tex -> pandoc -> 版式收尾
set -e
cd "$(dirname "$0")"
P=/opt/miniconda3/envs/common/lib/python3.10/site-packages/pypandoc/files/pandoc
CSL=/opt/miniconda3/envs/common/lib/python3.10/site-packages/citeproc_styles/styles/china-national-standard-gb-t-7714-2015-author-date.csl
ROOT=/home/intern2/rachel/overleaf_project

python3 build_docx.py
"$P" src_cn_flat.tex -f latex -t docx \
  --reference-doc=reference_zh.docx --citeproc \
  --bibliography="$ROOT/references.bib" --csl="$CSL" \
  --resource-path="$ROOT/image" -o out_raw.docx
python3 postprocess.py
cp out_zh.docx "/home/intern2/rachel/莫卓儒_Thesis Proposal.docx"
echo "done -> /home/intern2/rachel/莫卓儒_Thesis Proposal.docx"
