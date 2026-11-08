#!/bin/bash
# ============================================================
# check-env.sh —— 环境自检：确认没有再踩「LD_LIBRARY_PATH 污染」的坑
#
# 背景：这台机器是 CentOS 7（glibc 2.17），而 Node 24 需要 glibc >= 2.28。
#       错误做法是 export LD_LIBRARY_PATH 指向自定义 glibc —— 这个变量会被
#       所有子进程继承，导致 bash / git / make 在动态加载器阶段就崩：
#         relocation error ... _dl_audit_symbind_alt ... GLIBC_PRIVATE
#       正确做法是用 ld.so 显式加载（见 node-v24.22.2/bin/node 这个 wrapper）。
#
# 用法：
#   /opt/shared-toolchain/check-env.sh          # 只检查当前 shell 的环境
#   /opt/shared-toolchain/check-env.sh --deep   # 额外检查 node/koffi/dsh 能否跑
#
# 退出码：0 = 全部通过；1 = 有问题
# ============================================================

set -u
DEEP=0
[ "${1:-}" = "--deep" ] && DEEP=1

PASS=0; FAIL=0
ok()   { printf '  \033[32m[PASS]\033[0m %s\n' "$*"; PASS=$((PASS+1)); }
bad()  { printf '  \033[31m[FAIL]\033[0m %s\n' "$*"; FAIL=$((FAIL+1)); }
info() { printf '  \033[36m[INFO]\033[0m %s\n' "$*"; }

TC=/opt/shared-toolchain
NODE=$TC/node-v24.22.2/bin/node
GLIBC=$TC/glibc228

echo "============================================================"
echo " 环境自检  $(date '+%F %T')  用户=$(whoami)"
echo "============================================================"
echo

echo "【1】污染源检查（最关键）"
if [ -z "${LD_LIBRARY_PATH:-}" ]; then
  ok "LD_LIBRARY_PATH 未设置"
else
  bad "LD_LIBRARY_PATH 被设置 = $LD_LIBRARY_PATH"
  echo "        → 这会污染所有子进程。请从 .bashrc / .bash_profile / 启动脚本里删掉 export。"
fi

for v in LD_PRELOAD LD_AUDIT; do
  if [ -z "${!v:-}" ]; then ok "$v 未设置"; else bad "$v 被设置 = ${!v}"; fi
done
echo

echo "【2】启动文件检查"
POLLUTED=0
for f in "$HOME/.bashrc" "$HOME/.bash_profile" "$HOME/.profile" "$HOME/.zshrc"; do
  [ -f "$f" ] || continue
  if grep -qE '^\s*(export\s+)?LD_LIBRARY_PATH=' "$f" 2>/dev/null; then
    bad "$f 里有 export LD_LIBRARY_PATH"; POLLUTED=1
  fi
done
for f in /etc/profile /etc/bashrc /etc/profile.d/*.sh; do
  [ -f "$f" ] || continue
  if grep -qE '^\s*(export\s+)?LD_LIBRARY_PATH=' "$f" 2>/dev/null; then
    bad "$f 里有 export LD_LIBRARY_PATH（系统级，影响所有人）"; POLLUTED=1
  fi
done
[ "$POLLUTED" = "0" ] && ok "启动文件里没有 LD_LIBRARY_PATH 注入"
echo

echo "【3】基础命令能否启动（被污染时这里会崩）"
if bash -c 'true' 2>/dev/null; then ok "bash 可启动"; else bad "bash 启动失败（relocation error？）"; fi
if git --version >/dev/null 2>&1; then ok "git $(git --version 2>&1 | awk '{print $3}')"; else bad "git 无法运行"; fi
if make --version >/dev/null 2>&1; then ok "make 可用"; else bad "make 无法运行"; fi
echo

echo "【4】共享工具链"
if [ -x "$NODE" ]; then
  ok "node wrapper 存在且可执行: $NODE"
  V=$("$NODE" -v 2>/dev/null)
  if [ -n "$V" ]; then ok "node 版本 = $V"; else bad "node 无法执行"; fi
else
  bad "缺少 $NODE"
fi
if [ -f "$GLIBC/usr/lib64/ld-2.28.so" ]; then ok "glibc 2.28 loader 存在"; else bad "缺少 $GLIBC/usr/lib64/ld-2.28.so"; fi
echo

if [ "$DEEP" = "1" ]; then
  echo "【5】深度检查"
  # node 的子进程是否继承干净环境
  OUT=$("$NODE" -e 'console.log(process.env.LD_LIBRARY_PATH ?? "<unset>")' 2>/dev/null)
  if [ "$OUT" = "<unset>" ]; then ok "node 进程内 LD_LIBRARY_PATH = <unset>"; else bad "node 进程内 LD_LIBRARY_PATH = $OUT"; fi

  # node 能否派生子进程（dsh 的 bash 工具依赖这个）
  if "$NODE" -e 'require("child_process").execSync("bash -c \"echo ok\"",{stdio:"pipe"})' 2>/dev/null; then
    ok "node 能正常派生子进程"
  else
    bad "node 派生子进程失败"
  fi

  # koffi（dsh 的 FFI 依赖，曾因源码编译失败导致整个安装回滚）
  KDIR=$(ls -d "$HOME"/.npm-global/lib/node_modules/@deepseek-ai/dsh/node_modules/koffi 2>/dev/null | head -1)
  if [ -n "$KDIR" ]; then
    if "$NODE" -e "require('$KDIR')" 2>/dev/null; then ok "koffi 预编译二进制可加载"; else bad "koffi 加载失败"; fi
  else
    info "尚未安装 dsh，跳过 koffi 检查"
  fi

  # dsh 本体
  if [ -x "$HOME/.npm-global/bin/dsh" ]; then
    if "$HOME/.npm-global/bin/dsh" --version >/dev/null 2>&1; then ok "dsh 可执行"; else bad "dsh 存在但无法运行"; fi
  else
    info "尚未安装 dsh"
  fi
  echo
fi

echo "============================================================"
printf " 结果：\033[32m%d 通过\033[0m，\033[31m%d 失败\033[0m\n" "$PASS" "$FAIL"
echo "============================================================"
[ "$FAIL" -eq 0 ] || exit 1
