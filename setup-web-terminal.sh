#!/bin/bash
# ============================================================
# setup-web-terminal.sh
#   给这台 CentOS 7 装一个"浏览器里的 VS Code"（code-server），
#   让只有网页可用的人也能拿到终端，自己 git push / 改 tex。
#
# 用法：
#   bash setup-web-terminal.sh              # 默认端口 8080，密码 rachel
#   PORT=9000 PASS=mypw bash setup-web-terminal.sh
#
# 注意：本机是 CentOS 7（glibc 2.17），而官方 code-server 要求 glibc>=2.28，
#       所以优先走 docker（容器自带新 glibc，不动宿主机）。没有 docker 时
#       才退回本地二进制，并明确提示大概率起不来。
# ============================================================

set -u

PORT="${PORT:-8080}"
PASS="${PASS:-rachel}"
WORKDIR="/home/intern2/rachel"
NAME="dsh-code-server"

say()  { printf '\033[36m[%s]\033[0m %s\n' "$(date +%H:%M:%S)" "$*"; }
warn() { printf '\033[33m[%s] WARN:\033[0m %s\n' "$(date +%H:%M:%S)" "$*"; }
die()  { printf '\033[31m[%s] ERROR:\033[0m %s\n' "$(date +%H:%M:%S)" "$*"; exit 1; }

echo "============================================================"
say "环境检查"
echo "  glibc : $(ldd --version 2>/dev/null | head -1)"
echo "  工作区: ${WORKDIR}"
echo "  端口  : ${PORT}"
echo

[ -d "$WORKDIR" ] || die "工作区不存在：${WORKDIR}"

# ---------- 路线 A：docker（推荐） ----------
if command -v docker >/dev/null 2>&1; then
  say "检测到 docker，走容器路线（不碰宿主机 glibc）"

  if ! docker info >/dev/null 2>&1; then
    warn "docker 已安装但当前用户无权限（需要 sudo 或加入 docker 组）"
    warn "改用：sudo bash $0"
    die "docker 不可用"
  fi

  if docker ps -a --format '{{.Names}}' | grep -qx "$NAME"; then
    say "已存在容器 ${NAME}，先删除旧的"
    docker rm -f "$NAME" >/dev/null 2>&1 || true
  fi

  say "启动容器（镜像较大，首次要拉一会儿）"
  docker run -d --name "$NAME" \
    --restart unless-stopped \
    -p "${PORT}:8080" \
    -e "PASSWORD=${PASS}" \
    -v "${WORKDIR}:/home/coder/project" \
    -u "$(id -u):$(id -g)" \
    codercom/code-server:latest || die "容器启动失败，把上面的报错发我"

  sleep 3
  docker ps --filter "name=${NAME}" --format '  容器状态: {{.Status}}'
  IP=$(hostname -I 2>/dev/null | awk '{print $1}')
  echo
  say "完成。浏览器打开："
  echo "    http://${IP:-<本机IP>}:${PORT}"
  echo "    密码：${PASS}"
  echo
  echo "  打开后：菜单 → Terminal → New Terminal，就有 shell 了。"
  echo "  常用：cd ${WORKDIR}/overleaf_project && git add -A && git commit -m '...' && git push origin main"
  echo
  warn "如果打不开，检查防火墙："
  echo "    sudo firewall-cmd --add-port=${PORT}/tcp --permanent && sudo firewall-cmd --reload"
  warn "若这台机器可被公网访问，务必把 PASS 改成强口令，不要用 --auth none"
  exit 0
fi

# ---------- 路线 B：本地二进制（大概率失败，仅作说明） ----------
warn "没有 docker，尝试本地二进制路线"
warn "官方 code-server 需要 glibc>=2.28，本机 $(ldd --version 2>/dev/null | head -1) 很可能起不来"
echo
echo "  CentOS 7 的可行替代（任选其一，需要联网手动操作）："
echo "  1) 装 docker 再跑本脚本："
echo "       sudo yum install -y docker-ce && sudo systemctl enable --now docker"
echo "  2) 用社区维护的 CentOS 7 版本："
echo "       https://github.com/MikeWang000000/vscode-server-centos7"
echo "  3) 用老版 code-server（Node 14 时代，兼容 glibc 2.17）："
echo "       curl -fsSL https://github.com/coder/code-server/releases/download/v3.12.0/code-server-3.12.0-linux-amd64.tar.gz | tar -xz"
echo "       ./code-server-3.12.0-linux-amd64/bin/code-server --bind-addr 0.0.0.0:${PORT} --auth password"
echo
echo "  最省事的仍然是：找有权限的人直接跑 push-now.sh 把活干完。"
echo "      bash /home/intern2/rachel/push-now.sh"
exit 1
