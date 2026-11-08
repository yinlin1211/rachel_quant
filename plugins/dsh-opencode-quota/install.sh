#!/usr/bin/env bash
# 把 opencode-quota 插件装进 DSH 的 web profile，并（可选）重启 DSH 让它生效。
#
#   bash install.sh                  # 只装文件、打好补丁，不重启
#   bash install.sh --restart        # 装完顺手调用 restart-dsh.sh 重启
#   DSH_PROFILE=/path/to/profile bash install.sh
#
# 卸载用 uninstall.sh。
set -euo pipefail

PKG_NAME="dsh-plugin-opencode-quota"
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE="${DSH_PROFILE:-$HOME/.dsh/profiles/web}"
PATCH_FILE="$PROFILE/cordis.patch.yml"
TARGET="$PROFILE/node_modules/$PKG_NAME"
RESTART=0
[[ "${1:-}" == "--restart" ]] && RESTART=1

say() { printf '\033[36m[quota-plugin]\033[0m %s\n' "$*"; }
die() { printf '\033[31m[quota-plugin] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

[[ -f "$PROFILE/package.json" ]] || die "找不到 profile：$PROFILE（用 DSH_PROFILE 指定）"

# ── 1. 拷包 ────────────────────────────────────────────────────────────────
mkdir -p "$PROFILE/node_modules"
rm -rf "$TARGET"
mkdir -p "$TARGET/lib" "$TARGET/bin"
cp "$SRC/package.json" "$TARGET/package.json"
cp "$SRC/lib/index.js" "$SRC/lib/client.js" "$SRC/lib/quota.mjs" "$TARGET/lib/"
say "已复制到 $TARGET"

# ── 2. 打 profile 补丁（幂等）──────────────────────────────────────────────
if grep -q "id: opencode-quota" "$PATCH_FILE" 2>/dev/null; then
  say "cordis.patch.yml 里已有 opencode-quota 行，跳过"
else
  [[ -f "$PATCH_FILE" ]] || : > "$PATCH_FILE"
  cat >> "$PATCH_FILE" <<EOF

# $PKG_NAME（install.sh 追加；uninstall.sh 会删掉下面 3 行）
- insert:
    - id: opencode-quota
      name: $PKG_NAME
EOF
  say "已向 $PATCH_FILE 追加插件行"
fi

# ── 3. 语法自检（不启动服务，只确认模块能 import）──────────────────────────
NODE_BIN="${NODE_BIN:-node}"
if command -v "$NODE_BIN" >/dev/null 2>&1; then
  if "$NODE_BIN" --input-type=module -e "import('$TARGET/lib/index.js').then(()=>console.log('node 半边 OK'))" 2>/tmp/quota-import.err; then
    :
  else
    say "警告：node 半边 import 失败，先别重启。错误："
    sed 's/^/    /' /tmp/quota-import.err | head -12
  fi
else
  say "警告：PATH 里没有 node，跳过 import 自检"
fi

say "安装完成。"
if [[ "$RESTART" == "1" ]]; then
  say "重启 DSH（会中断当前会话，历史保留）…"
  exec bash "$HOME/rachel/restart-dsh.sh"
fi
cat <<'EOS'

下一步（二选一）：

  1) 先验证数字，不动界面：
       node <插件目录>/bin/opencode-quota.mjs
       node <插件目录>/bin/opencode-quota.mjs --verbose

  2) 生效到界面（会重启 DSH，当前会话中断、历史可恢复）：
       bash install.sh --restart
     重启后刷新 http://127.0.0.1:24202 ，会话头部应出现 "Go $x.xx/$100" 徽标。
     接口自检： curl -s http://127.0.0.1:24202/plugins/opencode-quota/data | head -c 400
EOS
