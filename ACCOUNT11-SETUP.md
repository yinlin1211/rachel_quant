# 账号 11（intern11）DSH 配置记录

**日期**：2026-11-08
**执行人**：intern2 会话（通过 SSH 远程配置）
**目标**：让 intern11 能独立跑 dsh，且**不再重演 2026-11-07 的环境事故**

---

## 1. 为什么会有这份文档

2026-11-07 在 intern2 上出过一次严重事故：所有 shell 命令退码 127，`bash`/`git`/`make`
全部无法启动，表现为

```
bash: relocation error: /home/intern2/glibc228/usr/lib64/libc.so.6:
symbol _dl_audit_symbind_alt, version GLIBC_PRIVATE not defined in
file ld-linux-x86-64.so.2 with link time reference
```

**根因**：Node 24 需要 glibc ≥ 2.28，而本机是 CentOS 7（glibc 2.17）。当时的做法是在
`node` 包装脚本里 `export LD_LIBRARY_PATH=<glibc228>` 再 exec 真身。

问题在于 **`LD_LIBRARY_PATH` 是进程级、且完全继承的**：dsh 用它启动后，dsh 派生的每个子进程
（`bash`、`git`、`make`）都拿到了「2.17 的加载器 + 2.28 的 libc」这个不匹配的组合，
在 `main()` 之前就被动态加载器拒绝。

**这次给 intern11 配置时，从设计上避免了这个问题**（见 §3）。

---

## 2. 已完成配置

| 项 | 内容 | 位置 |
|---|---|---|
| 共享工具链 | Node 24.22.2 + glibc 2.28 | `/opt/shared-toolchain/` |
| dsh | `0.2.2-rc.1`（654 包） | `~/.npm-global/` |
| PATH | 自动前置 node24 与 `.npm-global/bin` | `~/.bashrc` 的标记块 |
| dsh 配置 | provider / model / permission 预设 | `~/.dsh/profiles/web/` |
| 启动脚本 | 带启动前自检 | `~/start-dsh.sh` |
| 自检脚本 | 14 项检查 | `/opt/shared-toolchain/check-env.sh` |

### 2.1 共享工具链（关键设计）

```
/opt/shared-toolchain/
├── glibc228/                     9 MB   glibc 2.28（含 ld-2.28.so）
├── node-v24.22.2/
│   └── bin/
│       ├── node                  ← 包装脚本（位置无关）
│       └── node-real             121 MB  真正的 node 二进制
└── check-env.sh                        环境自检
```

**为什么放 `/opt/shared-toolchain` 而不是某个人的家目录**：

- 家目录默认 700，其他人（包括 inter11）进不去；
- 放共享位置后，**任何账号都能用同一份**，不必每人拷 200 MB；
- 权限设为 755，所有人可读可执行，但只有属主可写。

### 2.2 node 包装脚本（防复发的核心）

```sh
#!/bin/sh
SELF_DIR=$(cd "$(dirname "$0")" && pwd)
TC_DIR=$(dirname "$(dirname "$SELF_DIR")")
GLIBC228_DIR=${GLIBC228_DIR:-"$TC_DIR/glibc228"}
exec "$GLIBC228_DIR/usr/lib64/ld-2.28.so" \
     --library-path "$GLIBC228_DIR/usr/lib64:$GLIBC228_DIR/lib64" \
     "$SELF_DIR/node-real" "$@"
```

三个要点：

1. **用 `ld.so --library-path` 显式加载**，而不是 `export LD_LIBRARY_PATH` ——
   glibc 由加载器参数传入，**不进入进程环境**，子进程自然继承不到。
2. **路径用 `$(dirname $0)` 推导**，不写死绝对路径 —— 整棵树可以搬走、可以拷贝给别的账号。
3. `GLIBC228_DIR` 可覆盖 —— 万一 glibc 放在别处也能用。

### 2.3 dsh 安装的一个坑（`koffi`）

首次安装失败并回滚：

```
error path .../node_modules/koffi
error command sh -c node ./cnoke.cjs -P . -D src/koffi --prebuild --release
```

`koffi` 是 dsh 的 FFI 依赖（`dsh-subprocess-local` 用它做 `execve`），它的 install 脚本
要**从源码编译**，而本机 GCC 是 4.8.5，编译失败 → 整个 npm install 回滚，dsh 目录被删。

**解法**：`npm install --ignore-scripts`。因为所有原生依赖都有预编译包：

| 包 | 预编译二进制 |
|---|---|
| `koffi` | `@koromix/koffi-linux-x64/linux_x64/koffi.node` |
| `node-pty` | `prebuilds/<platform>/*.node` |
| `sherpa-onnx` | `sherpa-onnx-linux-x64/sherpa-onnx.node` |

已实测 `require('koffi')` 能正常加载（version 3.1.1），`--ignore-scripts` 无副作用。

> 注意：`--ignore-scripts` 会跳过所有包的 install 脚本。若将来某个依赖**必须**编译，
> 需要单独处理（或升级 GCC 到 devtoolset-8+）。

---

## 3. 防复发的四道防线

### 防线 1：包装脚本不 export（根本措施）

见 §2.2。环境变量从头到尾没有被设置过。

### 防线 2：启动文件里禁止注入

`~/.bashrc` 里只加 PATH，并**明确注释**为什么不能加 `LD_LIBRARY_PATH`：

```bash
# ⚠️ 绝对不要在这里 export LD_LIBRARY_PATH！
#    一旦把 glibc228 写进 LD_LIBRARY_PATH，它会继承给所有子进程，
#    bash / git / make 会在动态加载器阶段直接崩。
case ":$PATH:" in
  *":/opt/shared-toolchain/node-v24.22.2/bin:"*) ;;
  *) export PATH="/opt/shared-toolchain/node-v24.22.2/bin:$PATH" ;;
esac
```

### 防线 3：启动脚本自检（拒绝在脏环境启动）

`~/start-dsh.sh` 在启动前检查，脏了就**直接拒绝启动**并给出修法：

```bash
if [ -n "${LD_LIBRARY_PATH:-}" ]; then
  echo "❌ 检测到 LD_LIBRARY_PATH=$LD_LIBRARY_PATH" >&2
  echo "   这会污染所有子进程，dsh 的 bash 工具会崩。" >&2
  exit 1
fi
```

### 防线 4：随时可查的自检脚本

```sh
/opt/shared-toolchain/check-env.sh --deep
```

14 项检查，覆盖：污染源（`LD_*` 三个变量）、启动文件、基础命令能否启动、工具链完整性、
node 进程内环境、能否派生子进程、koffi 能否加载、dsh 能否执行。

**真出问题时，第一件事就是跑这个。**

---

## 4. 怎么用（intern11）

### 首次使用

```sh
# 1. 确认环境（14 项应全 PASS）
/opt/shared-toolchain/check-env.sh --deep

# 2. 启动（默认 3081 端口）
~/start-dsh.sh

# 3. 浏览器打开日志里打印的地址，形如
#    http://183.36.43.39:3081/?token=<每次启动随机>
```

### 端口约定

| 账号 | 端口 | 说明 |
|---|---|---|
| intern2 | 3080 | 已在跑，勿占用 |
| intern11 | 3081 | 本账号 |

换端口：`~/start-dsh.sh 3085`

### 后台常驻

```sh
nohup ~/start-dsh.sh 3081 > ~/dsh-run.log 2>&1 &
```

访问 token 在 `~/dsh-run.log` 第一行附近。

---

## 5. 实测验证记录（2026-11-08 17:45）

```
【自检】14 通过，0 失败
  ├ LD_LIBRARY_PATH 未设置 / LD_PRELOAD 未设置 / LD_AUDIT 未设置
  ├ 启动文件无注入
  ├ bash 可启动 / git 1.8.3.1 / make 可用
  ├ node wrapper 可执行 / node v24.22.2 / glibc loader 存在
  ├ node 进程内 LD_LIBRARY_PATH = <unset>
  ├ node 能正常派生子进程
  ├ koffi 预编译二进制可加载
  └ dsh 可执行

【启动】dsh web: http://183.36.43.39:3081/?token=bZ_dv7vJ...
  ├ 端口监听：183.36.43.39:3081 ✓
  ├ HTTP 探测：401（正常，需 token）
  └ 运行中进程的 /proc/<pid>/environ 中 LD_LIBRARY_PATH 条数 = 0 ✓✓
```

最后一条是决定性证据：**运行中的 dsh 及其所有子进程，环境里根本没有这个变量。**

---

## 6. 如果又出问题（排错顺序）

| 症状 | 先查什么 | 处理 |
|---|---|---|
| `bash: relocation error ... GLIBC_PRIVATE` | `echo $LD_LIBRARY_PATH` | 非空 → 从 `.bashrc`/启动脚本删掉 export，**重开终端** |
| `dsh: command not found` | `echo $PATH` | 缺 `~/.npm-global/bin` → `source ~/.bashrc` |
| `SyntaxError: ... 'node:fs/promises' does not provide an export named 'constants'` | `node -v` | 是 v16 → PATH 顺序不对，node24 没前置 |
| `koffi` 加载失败 | `check-env.sh --deep` | 重装时漏了 `--ignore-scripts` |
| 端口被占 | `ss -ltnp \| grep 3081` | 换端口 `~/start-dsh.sh 3082` |
| 页面 401 | 用的地址没带 token | 从 `~/dsh-run.log` 重新取 |

**永远不要做的事**：

```sh
export LD_LIBRARY_PATH=/home/intern2/glibc228/usr/lib64:...   # ❌ 会污染整个 shell 树
echo 'export LD_LIBRARY_PATH=...' >> /etc/profile.d/xxx.sh    # ❌ 会污染所有人
```

---

## 7. 与 intern2 的差异

| 项 | intern2（旧） | intern11（新） |
|---|---|---|
| 工具链位置 | `~/.npm-global` + `~/glibc228`（家目录 700，别人用不了） | `/opt/shared-toolchain`（755，共享） |
| node wrapper | 先 export 后 exec（**污染**），2026-11-08 已修 | 一开始就是 ld.so 显式加载 |
| dsh 安装 | 早期方式 | `--ignore-scripts` 避开 koffi 编译 |
| 启动脚本 | `~/bin/dsh-web` | `~/start-dsh.sh`（带自检、拒脏启动） |
| 自检 | 无 | `check-env.sh --deep`（14 项） |
| 端口 | 3080 | 3081 |

> intern2 的 wrapper 已在 2026-11-08 修复为同样的 ld.so 方式，但它的工具链仍在
> 家目录里，别的账号用不了。**如果要给更多账号配置，直接指向 `/opt/shared-toolchain` 即可。**
