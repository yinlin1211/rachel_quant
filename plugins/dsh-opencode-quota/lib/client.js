// 插件浏览器半边：会话头部放一个 “Go 额度” 小徽标，点开看三档进度条。
// 形态照抄 dsh-client-ui-jobs 的预构建产物：__ModuleLoader__ 工厂 + require 注入。
window.__ModuleLoader__.load({
  id: "dsh-plugin-opencode-quota",
  factory: (require) => {
    var module = { exports: {} };
    var exports = module.exports;
    Object.defineProperty(exports, Symbol.toStringTag, { value: "Module" });

    const React = require("react");
    const h = React.createElement;
    const ENDPOINT = "/plugins/opencode-quota/data";

    const COLORS = { h5: "#f59e0b", week: "#3b82f6", month: "#10b981" };
    const LABELS = { h5: "5 小时", week: "本周", month: "本月" };

    function money(n) {
      if (typeof n !== "number" || !Number.isFinite(n)) return "-";
      if (n < 0.01 && n > 0) return "<$0.01";
      return "$" + n.toFixed(2);
    }

    function useQuota() {
      const [state, setState] = React.useState({ status: "loading", body: undefined });
      React.useEffect(() => {
        let alive = true;
        const load = async () => {
          try {
            const res = await fetch(ENDPOINT, { headers: { accept: "application/json" } });
            const body = await res.json();
            if (alive) {
              setState({
                status: body && body.ok ? "ok" : "error",
                body,
                at: Date.now(),
              });
            }
          } catch (e) {
            if (alive) setState({ status: "error", body: { error: String((e && e.message) || e) } });
          }
        };
        load();
        const timer = setInterval(load, 60000);
        return () => {
          alive = false;
          clearInterval(timer);
        };
      }, []);
      return state;
    }

    function Bar(props) {
      const pct = Math.max(0, Math.min(100, props.percent || 0));
      return h(
        "div",
        { style: { height: 4, borderRadius: 2, background: "rgba(127,127,127,.25)", overflow: "hidden" } },
        h("div", { style: { width: pct + "%", height: "100%", background: props.color } }),
      );
    }

    function Panel(props) {
      const { summary, diagnostics } = props;
      const rows = ["h5", "week", "month"].map((key) => {
        const w = summary.windows[key];
        return h(
          "div",
          { key, style: { marginBottom: 10 } },
          h(
            "div",
            { style: { display: "flex", justifyContent: "space-between", fontSize: 11, marginBottom: 3 } },
            h("span", { style: { opacity: 0.85 } }, LABELS[key]),
            h(
              "span",
              null,
              money(w.usedUsd) + " / " + money(w.limitUsd) + "  (" + w.percent + "%)",
            ),
          ),
          h(Bar, { percent: w.percent, color: COLORS[key] }),
        );
      });
      const month = summary.windows.month;
      return h(
        "div",
        {
          style: {
            position: "absolute",
            top: 34,
            right: 0,
            zIndex: 40,
            width: 268,
            padding: 12,
            borderRadius: 10,
            background: "var(--dsh-popover-bg, #1f2124)",
            color: "var(--dsh-popover-fg, #e6e6e6)",
            boxShadow: "0 8px 28px rgba(0,0,0,.45)",
            border: "1px solid rgba(127,127,127,.28)",
            font: "12px/1.45 system-ui, -apple-system, 'Segoe UI', sans-serif",
          },
        },
        h(
          "div",
          { style: { display: "flex", justifyContent: "space-between", marginBottom: 8 } },
          h("strong", { style: { fontSize: 12 } }, "OpenCode Go 额度"),
          h("span", { style: { opacity: 0.6, fontSize: 10 } }, summary.label),
        ),
        rows,
        h(
          "div",
          { style: { opacity: 0.62, fontSize: 10, marginTop: 6, lineHeight: 1.5 } },
          "估算值：按官方价目表把本地 token 折成美元。只含 DSH 发出的请求；",
          "含其他客户端的真实数字请查 opencode.ai/console。",
        ),
        h(
          "div",
          { style: { opacity: 0.45, fontSize: 10, marginTop: 4 } },
          month.records + " 条用量记录 · " + (diagnostics ? diagnostics.sessionsRead : 0) + " 个会话",
        ),
      );
    }

    function QuotaBadge() {
      const state = useQuota();
      const [open, setOpen] = React.useState(false);
      const summary = state.body && state.body.summary;
      const month = summary && summary.windows ? summary.windows.month : undefined;
      const percent = month ? month.percent : 0;
      const color = percent >= 90 ? "#ef4444" : percent >= 70 ? "#f59e0b" : "#10b981";
      const label =
        state.status === "ok" && month
          ? "Go " + money(month.usedUsd) + "/" + money(month.limitUsd)
          : state.status === "loading"
            ? "Go 额度…"
            : "Go 额度 ?";

      return h(
        "div",
        { style: { position: "relative", display: "inline-flex" } },
        h(
          "button",
          {
            type: "button",
            onClick: () => setOpen((v) => !v),
            title:
              state.status === "ok"
                ? "本月已用 " + money(month.usedUsd) + " / " + money(month.limitUsd) + "（估算）"
                : "额度数据不可用，点开看原因",
            style: {
              display: "inline-flex",
              alignItems: "center",
              gap: 6,
              padding: "3px 8px",
              borderRadius: 999,
              border: "1px solid rgba(127,127,127,.32)",
              background: "transparent",
              color: "inherit",
              font: "inherit",
              fontSize: 11,
              cursor: "pointer",
            },
          },
          h("span", {
            style: { width: 6, height: 6, borderRadius: 3, background: color, display: "inline-block" },
          }),
          label,
        ),
        open
          ? summary
            ? h(Panel, { summary, diagnostics: state.body.diagnostics })
            : h(
                "div",
                {
                  style: {
                    position: "absolute",
                    top: 34,
                    right: 0,
                    zIndex: 40,
                    width: 268,
                    padding: 12,
                    borderRadius: 10,
                    background: "var(--dsh-popover-bg, #1f2124)",
                    color: "var(--dsh-popover-fg, #e6e6e6)",
                    font: "12px/1.5 system-ui, sans-serif",
                    boxShadow: "0 8px 28px rgba(0,0,0,.45)",
                    border: "1px solid rgba(127,127,127,.28)",
                  },
                },
                h("strong", null, "拿不到额度数据"),
                h(
                  "div",
                  { style: { opacity: 0.7, marginTop: 6, wordBreak: "break-all" } },
                  String((state.body && state.body.error) || "还在加载或接口未就绪"),
                ),
              )
          : null,
      );
    }

    const inject = ["slots"];

    function apply(ctx) {
      ctx.slots.inject("conversation.session.header.actions", () =>
        ctx.slots.register(
          {
            name: "conversation.session.header.actions",
            id: "opencode-quota",
            order: 30,
          },
          QuotaBadge,
        ),
      );
    }

    exports.apply = apply;
    exports.inject = inject;
    return module.exports;
  },
});
