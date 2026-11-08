#!/bin/bash
# ============================================================
# intern11 的 dsh 启动脚本
#
# 用法：
#   ./start-dsh.sh              # 默认 3081 端口
#   ./start-dsh.sh 3085         # 指定端口
#   ./start-dsh.sh --check      # 只做环境自检，不启动
#
# 环境说明：
#   - Node 24 装在共享工具链 /opt/shared-toolchain/node-v24.22.2
#     （wrapper 内部用 ld.so 显式加载 glibc 2.28，不设环境变量）
#   - PATH 已由 ~/.bashrc 配好；本脚本也会再设一次，防止非登录 shell 丢失
#   - 绝对不要 export LD_LIBRARY_PATH，否则 bash/git 会在动态加载器阶段崩
# ============================================================
set -u

TC=/opt/shared-toolchain
case ":$PATH:" in
  *":$TC/node-v24.22.2/bin:"*) ;;
  *) export PATH="$TC/node-v24.22.2/bin:$PATH" ;;
esac
case ":$PATH:" in
  *":$HOME/.npm-global/bin:"*) ;;
  *) export PATH="$HOME/.npm-global/bin:$PATH" ;;
esac

if [ "${1:-}" = "--check" ]; then
  exec "$TC/check-env.sh" --deep
fi

PORT="${1:-3081}"
shift 2>/dev/null || true
HOST=183.36.43.39

# 启动前自检：环境不干净就不启动，避免重演 2026-11-07 的事故
if [ -n "${LD_LIBRARY_PATH:-}" ]; then
  echo "❌ 检测到 LD_LIBRARY_PATH=$LD_LIBRARY_PATH" >&2
  echo "   这会污染所有子进程，dsh 的 bash 工具会崩。" >&2
  echo "   请从 ~/.bashrc / 启动脚本里删掉 export，然后重开终端。" >&2
  exit 1
fi

if ! command -v dsh >/dev/null 2>&1; then
  echo "❌ 找不到 dsh，请检查 PATH（应为 $HOME/.npm-global/bin）" >&2
  exit 1
fi

echo "启动 dsh：host=$HOST port=$PORT  node=$(node -v)  dsh=$(dsh --version 2>/dev/null)"
exec dsh web --no-open --host "$HOST" --port "$PORT" "$@"
