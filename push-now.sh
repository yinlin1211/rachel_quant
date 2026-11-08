#!/bin/bash
# push-now.sh —— 重启后一次性完成：验证 → 编译校验 → 提交 → 推送 Overleaf
# 用法：bash /home/intern2/rachel/push-now.sh
# 说明：输出里的 remote URL 会被遮蔽，避免 token 泄漏到日志。

set -u
cd /home/intern2/rachel/overleaf_project || { echo "FATAL: 目录不存在"; exit 1; }

mask() { sed -E 's#https://[^@/]*@#https://***@#g'; }

echo "===== 0. 环境检查 ====="
echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-<unset>}"
echo "bash=$BASH_VERSION  git=$(git --version 2>&1)"
command -v xelatex >/dev/null 2>&1 && echo "xelatex=ok" || echo "xelatex=MISSING"
command -v biber   >/dev/null 2>&1 && echo "biber=ok"   || echo "biber=MISSING"

echo
echo "===== 1. 待提交改动 ====="
git status --short
echo "--- 当前 HEAD ---"
git log --oneline -1 2>&1 | mask

echo
echo "===== 2. 编译校验（不阻断推送）====="
if command -v xelatex >/dev/null 2>&1 && command -v biber >/dev/null 2>&1; then
  for doc in main main_en; do
    [ -f "$doc.tex" ] || { echo "$doc.tex 不存在，跳过"; continue; }
    echo "--- $doc ---"
    xelatex -interaction=nonstopmode "$doc.tex" >/dev/null 2>&1
    biber "$doc" >/dev/null 2>&1 || echo "  biber 退出码非 0"
    xelatex -interaction=nonstopmode "$doc.tex" >/dev/null 2>&1
    xelatex -interaction=nonstopmode "$doc.tex" >/dev/null 2>&1
    if [ -f "$doc.pdf" ]; then
      echo "  pdf=$(stat -c%s "$doc.pdf") bytes"
    else
      echo "  PDF 未生成"
    fi
    if [ -f "$doc.log" ]; then
      undef=$(grep -c "Citation.*undefined" "$doc.log" 2>/dev/null || echo 0)
      empty=$(grep -c "Empty bibliography" "$doc.log" 2>/dev/null || echo 0)
      echo "  undefined-citation=${undef}  empty-bibliography=${empty}"
      grep -m3 "LaTeX Error\|Fatal error" "$doc.log" 2>/dev/null | head -3
    fi
  done
else
  echo "宿主机没有 xelatex/biber，跳过编译（PDF 请在 Overleaf 上编译）。"
  echo "提交的只是 .tex 与 .bib 源码，Overleaf 会在下次打开时自动重编译。"
fi

echo
echo "===== 3. 提交 ====="
git add -A
if git diff --cached --quiet; then
  echo "没有需要提交的改动"
else
  git commit -m "fix: 修正 LaTeX 语法与数值不一致；补 2025-2026 文献并中英同步" 2>&1 | tail -3
fi
echo "--- 提交后 HEAD ---"
git log --oneline -1

echo
echo "===== 4. 推送 Overleaf ====="
git push origin main 2>&1 | mask | tail -8
echo "push_exit=${PIPESTATUS[0]}"

echo
echo "===== 5. 结果 ====="
git status -sb | head -2 | mask
echo "local_head=$(git rev-parse --short HEAD)"
echo "remote_head=$(git rev-parse --short origin/main 2>/dev/null || echo unknown)"
