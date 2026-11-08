# dsh-plugin-opencode-quota

在 DSH Web 界面里看 **OpenCode Go** 套餐额度用掉多少：会话头部一个徽标 `Go $x.xx/$100`，
点开是三档进度条（5 小时 / 本周 / 本月），60 秒自动刷新。

## 它显示的是"估算"，为什么

OpenCode 网关每次响应都会在 `usage` 里回一块 `opencode`：

```json
"usage": {
  "prompt_tokens": 1200, "completion_tokens": 300, "cost": 0.00412,
  "opencode": { "billing_source": "go", "list_cost": 0.00412,
                "allowance": 0.00412, "tokens": {...} }
}
```

`allowance` 就是"这次请求吃掉了多少额度"，是权威数字。**但 DSH 落盘时把 usage 裁剪成了
`inputTokens / outputTokens / cacheReadTokens` 三个计数**（见 `~/.dsh/storages/session_projcache`
里的 `tokenMeasurement`），`allowance` 和 `cost` 都丢了。所以插件只能：

1. 扫本地 DSH 会话日志，还原每条请求的 token；
2. 按官方价目表（DeepSeek V4.1 Flash，峰谷两档）反算美元；
3. 除以 Go 套餐的分档上限。

两个已知偏差：

- **只算 DSH 自己发的请求**。opencode TUI、Claude Code 等其他客户端的用量不在这里。
- **是价目表反算，不是网关记账**。缓存读写、峰谷切换的边界可能带来小误差。

真要看权威数字，去 <https://opencode.ai/console> 的 Activity 页。徽标是"心里有数"用的。

## Go 套餐的限额（2026-11 官方文档）

| 窗口 | 比例 | DeepSeek V4.1 Flash 实际额度 |
|---|---|---|
| 5 小时 | 月额度 20% | $20 |
| 本周 | 月额度 50% | $50 |
| 本月 | 100% | $100 |

峰时 = 周一至周五 `01:00–04:00` 与 `06:00–10:00` UTC（即北京时间 09:00–12:00、14:00–18:00），
其余按谷时价。价格表在 `lib/quota.mjs` 的 `MODELS` 里，换模型改这里即可。

## 装

```bash
cd ~/rachel/plugins/dsh-opencode-quota
bash install.sh            # 只装文件 + 打补丁，不重启
bash install.sh --restart  # 装完顺带重启 DSH（会中断当前会话，历史可恢复）
```

装完刷新 `http://127.0.0.1:24202`。

## 先验证数字，再上界面

CLI 和插件共用同一套聚合逻辑，先在终端看数对不对：

```bash
node bin/opencode-quota.mjs           # 三档进度条
node bin/opencode-quota.mjs --json    # 原始数据
node bin/opencode-quota.mjs --verbose # 识别不到用量时打印样例行
```

如果 `识别出 0 条用量记录`：说明 DSH 的日志 schema 和我猜的不一样。
把 `--verbose` 打印的样例行发我，我改 `lib/quota.mjs` 里的 `extractUsage()` 就行——
它现在是"宽容匹配"（看 `usage` 容器里有没有 input/output 一类计数），不是硬编码 schema。

## 接口自检

```bash
curl -s http://127.0.0.1:24202/plugins/opencode-quota/data | head -c 400
```

应返回 `{"ok":true,"estimate":true,"summary":{...}}`。

## 卸

```bash
bash uninstall.sh   # 删包 + 从 cordis.patch.yml 摘行（原文件会备份）
bash ~/rachel/restart-dsh.sh
```

## 结构

| 文件 | 作用 |
|---|---|
| `lib/quota.mjs` | 价目表 + 日志扫描 + 三窗口汇总（CLI 与插件共用） |
| `lib/index.js` | node 半边：`ctx.webServer.register()` 挂 `GET /plugins/opencode-quota/data` |
| `lib/client.js` | 浏览器半边：`__ModuleLoader__` 工厂 + `ctx.slots.inject('conversation.session.header.actions')` |
| `bin/opencode-quota.mjs` | 独立 CLI |
| `install.sh` / `uninstall.sh` | 装/卸到 `~/.dsh/profiles/web` |

## 排错

- **界面没徽标**：看 `~/dsh-web.log` 里有没有 `opencode-quota`；`curl` 接口若 404，说明 profile
  没加载到包（检查 `~/.dsh/profiles/web/node_modules/dsh-plugin-opencode-quota/` 和
  `cordis.patch.yml` 末尾那三行）。
- **接口 500 / `ok:false`**：多半是 Node 缺 `zlib.zstdDecompressSync`（要 Node ≥ 22.15 或 ≥ 23.8），
  这时先跑一次 CLI，它会把具体错误打出来。
- **想彻底回退**：`bash uninstall.sh` 后重启；补丁文件前面几行是别的东西，别整份删。
