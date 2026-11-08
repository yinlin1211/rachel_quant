# 本机 Clash 代理使用说明

## 一、现状（2026-11-07 已装好并验证）

| 项目 | 值 |
|---|---|
| 内核 | mihomo v1.19.33（Clash.Meta，兼容 Clash for Windows 配置） |
| 安装目录 | `/home/intern2/clash/` |
| 二进制 | `/home/intern2/clash/bin/mihomo` |
| 实际配置 | `/home/intern2/clash/config.yaml` |
| 原始订阅 | `/home/intern2/rachel/vpn/1788267126940.yml` |
| 代理端口 | **127.0.0.1:7891**（HTTP 和 SOCKS5 共用一个口，mixed-port） |
| DNS 端口 | 127.0.0.1:1053（内核自带，一般不用管） |
| 运行状态 | 已在后台运行，`pgrep -af "bin/mihomo -d"` 可查 |
| 验证结果 | `google/generate_204` → 204；出口 IP 45.149.92.7 |

> 端口说明：原 yaml 是 `mixed-port: 7890`，但本机 7890 被一个 14 天前遗留的旧 mihomo 进程（PID 63157）占用，按你的要求改用 **7891**，旧进程未动。如果哪天旧进程没了，想回到 7890，把 `config.yaml` 第一行改成 `mixed-port: 7890` 再重启即可。

---

## 二、启动 / 停止 / 看日志

```bash
# 启动（已在运行会提示，不会重复起）
/home/intern2/clash/start.sh

# 停止
pkill -f "bin/mihomo -d"

# 重启
pkill -f "bin/mihomo -d"; /home/intern2/clash/start.sh

# 看日志
tail -f /home/intern2/clash/clash.log

# 检查配置合法性（不启动，只校验）
/home/intern2/clash/bin/mihomo -d /home/intern2/clash -t
```

注意：**重启机器后不会自动启动**，需要手动跑一次 `start.sh`。

---

## 三、怎么让程序走代理

代理是"opt-in"的：不设环境变量时，程序仍然直连。要用代理，任选一种。

### 1. 当前 shell 临时生效（最常用）

```bash
export http_proxy=http://127.0.0.1:7891
export https_proxy=http://127.0.0.1:7891
export all_proxy=socks5://127.0.0.1:7891   # 给不认 http_proxy 的程序用
```

取消：

```bash
unset http_proxy https_proxy all_proxy
```

想永久生效可以写进 `~/.bashrc`，但**不建议全局开**——本机跑回测/拉数据时走代理可能反而变慢或出问题，建议用的时候再 export。

### 2. 单条命令，不改环境

```bash
curl  -x http://127.0.0.1:7891 https://www.google.com
wget  -e use_proxy=yes -e https_proxy=127.0.0.1:7891 https://example.com/file
```

### 3. 各类工具单独配置

```bash
# git（只给 github 用，推荐按域名配）
git config --global http.https://github.com.proxy http://127.0.0.1:7891
git config --global --unset http.https://github.com.proxy   # 取消

# pip
pip install --proxy http://127.0.0.1:7891 包名

# npm
npm config set proxy http://127.0.0.1:7891
npm config set https-proxy http://127.0.0.1:7891

# conda
conda config --set proxy_servers.http http://127.0.0.1:7891

# docker 拉镜像（需要改 daemon，较重，一般不用）
# 在 /etc/docker/daemon.json 加 "proxies": {"http-proxy": "...", "https-proxy": "..."} 后重启 docker
```

### 4. 验证是否真的走了代理

```bash
# 直连（大概率超时）：应该失败
curl -s -m 8 https://api.ipify.org

# 走代理：应返回境外 IP
curl -s -m 8 -x http://127.0.0.1:7891 https://api.ipify.org
```

---

## 四、换节点 / 改分流

这份配置里没有开 `external-controller`，所以**不能**用网页面板或 API 切节点，只能改配置重启：

1. 编辑 `/home/intern2/clash/config.yaml`
2. 找到 `proxy-groups` 里的 `节点选择`（默认分流都会落到它），把它下面 `proxies:` 列表的**第一个**换成你想用的节点名，例如：
   ```yaml
   - { name: 节点选择, type: select, proxies: [日本-优化, 自动选择, ...] }
   ```
   `自动选择` 是 url-test 组，每 600 秒测一次速自动挑最快的（目前只在日本/新加坡/香港/台湾里选）。
3. 重启：`pkill -f "bin/mihomo -d"; /home/intern2/clash/start.sh`

如果以后想要能动态切节点，我可以给配置加上：

```yaml
external-controller: 127.0.0.1:9090
secret: "自己设一个密码"
```

然后就能用 `curl -X PUT -H "Authorization: Bearer 密码" -d '{"name":"香港-优化"}' http://127.0.0.1:9090/proxies/节点选择` 切，或接 yacd 面板。**没有加是因为 9090 一旦暴露到公网等于把代理送人，需要的话我再加并绑 127.0.0.1。**

---

## 五、常见问题

**1. 启动报 `bind: address already in use`**
端口被占。查：`ss -lntp | grep 789`，换 `mixed-port` 或杀掉占用进程。

**2. 启动卡在 `MMDB invalid, remove and download`**
缺 GeoIP 库，且内核会去 github 下载（本机直连不通）。已在 `/home/intern2/clash/country.mmdb` 放好镜像下载的库，别删。若丢失，重新下：
```bash
curl -L -o /home/intern2/clash/country.mmdb \
  "https://gh-proxy.com/https://github.com/MetaCubeX/meta-rules-dat/releases/download/latest/country.mmdb"
```

**3. 只有本机能用，别的机器连不上**
配置里 `allow-lan: false`。要局域网共享改成 `true`，并且**务必**确认本机防火墙没把 7891 暴露到公网。

**4. 订阅怎么更新**
现在用的是静态导出的 yaml（`1788267126940.yml`）。订阅链接更新后：把新 yaml 覆盖到 `/home/intern2/clash/config.yaml`，**记得把第一行 `mixed-port` 改回 7891**，再重启。

**5. 流量/套餐信息**
yaml 里的 `剩余流量：43.34 GB`、`套餐到期：长期有效` 是机场导出时写死的假节点，只用于显示，别选它们当出口。

---

## 六、文件清单

```
/home/intern2/clash/
├── bin/mihomo       # 内核
├── config.yaml      # 生效配置（由 vpn/1788267126940.yml 复制后改端口）
├── country.mmdb     # GeoIP 库（7.6MB，必须保留）
├── start.sh         # 启动脚本
├── clash.log        # 运行日志
└── mihomo.gz        # 原始下载包，可删

/home/intern2/rachel/vpn/
├── 1788267126940.yml # 原始 Clash for Windows 订阅
└── README.md         # 本文件
```
