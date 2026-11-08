#!/usr/bin/env node
// OpenCode Go 额度估算 CLI —— 先在终端验证数字对不对，再上界面。
//
//   node bin/opencode-quota.mjs
//   node bin/opencode-quota.mjs --json
//   node bin/opencode-quota.mjs --verbose     # 打印解析不到用量的样例行，便于修 schema
//   node bin/opencode-quota.mjs --root /path/to/.dsh --model deepseek-v4.1-flash
import { collect, MODELS } from "../lib/quota.mjs";

function arg(name, fallback) {
  const i = process.argv.indexOf("--" + name);
  if (i === -1) return fallback;
  const v = process.argv[i + 1];
  return v && !v.startsWith("--") ? v : true;
}

const asJson = process.argv.includes("--json");
const verbose = process.argv.includes("--verbose");
const root = typeof arg("root", undefined) === "string" ? arg("root") : undefined;
const model = typeof arg("model", "deepseek-v4.1-flash") === "string" ? arg("model") : "deepseek-v4.1-flash";

const { summary, diagnostics } = await collect({ root, modelId: model, verbose });

if (asJson) {
  console.log(JSON.stringify({ summary, diagnostics }, null, 2));
  process.exit(0);
}

const money = (n) => (n >= 0.01 ? "$" + n.toFixed(2) : n > 0 ? "<$0.01" : "$0.00");
const bar = (pct) => {
  const width = 28;
  const filled = Math.max(0, Math.min(width, Math.round((pct / 100) * width)));
  return "█".repeat(filled) + "░".repeat(width - filled);
};
const label = { h5: "5 小时", week: "本周", month: "本月" };

console.log("");
console.log(`OpenCode Go 额度估算 · ${summary.label} · 月额度 $${summary.monthlyLimitUsd}`);
console.log(`数据源：${diagnostics.root}/sessions（仅统计 DSH 自己发出的请求）`);
console.log("");

for (const key of ["h5", "week", "month"]) {
  const w = summary.windows[key];
  console.log(
    `  ${label[key].padEnd(6)} ${bar(w.percent).replace(/░/g, "·")} ${String(w.percent).padStart(6)}%` +
      `   ${money(w.usedUsd)} / ${money(w.limitUsd)}   剩余 ${money(w.remainingUsd)}`,
  );
}

console.log("");
console.log(
  `  扫描：${diagnostics.sessionsRead}/${diagnostics.sessionsFound} 个会话，` +
    `${diagnostics.linesScanned} 行，识别出 ${diagnostics.usageFound} 条用量记录` +
    (diagnostics.parseErrors ? `，${diagnostics.parseErrors} 行解析失败` : ""),
);
if (diagnostics.decompressErrors.length) {
  console.log("  解压失败：");
  for (const e of diagnostics.decompressErrors.slice(0, 5)) console.log("    - " + e);
}
if (diagnostics.usageFound === 0) {
  console.log("");
  console.log("  ⚠ 一条用量都没识别到。加上 --verbose 再跑一次，把输出的样例行发给我，");
  console.log("    我按真实 schema 调 extractUsage()。");
}
if (diagnostics.samples.length) {
  console.log("");
  console.log("  样例行：");
  for (const s of diagnostics.samples) console.log("    " + s);
}
console.log("");
console.log(`  已知模型价目表：${Object.keys(MODELS).join(", ")}`);
console.log("  峰时 = 周一至周五 01:00–04:00 / 06:00–10:00 UTC，其余按谷时价。");
console.log("");
