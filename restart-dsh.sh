#!/bin/bash
# ============================================================
# restart-dsh.sh —— 重启 DSH Web 服务，并去掉 LD_LIBRARY_PATH 污染
#
# 背景：这台机器是 CentOS 7（glibc 2.17），而 dsh 要 node >= 24.9，
#       node 24 需要 glibc >= 2.28，所以用 /home/intern2/glibc228 这套
#       自定义 glibc 让 node 能跑。但 LD_LIBRARY_PATH 一旦进入环境，
#       子进程（bash / git / latexmk）也会加载这套 glibc 的 libc.so.6，
#       与系统 ld-linux 的 GLIBC_PRIVATE 符号不匹配 → 全部启动失败：
#         bash: relocation error: ... _dl_audit_symbind_alt ... GLIBC_PRIVATE
#
# 做法：只让 node 自己用 glibc228（通过 ld-2.28.so --library-path 或
#       node 自带 rpath），不给环境变量，这样子进程回归系统 glibc。
#
# 用法：
#   bash restart-dsh.sh            # 执行重启
#   bash restart-dsh.sh --dry-run  # 只探测、不杀进程
#   bash restart-dsh.sh 24202      # 指定端口（默认 24202）
#
# 注意：重启会中断当前正在运行的 dsh（也就是正在聊天的那个会话），
#       会话记录是持久化的，重连后从历史里恢复即可。
# ============================================================

set -u

PORT=24202
DRY_RUN=0
for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=1 ;;
    *) [[ "$arg" =~ ^[0-9]+$ ]] && PORT="$arg" ;;
  esac
done

say() { printf '\033[36m[%s]\033[0m %s\n' "$(date +%H:%M:%S)" "$*"; }
warn() { printf '\033[33m[%s] WARN:\033[0m %s\n' "$(date +%H:%M:%S)" "$*"; }
die() { printf '\033[31m[%s] ERROR:\033[0m %s\n' "$(date +%H:%M:%S)" "$*"; exit 1; }

# ---------- 0. 先把可能已污染的环境清掉，否则 ss/curl 自己也跑不起来 ----------
ORIG_LD="${LD_LIBRARY_PATH:-}"
unset LD_LIBRARY_PATH

GLIBC_ROOT=/home/intern2/glibc228
DEFAULT_LIB="$GLIBC_ROOT/usr/lib64:$GLIBC_ROOT/lib64"

# 找一个能用的动态加载器（优先常见软链名，其次真实文件名）
find_loader() {
  local c
  for c in \
    "$GLIBC_ROOT/usr/lib64/ld-linux-x86-64.so.2" \
    "$GLIBC_ROOT/usr/lib64/ld-2.28.so" \
    "$GLIBC_ROOT/lib64/ld-linux-x86-64.so.2" \
    "$GLIBC_ROOT/lib64/ld-2.28.so" ; do
    [ -e "$c" ] && { echo "$c"; return 0; }
  done
  return 1
}
LD_SO="$(find_loader || true)"

# ---------- 1. 找到当前监听该端口的进程 ----------
listener_pid() {
  local pid=""
  if command -v ss >/dev/null 2>&1; then
    pid=$(ss -ltnp "( sport = :$PORT )" 2>/dev/null | grep -o 'pid=[0-9]*' | head -1 | cut -d= -f2)
  fi
  if [ -z "$pid" ] && command -v lsof >/dev/null 2>&1; then
    pid=$(lsof -ti "tcp:$PORT" 2>/dev/null | head -1)
  fi
  if [ -z "$pid" ] && command -v fuser >/dev/null 2>&1; then
    pid=$(fuser "$PORT/tcp" 2>/dev/null | tr -s ' ' '\n' | grep -E '^[0-9]+$' | head -1)
  fi
  echo "$pid"
}

PID="$(listener_pid)"
[ -z "${PID:-}" ] && die "找不到监听 :$PORT 的进程。用 'ss -ltnp | grep $PORT' 确认端口，或用参数指定端口。"

NODE="$(tr '\0' '\n' < "/proc/$PID/cmdline" 2>/dev/null | head -1)"
mapfile -t ARGS < <(tr '\0' '\n' < "/proc/$PID/cmdline" 2>/dev/null | tail -n +2)
CWD="$(readlink "/proc/$PID/cwd" 2>/dev/null || echo "$PWD")"
PROC_LD="$(tr '\0' '\n' < "/proc/$PID/environ" 2>/dev/null | sed -n 's/^LD_LIBRARY_PATH=//p' | head -1)"
LIB_PATH="${PROC_LD:-${ORIG_LD:-$DEFAULT_LIB}}"

say "端口      : $PORT"
say "PID       : $PID"
say "node      : ${NODE:-<未知>}"
say "cwd       : $CWD"
say "原本 LD_LIBRARY_PATH : ${PROC_LD:-(空)}"
say "参数      : ${ARGS[*]:-(空)}"

[ -n "${NODE:-}" ] && [ -x "$NODE" ] || die "读不到可执行的 node 路径（/proc/$PID/cmdline），放弃。"
[ ${#ARGS[@]} -gt 0 ] || die "读不到启动参数（/proc/$PID/cmdline），放弃。"

# ---------- 2. 探针：确认哪种启动方式真的能让 node 起来 ----------
probe_node() {
  # $1 = clean | loader
  case "$1" in
    clean)
      env -u LD_LIBRARY_PATH "$NODE" -e 'process.exit(0)' >/dev/null 2>&1 ;;
    loader)
      [ -n "$LD_SO" ] || return 1
      env -u LD_LIBRARY_PATH "$LD_SO" --library-path "$LIB_PATH" "$NODE" -e 'process.exit(0)' >/dev/null 2>&1 ;;
    *) return 1 ;;
  esac
}

METHOD=""
for m in clean loader; do
  if probe_node "$m"; then METHOD="$m"; break; fi
  say "探针 $m 失败"
done
[ -z "$METHOD" ] && die "node 在「纯净环境」和「loader 模式」下都起不来；为了不把你晾在半路，旧进程未动。"

say "探针通过：method=$METHOD"
[ "$METHOD" = "loader" ] && say "将使用加载器：$LD_SO"
[ "$METHOD" = "loader" ] && say "库路径        ：$LIB_PATH"

# 目标检查：新环境下 bash 是否可用
if env -u LD_LIBRARY_PATH bash -c 'true' >/dev/null 2>&1; then
  say "检查通过：纯净环境下 bash 可正常启动"
else
  warn "纯净环境下 bash 仍然不可用，重启可能无意义（请把下面的输出发我）"
  env -u LD_LIBRARY_PATH bash -c 'true' 2>&1 | head -3
fi

if [ "$DRY_RUN" = "1" ]; then
  say "--dry-run：不杀进程、不启动。以上信息确认无误后去掉该参数再跑。"
  exit 0
fi

# ---------- 3. 组装新命令 ----------
if [ "$METHOD" = "clean" ]; then
  LAUNCH=( "$NODE" "${ARGS[@]}" )
else
  LAUNCH=( "$LD_SO" --library-path "$LIB_PATH" "$NODE" "${ARGS[@]}" )
fi

LOG="${HOME}/dsh-web.log"

# ---------- 4. 停旧进程 ----------
say "停止旧进程 $PID …"
kill "$PID" 2>/dev/null
for _ in $(seq 1 10); do
  kill -0 "$PID" 2>/dev/null || break
  sleep 1
done
kill -9 "$PID" 2>/dev/null || true
sleep 1
[ -n "$(listener_pid)" ] && die "端口 $PORT 仍被占用，可能还有残留进程，请手动检查。"

# ---------- 5. 启动新进程（脱离终端，断线也不退出） ----------
say "启动新进程（工作目录 $CWD，日志 $LOG）…"
cd "$CWD" 2>/dev/null || cd "$HOME"
setsid nohup env -u LD_LIBRARY_PATH "${LAUNCH[@]}" >>"$LOG" 2>&1 < /dev/null &
NEWPID=$!
say "新进程 PID=$NEWPID"

# ---------- 6. 健康检查 ----------
health() {
  if command -v curl >/dev/null 2>&1; then
    curl -fsS -o /dev/null --max-time 3 "http://127.0.0.1:$PORT/" >/dev/null 2>&1
  else
    (exec 3<>"/dev/tcp/127.0.0.1/$PORT") >/dev/null 2>&1
  fi
}

for i in $(seq 1 60); do
  if health; then
    say "服务已就绪： http://127.0.0.1:$PORT/"
    say "浏览器重新打开上面的地址即可；本次会话已持久化，可从历史恢复。"
    say "环境检查：新进程的子进程应当没有 LD_LIBRARY_PATH，bash/git 恢复正常。"
    exit 0
  fi
  kill -0 "$NEWPID" 2>/dev/null || { warn "新进程已退出，见 $LOG"; break; }
  sleep 1
done

# ---------- 7. 失败则回滚成原来的启动方式，至少保证服务活着 ----------
warn "新方式启动失败，回滚为原来带 LD_LIBRARY_PATH 的启动方式（shell 仍会不可用）…"
LOG2="${HOME}/dsh-web-fallback.log"
setsid nohup env LD_LIBRARY_PATH="$LIB_PATH" "${LAUNCH[@]}" >>"$LOG2" 2>&1 < /dev/null &
for i in $(seq 1 30); do
  health && { warn "已回滚启动： http://127.0.0.1:$PORT/ （日志 $LOG2）"; exit 1; }
  sleep 1
done
die "回滚也没起来。请看 $LOG 与 $LOG2，或手动前台启动排查。"
