# 修复指南：让 DSH 里的 bash/git 恢复可用

> 目标：把 `node` 启动包装脚本从「export LD_LIBRARY_PATH」改成「用 ld.so 显式加载」，
> 使 node 仍然使用 glibc 2.28，但**不再污染子进程环境**。
>
> 影响文件：**仅 1 个** —— `/home/intern2/node-v24.22.2/bin/node`（3 行的 sh 脚本）。
> 不动 `node-real`、不动 `dsh-env.sh`、不动系统 glibc。
>
> 预计耗时：2 分钟。可回滚。

---

## 0. 背景（一句话说清根因）

这台机器是 CentOS 7（glibc 2.17），Node 24 需要 glibc ≥ 2.28，所以有人做了个包装脚本
`node`，在里面 `export LD_LIBRARY_PATH` 后 exec `node-real`。

问题：**`export` 会写进进程环境**，而环境是继承的。`dsh` 用这个 `node` 启动，于是 dsh
及其所有子进程（`bash`、`git`、`xelatex`）都拿到了「2.17 的加载器 + 2.28 的 libc」，
启动即崩：

```
bash: relocation error: /home/intern2/glibc228/usr/lib64/libc.so.6:
symbol _dl_audit_symbind_alt, version GLIBC_PRIVATE not defined in
file ld-linux-x86-64.so.2 with link time reference
```

`node` 自己没事，因为它是被 patchelf 改过 interpreter 的（配对的一对）。坏的是那些
**只是继承了环境变量、但自己没被改过**的程序。

正确做法：**用 ld.so 显式指定库路径**，这样 node 拿到新 glibc，而环境变量从头到尾没被设置。

---

## 1. 先验证「新启动方式」可行（不改任何文件）

```sh
env -u LD_LIBRARY_PATH \
  /home/intern2/glibc228/usr/lib64/ld-2.28.so \
  --library-path /home/intern2/glibc228/usr/lib64:/home/intern2/glibc228/lib64 \
  /home/intern2/node-v24.22.2/bin/node-real -e 'console.log("node ok", process.version)'
```

**期望输出**：`node ok v24.22.2`

- ✅ 能打印 → 继续第 2 步
- ❌ 报错 → **立刻停止**，把报错发回来，不要改任何文件

---

## 2. 备份并改写包装脚本

### 2.1 备份

```sh
cp -a /home/intern2/node-v24.22.2/bin/node \
      /home/intern2/node-v24.22.2/bin/node.wrapper.bak
```

### 2.2 改前（现状，仅 3 行）

```sh
#!/bin/sh
export LD_LIBRARY_PATH=/home/intern2/glibc228/usr/lib64:/home/intern2/glibc228/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
exec /home/intern2/node-v24.22.2/bin/node-real "$@"
```

### 2.3 改后（用 ld.so 显式加载，不设环境变量）

```sh
#!/bin/sh
# Node 24 needs glibc >= 2.28 while the host is glibc 2.17 (CentOS 7).
# Load node-real through the newer loader *explicitly* instead of exporting
# LD_LIBRARY_PATH: an exported loader path leaks into every child process
# (bash, git, latexmk) and makes them abort in the dynamic linker.
# Rollback: cp /home/intern2/node-v24.22.2/bin/node.wrapper.bak \
#              /home/intern2/node-v24.22.2/bin/node
exec /home/intern2/glibc228/usr/lib64/ld-2.28.so \
     --library-path /home/intern2/glibc228/usr/lib64:/home/intern2/glibc228/lib64 \
     /home/intern2/node-v24.22.2/bin/node-real "$@"
```

写入后确认权限没变（应为可执行）：

```sh
ls -l /home/intern2/node-v24.22.2/bin/node
```

### 2.4 注意

- 加载器**只有** `/home/intern2/glibc228/usr/lib64/ld-2.28.so` 这一个文件，
  没有 `ld-linux-x86-64.so.2` 软链，**别写成后者**。
- 用 `#!/bin/sh` 而不是 `#!/bin/bash`：保持和原来一致，`sh` 在这台机器上是正常的。

---

## 3. 验证包装脚本（此时 dsh 还在旧进程里）

```sh
# 1) node 本身能跑
node -v
# 期望：v24.22.2

# 2) 关键：子进程环境里不该再有 LD_LIBRARY_PATH
node -e 'console.log("LD_LIBRARY_PATH=", process.env.LD_LIBRARY_PATH ?? "<unset>")'
# 期望：LD_LIBRARY_PATH= <unset>

# 3) 关键：bash 能起
bash -c 'echo bash-ok'
# 期望：bash-ok

# 4) git 能用
git --version
```

第 2 条是核心 —— 只要它显示 `<unset>`，说明污染源已切断。

---

## 4. 重启 DSH 服务

> ⚠️ 重启会断开当前网页会话。会话记录是持久化的，刷新页面即可恢复。
> ⚠️ 必须在**独立终端**里执行（在 dsh 里跑等于杀掉自己）。

```sh
# 4.1 找到旧进程（端口以实际为准，bin/dsh-web 里写的是 3080）
ss -ltnp | grep -E 'dsh|3080'

# 4.2 记下原启动参数，避免猜错
tr '\0' ' ' < /proc/<PID>/cmdline; echo

# 4.3 停掉
kill <PID>
sleep 3
ss -ltnp | grep 3080 || echo "已停止"

# 4.4 用原命令重新启动（推荐直接用原来的启动脚本）
cd /home/intern2
nohup /home/intern2/bin/dsh-web >> /home/intern2/dsh-web.log 2>&1 &
sleep 8

# 4.5 确认起来了
ss -ltnp | grep 3080
tail -20 /home/intern2/dsh-web.log
```

**注意**：`/home/intern2/bin/dsh-web` 内部是 `exec dsh web --no-open --host 183.36.43.39 --port 3080`，
`dsh` 的 shebang 是 `#!/usr/bin/env node`，所以它会走第 2 步改过的包装脚本 —— 这正是我们想要的。

---

## 5. 验收

新会话里跑：

```sh
bash -c 'echo bash-ok'
git -C /home/intern2/rachel/overleaf_project status --short
```

- 第 1 条输出 `bash-ok` → DSH 的 bash 工具恢复
- 第 2 条能列出改动 → git 正常

然后即可执行积压的推送：

```sh
bash /home/intern2/rachel/push-now.sh
```

---

## 6. 回滚（如果第 5 步不理想）

```sh
cp -a /home/intern2/node-v24.22.2/bin/node.wrapper.bak \
      /home/intern2/node-v24.22.2/bin/node
# 然后按第 4 步重启
```

---

## 7. 附：还有一处临时补丁可以一并撤掉

2026-11-07 我曾改过 DSH 的 node_modules，让子进程环境剥掉 LD_*：

文件：`/home/intern2/.npm-global/lib/node_modules/@deepseek-ai/dsh/node_modules/@deepseek-ai/dsh-subprocess-local/lib/runner-launch-*.js`

- `targetEnvironment()` 里有一行 `for (const name of ["LD_LIBRARY_PATH", "LD_PRELOAD", "LD_AUDIT"]) Reflect.deleteProperty(env, name);`
- 另有一个 `childEnvWithoutLoader()` 辅助函数

**第 2 步修好并重启成功后，这些补丁就不再需要了**，可以删掉恢复原样（删除后需再重启一次）。
留着也无害：环境里本来就没有那个变量时，它只是空转。

---

## 8. 长期建议（不急）

- CentOS 7 已于 2024-06 EOL，后续建议迁到 Rocky Linux 8/9 或 Ubuntu 22.04+，
  Node 24 原生即可运行，不再需要任何 glibc 绕法。
- 或者把 dsh 放进容器（`node:24` 镜像），挂载 `/home/intern2` 与 `/opt`，与宿主机 glibc 彻底解耦。
- **不要再往 `/etc/profile.d` 或 `~/.bashrc` 里加 `export LD_LIBRARY_PATH`** —— 这是
  本次故障的同类做法，会再次污染所有登录 shell。


结果
 能，而且已经修好并验证过了。就按 FIX-NODE-WRAPPER.md 做的：
     
  ┌───────────────────────────┬─────────────────────────────────────────────────────────────────────────────────┐
  │           步骤            │                                      状态                                       │
  ├───────────────────────────┼─────────────────────────────────────────────────────────────────────────────────┤
  │ ① 验证 ld.so 显式加载可行 │ ✅ node ok v24.22.2                                                             │
  ├───────────────────────────┼─────────────────────────────────────────────────────────────────────────────────┤
  │ ② 备份 + 改写包装脚本     │ ✅ 备份 node.wrapper.bak，新脚本用 exec ld-2.28.so --library-path ... node-real │
  ├───────────────────────────┼─────────────────────────────────────────────────────────────────────────────────┤
  │ ③ 验证包装脚本            │ ✅ 见下                                                                         │
  ├───────────────────────────┼─────────────────────────────────────────────────────────────────────────────────┤
  │ ④ 重启 dsh                │ ✅ 已重启，新 PID 11067                                                         │
  ├───────────────────────────┼─────────────────────────────────────────────────────────────────────────────────┤
  │ ⑤ 验收                    │ ✅ 通过                                                                         │
  └───────────────────────────┴─────────────────────────────────────────────────────────────────────────────────┘

  端到端实测（用 dsh 的 PATH 跑 node，再由 node 起子进程）：

  node v24.22.2
  LD= <unset>
  bash-ok
  git version 1.8.3.1

  对比修复前：dsh 进程环境里有 LD_LIBRARY_PATH=/home/intern2/glibc228/...，子进程一启动就 relocation error ... GLIBC_PRIVATE。现在这个变量在 dsh
  及其所有子孙进程里都不存在（/proc/<pid>/environ 里条数 0）。

  还剩两件小事（不影响使用）：

  1. 旧 server 留下的两个孤儿 runner 还在：PID 18893、33043（监听 127.0.0.1 的临时端口）。其中 33043 环境里仍带着污染的 LD_LIBRARY_PATH，但已和 3080
     服务断开，属于残留。要不要我 kill 18893 33043 清掉？（会结束它们挂着的旧会话，网页刷新即恢复）
  2. 文档第 7 节提到的 dsh-subprocess-local 补丁（剥 LD_*）还在。现在属于空转，删不删都行——留着我建议别动，下次 dsh 升级会覆盖掉。

  另外顺带发现：这台机器 PATH 里没有 xelatex/latexmk（command -v 查不到）。文档里把 latexmk 列成受害程序，实际它压根不在宿主机上——如果你在 Overleaf
  编译就无所谓，本地编译才需要装 TeX Live。

  dsh 新地址（token 已变）：
  http://183.36.43.39:3080/?token=bOXqE5lFBBNW4ee6Q-WA3I1kkjG4R4696KyQYPvpl7E