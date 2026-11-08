#!/usr/bin/env bash
# 卸载 opencode-quota：删包 + 从 cordis.patch.yml 摘掉插件行。需重启 DSH 生效。
set -euo pipefail

PKG_NAME="dsh-plugin-opencode-quota"
PROFILE="${DSH_PROFILE:-$HOME/.dsh/profiles/web}"
PATCH_FILE="$PROFILE/cordis.patch.yml"
TARGET="$PROFILE/node_modules/$PKG_NAME"

say() { printf '\033[36m[quota-plugin]\033[0m %s\n' "$*"; }

if [[ -d "$TARGET" ]]; then
  rm -rf "$TARGET"
  say "已删除 $TARGET"
else
  say "包目录不存在，跳过"
fi

if [[ -f "$PATCH_FILE" ]] && grep -q "id: opencode-quota" "$PATCH_FILE"; then
  BAK="$PATCH_FILE.bak.$(date +%Y%m%d%H%M%S)"
  cp "$PATCH_FILE" "$BAK"
  awk '
    BEGIN { skip = 0 }
    /^# dsh-plugin-opencode-quota（install.sh 追加/ { skip = 1; next }
    skip == 1 && /^- insert:$/ { skip = 2; next }
    skip == 2 && /^    - id: opencode-quota$/ { skip = 3; next }
    skip == 3 && /^      name: dsh-plugin-opencode-quota$/ { skip = 0; next }
    { skip = 0; print }
  ' "$BAK" > "$PATCH_FILE.tmp"

  if ! grep -q "id: opencode-quota" "$PATCH_FILE.tmp"; then
    mv "$PATCH_FILE.tmp" "$PATCH_FILE"
    say "已从 cordis.patch.yml 摘掉插件行（备份：$BAK）"
  else
    rm -f "$PATCH_FILE.tmp"
    say "警告：自动摘除失败，请手动删掉 cordis.patch.yml 末尾这 3 行（备份：$BAK）："
    printf '    - insert:\n        - id: opencode-quota\n          name: %s\n' "$PKG_NAME"
  fi
else
  say "cordis.patch.yml 里没有插件行，跳过"
fi

say "卸载完成。重启 DSH 后生效：bash ~/rachel/restart-dsh.sh"
