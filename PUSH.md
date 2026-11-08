# 开题报告 → Overleaf 推送说明

> 一句话：**中英文同步好之后，在 `overleaf_project/` 里 commit + push 到 Overleaf 的 `main` 分支。**

## 1. 仓库在哪

| 项 | 值 |
|---|---|
| 本地路径 | `/home/intern2/rachel/overleaf_project` |
| 远程 | `https://git.overleaf.com/6abfa45ff52425f8480d9abb`（Overleaf 项目 `6abfa45ff52425f8480d9abb`） |
| 分支 | `main` |
| 认证 | Overleaf git token 已内嵌在 `.git/config` 的 remote URL 里（`https://git:olp_xxx@git.overleaf.com/...`） |

⚠️ token 就在 `.git/config` 里，**不要打印出来、不要复制到任何会被提交的文件**。也正因为 token 是明文，这个目录别随便打包发人。

## 2. 推送前必须做：中英文同步

两版是**一一对应**的，改一处就要改另一处：

| 中文版 | 英文版 |
|---|---|
| `main.tex` + `chapter/*.tex` | `main_en.tex` + `en/*.tex` |
| `config.tex`（题目、姓名、导师、Fandol 字体） | `config_en.tex`（同上，英文） |

对照清单（逐个过一遍）：

- 章节标题与顺序：`01` Topic Selection Report / `02` Research Design / `03` Thesis Writing Plan
- 小节结构：研究背景 7 节、研究设计 12 节（含 L0–L3、稳健性、已有基础、困难、可行性、资源、进度、预期成果）
- 表格：分层定义表、已完成基础表、进度表 —— 行数、列名、数字要一致
- 编号：研究现状（1）–（5）、预期成果 1–4
- 关键数字：427 个交易日、4.37 亿条/天、47 秒、40.1%、1,800 个候选、13–19 bp、0.052

## 3. 推送步骤

```bash
cd /home/intern2/rachel/overleaf_project

# ① 看改了什么（.gitignore 已忽略 *.aux *.log main.pdf 等编译产物）
git status --short --branch

# ② 建议先本地编译确认没报错
latexmk -xelatex main.tex
latexmk -xelatex main_en.tex

# ③ 提交并推送
git add -A
git commit -m "Sync EN with CN: <一句话说明这次改了什么>"
git push origin main
```

**推送被拒**（远程有新提交，Overleaf 网页上改过）时，先拉再推：

```bash
git pull --ff-only origin main
git push origin main
```

## 4. 环境坑：`LD_LIBRARY_PATH` 会让所有命令失效

如果命令直接报错、**连 bash 都没执行**（退码 127）：

```
bash: relocation error: /home/intern2/glibc228/usr/lib64/libc.so.6:
symbol _dl_audit_symbind_alt, version GLIBC_PRIVATE not defined ...
```

说明进程环境里有：

```bash
LD_LIBRARY_PATH=/home/intern2/glibc228/usr/lib64:/home/intern2/glibc228/lib64
```

这套自定义 glibc 与系统 `ld-linux` 不匹配，git / latexmk / python 全挂。

- **在终端里**（命令还能跑的情况下）：`unset LD_LIBRARY_PATH` 后重试，或 `env -u LD_LIBRARY_PATH <cmd>`。
- **如果 bash 本身起不来**（人已经进不了 shell，比如在 dsh 里）：必须在**启动 dsh 之前**处理，例如 `env -u LD_LIBRARY_PATH dsh web`；进去以后再 `env -u` 是没用的，因为 bash 已经先崩了。
- ⚠️ 但本机 node 24 依赖这套 glibc（CentOS 7 只有 glibc 2.17），**直接 unset 会让 dsh 起不来**。正确做法是用 `ld-2.28.so --library-path` 只给 node 加载新 glibc，不设环境变量。现成脚本：根目录 `restart-dsh.sh`（先 `--dry-run` 探测，再去掉参数执行）。

## 5. 怎么确认推成功了

```bash
git log --oneline -3     # 本地最新提交
git status               # 应显示 "Your branch is up to date with 'origin/main'"
```

然后打开 Overleaf 项目页面，看新版本是否正常编译、正文页数是否仍 ≥ 10 页。

## 6. 当前状态（2026-11-07）

- 本地 `main` 与 `origin/main` 都是 `1530e74`（Complete bilingual proposal…），**没有落后**。
- 待推送：中英同步改动 3 处 ——
  1. `02` 章标题从误写的 `Topic Selection Report` 改为 `Research Design`（中英各一处）
  2. 英文版补 `Expected Outcomes` 小节（原来塞在进度表后、行内编号）
  3. 英文版研究现状加（1）–（5）编号
- 尚未编译验证、尚未提交。
