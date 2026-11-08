// 插件 node 半边：注册一个只读 HTTP 路由，把 OpenCode Go 额度估算以 JSON 吐给浏览器。
// 路由名与 dsh-client-hmr 的 /plugins/events 同级，用 exact 路径，不会撞上 /plugins 前缀。
import { collect, DEFAULT_MODEL } from "./quota.mjs";

/** 浏览器半边读取的地址（含插件 id，便于和 /plugins/<id>/client.js 归到一起）。 */
export const DATA_ENDPOINT = "/plugins/opencode-quota/data";

const CACHE_MS = 30_000;
let cache = { at: 0, body: undefined };

async function payload() {
  const now = Date.now();
  if (cache.body && now - cache.at < CACHE_MS) return cache.body;
  try {
    const { summary, diagnostics } = await collect({ modelId: MODEL_ID });
    const body = {
      ok: true,
      generatedAt: now,
      // 单位说明：Go 的额度就是“美元额度”，这里给的是按官方价目表反算的估算值。
      estimate: true,
      summary,
      diagnostics: {
        sessionsFound: diagnostics.sessionsFound,
        sessionsRead: diagnostics.sessionsRead,
        usageFound: diagnostics.usageFound,
        parseErrors: diagnostics.parseErrors,
        decompressErrors: diagnostics.decompressErrors.slice(0, 3),
      },
    };
    cache = { at: now, body };
    return body;
  } catch (error) {
    const body = { ok: false, generatedAt: now, error: String(error?.message ?? error) };
    // 失败不缓存，方便修好后立刻恢复
    return body;
  }
}

let MODEL_ID = DEFAULT_MODEL;

export const name = "opencode-quota";

/** 只需要 webserver 注册路由；不依赖会话服务，挂了也不影响主流程。 */
export const inject = ["webServer"];

export function apply(ctx, config = {}) {
  MODEL_ID = typeof config.model === "string" && config.model ? config.model : DEFAULT_MODEL;
  ctx.effect(() => {
    const dispose = ctx.webServer.register({
      kind: "exact",
      path: DATA_ENDPOINT,
      handler: async (req, res) => {
        if (req.method !== "GET" && req.method !== "HEAD") {
          res.writeHead(405, { allow: "GET, HEAD" });
          res.end();
          return;
        }
        let body;
        try {
          body = await payload();
        } catch (error) {
          body = { ok: false, error: String(error?.message ?? error) };
        }
        const json = JSON.stringify(body);
        res.writeHead(200, {
          "content-type": "application/json; charset=utf-8",
          "cache-control": "no-store",
          "content-length": Buffer.byteLength(json),
        });
        res.end(req.method === "HEAD" ? undefined : json);
      },
    });
    return () => dispose();
  }, "opencode-quota: data route");
}
