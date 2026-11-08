// OpenCode Go 额度估算：从 DSH 本地会话日志里还原 token 用量，按 Go 价目表折算成
// “已占用多少美元额度”，再切成 5 小时 / 本周 / 本月三个窗口。
//
// 为什么是“估算”：网关每次响应会回 usage.opencode.allowance（本次请求算掉多少额度），
// 但 DSH 落盘时把 usage 裁剪成 input/output/cacheRead 三个计数，allowance 和 cost 都丢了。
// 所以这里用官方价目表按 token 反算 list_cost，作为 allowance 的近似。
//
// 只统计 DSH 自己发出的请求；opencode 其他客户端（TUI、Claude Code 等）的用量不在这里。
import { readdirSync, statSync } from "node:fs";
import { readFile } from "node:fs/promises";
import { homedir } from "node:os";
import { basename, join } from "node:path";
import * as zlib from "node:zlib";

// ── OpenCode Go 价目表（美元 / 1M tokens）────────────────────────────────────
// 来源：https://opencode.ai/docs/console/go#usage-limits（2026-11 抓取）
// DeepSeek V4.1 Flash 分峰谷价；峰时 = 周一至周五 01:00-04:00 与 06:00-10:00 UTC。
export const MODELS = {
  "deepseek-v4.1-flash": {
    label: "DeepSeek V4.1 Flash",
    monthlyLimit: 100,
    peak: { input: 0.3, output: 1.2, cacheRead: 0.006 },
    offPeak: { input: 0.15, output: 0.6, cacheRead: 0.003 },
    peakWindowsUtc: [
      [1, 4],
      [6, 10],
    ],
    weekdaysOnly: true,
  },
};

export const DEFAULT_MODEL = "deepseek-v4.1-flash";

/** Go 的三档窗口：5 小时 = 月额度的 20%，本周 = 50%，本月 = 100%。 */
export const WINDOW_RATIO = { h5: 0.2, week: 0.5, month: 1 };

function isPeak(ms, model) {
  const d = new Date(ms);
  const day = d.getUTCDay(); // 0 = 周日
  if (model.weekdaysOnly && (day === 0 || day === 6)) return false;
  const hour = d.getUTCHours();
  return model.peakWindowsUtc.some(([from, to]) => hour >= from && hour < to);
}

/** 一条用量记录折算成美元。 */
export function priceOf(rec, model) {
  const p = isPeak(rec.at, model) ? model.peak : model.offPeak;
  return (
    ((rec.input || 0) / 1e6) * p.input +
    ((rec.output || 0) / 1e6) * p.output +
    ((rec.cacheRead || 0) / 1e6) * p.cacheRead
  );
}

// ── 日志扫描 ────────────────────────────────────────────────────────────────

const NUM_KEYS = {
  input: ["input", "inputTokens", "input_tokens", "prompt", "promptTokens", "prompt_tokens"],
  output: [
    "output",
    "outputTokens",
    "output_tokens",
    "completion",
    "completionTokens",
    "completion_tokens",
  ],
  cacheRead: [
    "cacheRead",
    "cacheReadTokens",
    "cache_read",
    "cache_read_input_tokens",
    "cachedInputTokens",
    "cached",
    "cachedTokens",
  ],
};

function pickNumber(obj, keys) {
  for (const k of keys) {
    const v = obj[k];
    if (typeof v === "number" && Number.isFinite(v)) return v;
  }
  return 0;
}

const TIME_KEYS = ["time", "timestamp", "ts", "at", "createdAt", "created", "date"];

function pickTime(obj, fallback) {
  for (const k of TIME_KEYS) {
    const v = obj[k];
    if (typeof v === "number" && v > 1e12) return v;
    if (typeof v === "number" && v > 1e9) return v * 1000;
    if (v && typeof v === "object" && typeof v.created === "number") {
      return v.created > 1e12 ? v.created : v.created * 1000;
    }
    if (typeof v === "string") {
      const t = Date.parse(v);
      if (Number.isFinite(t)) return t;
    }
  }
  return fallback;
}

const MODEL_KEYS = ["model", "modelId", "modelID", "model_id", "providerModel"];

function pickModel(obj) {
  for (const k of MODEL_KEYS) {
    const v = obj[k];
    if (typeof v === "string" && v) return v;
    if (v && typeof v === "object" && typeof v.id === "string") return v.id;
  }
  return undefined;
}

/**
 * 在一个事件对象里找用量。返回 {input, output, cacheRead, at, model} 或 undefined。
 * 容错设计：日志 schema 可能变，这里按“带 input/output 一类计数”就认一条用量。
 */
function extractUsage(obj, fallbackAt) {
  if (!obj || typeof obj !== "object") return undefined;
  for (const containerKey of ["usage", "tokenUsage", "usageMetadata"]) {
    const u = obj[containerKey];
    if (!u || typeof u !== "object") continue;
    const input = pickNumber(u, NUM_KEYS.input);
    const output = pickNumber(u, NUM_KEYS.output);
    const cacheRead = pickNumber(u, NUM_KEYS.cacheRead);
    if (input + output + cacheRead <= 0) continue;
    return {
      input,
      output,
      cacheRead,
      at: pickTime(obj, pickTime(u, fallbackAt)),
      model: pickModel(obj) ?? pickModel(u),
    };
  }
  return undefined;
}

const SESSION_FILES = [
  "session.v4.jsonl.zstd",
  "session.v4.jsonl.zst",
  "session.v4.jsonl",
  "session.jsonl.zstd",
  "session.jsonl",
];

function listDirs(dir) {
  try {
    return readdirSync(dir, { withFileTypes: true })
      .filter((d) => d.isDirectory())
      .map((d) => d.name);
  } catch {
    return [];
  }
}

function candidates(root) {
  const out = [];
  const sessionsDir = join(root, "sessions");
  for (const project of listDirs(sessionsDir)) {
    const projectPath = join(sessionsDir, project);
    for (const session of listDirs(projectPath)) {
      const dir = join(projectPath, session);
      for (const name of SESSION_FILES) {
        const path = join(dir, name);
        try {
          statSync(path);
          out.push({ path, session, project });
          break;
        } catch {
          /* 试下一个 */
        }
      }
    }
  }
  return out;
}

function decompress(buf, path) {
  if (path.endsWith(".zstd") || path.endsWith(".zst")) {
    if (typeof zlib.zstdDecompressSync !== "function") {
      throw new Error("当前 Node 没有 zlib.zstdDecompressSync（需要 Node ≥ 22.15 或 ≥ 23.8）");
    }
    return zlib.zstdDecompressSync(buf).toString("utf8");
  }
  return buf.toString("utf8");
}

/**
 * 扫描本地 DSH 会话日志，返回所有可识别的用量记录。
 * @returns {Promise<{records: Array, diagnostics: object}>}
 */
export async function scanUsage({ root = process.env.DSH_HOME || join(homedir(), ".dsh"), verbose = false } = {}) {
  const files = candidates(root);
  const records = [];
  const diagnostics = {
    root,
    sessionsFound: files.length,
    sessionsRead: 0,
    linesScanned: 0,
    usageFound: 0,
    parseErrors: 0,
    decompressErrors: [],
    samples: [],
  };

  for (const f of files) {
    let text;
    try {
      text = decompress(await readFile(f.path), f.path);
      diagnostics.sessionsRead += 1;
    } catch (e) {
      diagnostics.decompressErrors.push(`${basename(f.path)}: ${e.message}`);
      continue;
    }
    let fallbackAt = Date.now();
    try {
      fallbackAt = statSync(f.path).mtimeMs;
    } catch {
      /* ignore */
    }
    for (const line of text.split("\n")) {
      if (!line.trim()) continue;
      diagnostics.linesScanned += 1;
      let obj;
      try {
        obj = JSON.parse(line);
      } catch {
        diagnostics.parseErrors += 1;
        continue;
      }
      const found = extractUsage(obj, fallbackAt);
      if (!found) {
        if (verbose && diagnostics.samples.length < 3) diagnostics.samples.push(line.slice(0, 500));
        continue;
      }
      diagnostics.usageFound += 1;
      records.push({ ...found, session: f.session, project: f.project, file: basename(f.path) });
    }
  }

  return { records, diagnostics };
}

/**
 * 按 5 小时 / 本周 / 本月三个窗口汇总。
 * @param records 用量记录
 * @param modelId 模型 id（决定价目表和月限额）
 * @param now 参考时刻（毫秒）
 */
export function summarize(records, modelId = DEFAULT_MODEL, now = Date.now()) {
  const model = MODELS[modelId] ?? MODELS[DEFAULT_MODEL];
  const d = new Date(now);
  const monthStart = Date.UTC(d.getUTCFullYear(), d.getUTCMonth(), 1, 0, 0, 0, 0);
  const windows = {
    h5: { from: now - 5 * 3600e3, to: now },
    week: { from: now - 7 * 24 * 3600e3, to: now },
    month: { from: monthStart, to: now },
  };

  const out = {
    model: modelId,
    label: model.label,
    monthlyLimitUsd: model.monthlyLimit,
    now,
    totals: { records: records.length, tokens: { input: 0, output: 0, cacheRead: 0 } },
    windows: {},
  };

  for (const [key, w] of Object.entries(windows)) {
    const limit = model.monthlyLimit * WINDOW_RATIO[key];
    let used = 0;
    let count = 0;
    const byModel = new Map();
    for (const r of records) {
      if (r.at < w.from || r.at > w.to) continue;
      count += 1;
      const usd = priceOf(r, model);
      used += usd;
      const m = r.model ?? modelId;
      const cur = byModel.get(m) ?? { usd: 0, records: 0 };
      cur.usd += usd;
      cur.records += 1;
      byModel.set(m, cur);
    }
    out.windows[key] = {
      from: w.from,
      to: w.to,
      limitUsd: Number(limit.toFixed(2)),
      usedUsd: Number(used.toFixed(4)),
      remainingUsd: Number(Math.max(0, limit - used).toFixed(4)),
      percent: limit > 0 ? Number(((used / limit) * 100).toFixed(2)) : 0,
      records: count,
      byModel: [...byModel.entries()]
        .map(([m, v]) => ({ model: m, usedUsd: Number(v.usd.toFixed(4)), records: v.records }))
        .sort((a, b) => b.usedUsd - a.usedUsd),
    };
  }

  for (const r of records) {
    out.totals.tokens.input += r.input;
    out.totals.tokens.output += r.output;
    out.totals.tokens.cacheRead += r.cacheRead;
  }
  return out;
}

/** 一次性拿到汇总结果（供插件路由与 CLI 共用）。 */
export async function collect({ root, modelId = DEFAULT_MODEL, now = Date.now(), verbose = false } = {}) {
  const { records, diagnostics } = await scanUsage({ root, verbose });
  return { summary: summarize(records, modelId, now), diagnostics };
}
