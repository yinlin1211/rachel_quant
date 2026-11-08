# 项目记忆（AGENTS.md）

本文件记录跨会话需要记住的环境事实与约定。修改前请保持简洁，只写“容易被忘记、且猜不到”的事实。

## 1. 仓库与推送

工作区里有**两个** git 仓库，别搞混：

| 仓库 | 路径 | 远程 | 分支 |
|---|---|---|---|
| Overleaf 开题报告 | `overleaf_project/.git` | `https://git.overleaf.com/6abfa45ff52425f8480d9abb` | `main` |
| GitHub 量化项目 | `.git`（工作区根） | `https://github.com/yinlin1211/rachel_quant.git` | `master` |

- **推送开题报告 = 推 Overleaf 仓库**，完整步骤（同步清单、命令、排错、验证）见根目录 **[PUSH.md](PUSH.md)**，命令速查：
  ```bash
  cd overleaf_project && git add -A && git commit -m "..." && git push origin main
  ```
- `overleaf_project/.gitignore` 已忽略 `*.aux *.log main.pdf main_en.pdf` 等编译产物，`git add -A` 不会把产物带进去。
- Overleaf 的 git token 已内嵌在 `overleaf_project/.git/config` 的 remote URL 里（形如 `https://git:olp_xxx@git.overleaf.com/...`），**不要复制到任何会被提交的文件**，也不要打印到聊天或日志里。
- 推送历史可从 `overleaf_project/.git/logs/` 查：最近一次推送为 `1530e74`（Complete bilingual proposal…），当时本地与 `origin/main` 一致。

## 2. 环境坑：LD_LIBRARY_PATH 让所有命令失效

DSH 进程环境带有：

```
LD_LIBRARY_PATH=/home/intern2/glibc228/usr/lib64:/home/intern2/glibc228/lib64
```

这套自定义 glibc 与系统 `ld-linux` 的 `GLIBC_PRIVATE` 符号不匹配，**bash 一启动就报 `relocation error`**，git / latexmk / python 同理，表现为所有 shell 命令都不可用（退码 127）。

- 判断方法：命令返回 `relocation error ... _dl_audit_symbind_alt ... GLIBC_PRIVATE`。
- 修复：启动 dsh 前 `unset LD_LIBRARY_PATH`，或 `env -u LD_LIBRARY_PATH dsh web`；单条命令用 `env -u LD_LIBRARY_PATH <cmd>`。
- 副作用：shell 不可用时**无法编译 PDF、无法 git push**，只能用 read/write/glob/grep 等文件工具。
- **根因不是误设（待验证）**：本机是 CentOS 7（glibc 2.17），dsh 要 node ≥ 24.9，node 24 官方二进制要 glibc ≥ 2.28，所以 `/home/intern2/glibc228` 很可能是 node 能跑的前提；`node22/bin/` 下 `node` 与 `node.orig` 并存，说明 node 被 patch 过（可能已带 rpath）。**结论未实机验证**：直接 unset 后 dsh 未必起不来，但存在这个风险，所以重启脚本会先探针再决定。
- 正确做法：只给 node 进程用 glibc228，不设环境变量——`env -u LD_LIBRARY_PATH <ld-2.28.so> --library-path <glibc 目录> <node> <args>`，这样子进程回归系统 glibc。
- **一键重启脚本**：`bash /home/intern2/rachel/restart-dsh.sh --dry-run`（只探测不杀进程），确认后去掉 `--dry-run` 执行。脚本会从 `/proc/<pid>/cmdline` 读出原启动命令原样重启、自动探测 clean/loader 两种方式、做健康检查、失败自动回滚。**必须在独立终端跑**，不要在 dsh 会话内执行（会杀掉自己）。
- **共享工具链（2026-11-08 新增）**：`/opt/shared-toolchain/`（755，任何账号可用）放着
  glibc 2.28 + node 24.22.2，wrapper 用 `ld.so --library-path` 显式加载、**不设环境变量**。
  配套自检脚本：`/opt/shared-toolchain/check-env.sh --deep`（14 项，出问题先跑这个）。
- **账号 11（intern11）已配置完成**：dsh 0.2.2-rc.1 装在 `~/.npm-global`，
  启动用 `~/start-dsh.sh`（默认端口 **3081**；intern2 占 3080），
  启动脚本会先自检、检测到 `LD_LIBRARY_PATH` 污染直接拒绝启动。
  完整记录见 `ACCOUNT11-SETUP.md`。装 dsh 必须加 `--ignore-scripts`
  （`koffi` 要从源码编译，本机 GCC 4.8.5 会失败并导致整包回滚）。
- **已打的本地补丁（2026-11-07）**：`node_modules/@deepseek-ai/dsh-subprocess-local/lib/runner-launch-*.js` 的 `targetEnvironment()` 里加了一段，把 `LD_LIBRARY_PATH/LD_PRELOAD/LD_AUDIT` 从**目标进程**环境里删掉（bootstrap/node 那条路走 `childEnv()`，不受影响），这样子进程回归系统 glibc。**删除那段 for 循环即可恢复原样**；npm 升级 dsh 后补丁会丢失，需重打。注意副作用：dsh 里跑 bt5 那类需要 glibc228 的程序时，要自己显式 `LD_LIBRARY_PATH=... cmd`。

## 3. 开题报告（bilingual proposal）

- 目录 `overleaf_project/`：中文版 `main.tex` + `chapter/`，英文版 `main_en.tex` + `en/`；两版**一一对应**，改一处必须同步另一处。
- 官方要求：**正文（main body）不少于 10 页**；封面无页码、guideline 页用罗马数字，均不计入。
- 章节：`01` Topic Selection Report（研究背景）、`02` Research Design（研究设计）、`03` Thesis Writing Plan（写作计划）。
- 排版：XeLaTeX + Biber；中文用 Fandol 字体（`config.tex`），英文不需 xeCJK（`config_en.tex`）。
- 已完成的同步：`02` 章标题从误写的 `Topic Selection Report` 改为 `Research Design`（中英各一处）；英文版补齐 `Expected Outcomes` 小节、研究现状加（1）–（5）编号，与中文版结构对齐。

## 3.5 模型走的是 OpenCode Go 网关（额度相关）

- DSH 的 `llm-pi-ai` 被 `~/.dsh/profiles/web/cordis.patch.yml` 改成了 **anthropic 协议 + `https://opencode.ai/inference/go/anthropic`**，token 从 `ANTHROPIC_AUTH_TOKEN` 取，模型 id 写死 `deepseek-v4.1-flash`。也就是说我们用的是 **OpenCode Go（$10/月）**套餐，不是 DeepSeek 官方 API。
- Go 的“额度”是**按模型的美元月额度**：5 小时 = 20%、本周 = 50%、本月 = 100%。DeepSeek V4.1 Flash 月额度 $100。峰时（UTC 周一至五 01–04、06–10）单价翻倍。
- 网关每次响应会回 `usage.opencode.allowance`（本次吃掉多少额度），**但 DSH 落盘时把 usage 裁成 inputTokens/outputTokens/cacheReadTokens，allowance/cost 丢弃**；`/api/usage` 之类接口是 404，没有公开查用量 API。所以本地只能按价目表反算估算值。
- 相关产物：`plugins/dsh-opencode-quota/`（插件 + CLI，见其 README）。

## 4. 提交要求（来自 `开题报告要求/开题报告要求.txt`）

- 截止：11 月 13 日 23:00 前在 Blackboard 提交，勾选查重，重复率 < 20%。
- 命名：报告 `XXX_Thesis Proposal.pdf`，查重 `XXX_Originality Report.pdf`，两份一并发邮件给导师和 humengnan@szu.edu.cn。
- 答辩需带最新纸质版；首次不通过的修改若仍不达标，延毕至少半年。
