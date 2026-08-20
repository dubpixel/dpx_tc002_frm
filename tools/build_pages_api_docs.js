/**
 * build_pages_api_docs.js — publish the on-device API reference to GH Pages.
 *
 * The device already serves a complete, accurate API reference at /api-ref
 * (usermods/dpx_matrix/dpx_html.h, apiref_html) — endpoints, JSON keys, OSC
 * addresses, MQTT topics, draw commands, copy-paste examples. It's only
 * reachable from the same network as a flashed device, and its dark
 * monospace-terminal styling doesn't match the public site.
 *
 * Rather than hand-author a second copy of this content (which would drift
 * the moment either one is edited), this script extracts the *exact* HTML
 * body the firmware serves and re-skins it with the site's own look
 * (site/index.html's purple/dark theme + a sidebar nav). All ids,
 * table content, and inline <script> logic (copy buttons, live effects
 * fetch) are left untouched — only the <style> block and outer chrome
 * change, so this can never silently diverge from what's actually
 * implemented in dpx_html.h.
 *
 * Usage: node tools/build_pages_api_docs.js
 * Output: site/api/index.html
 */

const fs = require("node:fs");
const path = require("node:path");

const SRC = path.join(__dirname, "..", "usermods", "dpx_matrix", "dpx_html.h");
const OUT_DIR = path.join(__dirname, "..", "site", "api");
const OUT_FILE = path.join(OUT_DIR, "index.html");

const src = fs.readFileSync(SRC, "utf8");
const match = src.match(/apiref_html\[\] PROGMEM = R"APIREF\(\n([\s\S]*?)\n\)APIREF";/);
if (!match) {
  console.error("build_pages_api_docs: couldn't find apiref_html block in " + SRC);
  process.exit(1);
}
let page = match[1];

// Pull out <head> extras (title) and the body content separately so we can
// wrap the original body in a sidebar layout without touching its markup.
const bodyMatch = page.match(/<body>([\s\S]*)<\/body>/);
if (!bodyMatch) {
  console.error("build_pages_api_docs: couldn't find <body> in extracted apiref_html");
  process.exit(1);
}
const body = bodyMatch[1];

// The device page's own nav/TOC block links to /ctrl and /browse (device-
// local pages that don't exist on the static site) and duplicates the
// sidebar we're adding here — strip both, everything else in body is kept.
const bodyStripped = body
  .replace(/<nav>[\s\S]*?<\/nav>\s*/, "")
  .replace(/<div class="toc">[\s\S]*?<\/div>\s*/, "");

const THEME = `
:root {
  color-scheme: dark;
  --purple: #b76ce7;
  --bg: #14121a;
  --panel: #1d1a26;
  --panel-border: #322d40;
  --text: #eee9f5;
  --text-dim: #a89fbc;
}
*{box-sizing:border-box}
body{
  margin:0;
  font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;
  background:radial-gradient(ellipse at top,#201c2b 0%,var(--bg) 55%);
  color:var(--text);
  display:flex;
  min-height:100vh;
}
aside{
  width:220px;
  flex-shrink:0;
  padding:2rem 1.25rem;
  position:sticky;
  top:0;
  height:100vh;
  overflow-y:auto;
  border-right:1px solid var(--panel-border);
}
aside .back{display:block;color:var(--text-dim);text-decoration:none;font-size:0.85rem;margin-bottom:1.5rem}
aside .back:hover{color:var(--text)}
aside .brand{font-weight:600;margin-bottom:1.25rem;font-size:0.95rem}
aside nav a{display:block;color:var(--text-dim);text-decoration:none;font-size:0.85rem;line-height:2.1;padding:0.1rem 0}
aside nav a:hover{color:var(--purple)}
main{flex:1;padding:2.5rem 2.5rem 5rem;min-width:0;font-family:monospace;font-size:13px;line-height:1.6;max-width:900px}
h1{color:var(--purple);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;font-size:1.6rem;margin:0 0 1.5rem}
h2{color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;font-size:1.05rem;margin:2.25rem 0 0.75rem;padding-bottom:0.5rem;border-bottom:1px solid var(--panel-border)}
h2:first-of-type{margin-top:0}
h3{color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;font-size:0.9rem;margin:1rem 0 0.4rem}
table{width:100%;border-collapse:collapse;margin-bottom:0.75rem;font-size:12px;background:var(--panel);border:1px solid var(--panel-border);border-radius:8px;overflow:hidden}
th{background:rgba(183,108,231,0.08);color:var(--purple);text-align:left;padding:0.5rem 0.7rem;font-weight:600}
td{padding:0.4rem 0.7rem;border-top:1px solid var(--panel-border);vertical-align:top;color:var(--text-dim)}
code,.snip{background:rgba(255,255,255,0.07);color:var(--purple);padding:1px 5px;border-radius:4px;font-family:monospace}
.block{position:relative;background:var(--panel);border:1px solid var(--panel-border);border-radius:8px;padding:0.7rem 2.4rem 0.7rem 0.8rem;margin:0.4rem 0;white-space:pre-wrap;font-size:12px;color:var(--text-dim)}
.cp{position:absolute;top:0.5rem;right:0.5rem;background:var(--purple);color:#1a1421;border:none;border-radius:5px;padding:2px 8px;cursor:pointer;font-size:10px;font-weight:600}
.cp:hover{opacity:0.85}
.pill{display:inline-block;padding:1px 7px;border-radius:4px;font-size:10px;margin-right:3px;font-weight:600}
.get{background:rgba(80,220,120,0.15);color:#7f6}
.post{background:rgba(255,160,80,0.15);color:#fb8}
.tag{color:#6b6478;font-size:10px}
.toast{position:fixed;bottom:1rem;right:1rem;background:var(--purple);color:#1a1421;padding:0.5rem 1rem;border-radius:6px;display:none;font-size:12px;font-weight:600;z-index:999}
@media (max-width:800px){body{flex-direction:column}aside{position:relative;width:100%;height:auto;border-right:none;border-bottom:1px solid var(--panel-border)}}
`;

const SIDEBAR_NAV = `
<a href="#endpoints">HTTP Endpoints</a>
<a href="#notify">Notify / Custom App JSON</a>
<a href="#settings">Settings Keys</a>
<a href="#wled">WLED JSON API</a>
<a href="#effects">Effects &amp; Overlays</a>
<a href="#draw">Draw Commands</a>
<a href="#osc">OSC Addresses</a>
<a href="#mqtt">MQTT Topics</a>
`;

const out = `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>API Reference — dpx_tc002_frm</title>
<link rel="icon" href="../fav_icon.png">
<style>${THEME}</style>
</head>
<body>
<aside>
  <a class="back" href="../">&larr; dpx_tc002_frm</a>
  <div class="brand">API Reference</div>
  <nav>${SIDEBAR_NAV}</nav>
</aside>
<main>
<h1>&#128196; API Reference</h1>
<p style="color:var(--text-dim);font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;margin:-1rem 0 1rem;font-size:0.9rem">
  Generated from the firmware's own <code>/api-ref</code> page — always matches what's actually implemented.
  Point <code>[IP]</code> at your device's address (<code>dpx-tc002.local</code> or its IP).
</p>
<p style="font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;margin:0 0 1.5rem">
  <a href="./llms.txt" style="display:inline-block;background:rgba(183,108,231,0.1);border:1px solid var(--purple);color:var(--purple);border-radius:8px;padding:0.5rem 1rem;font-size:0.85rem;text-decoration:none;font-weight:600">&#8681; Download as plain markdown (llms.txt) — for AI agents / offline reading</a>
</p>
${bodyStripped}
</main>
</body>
</html>
`;

fs.mkdirSync(OUT_DIR, { recursive: true });
fs.writeFileSync(OUT_FILE, out);
console.log("Wrote " + OUT_FILE + " (" + out.length + " bytes)");

// ── Agent-digestible markdown version ───────────────────────────────────────
// Same source (bodyStripped, before the HTML restyle above) converted to
// plain markdown instead of styled HTML — a page like this one is exactly
// what LLM agents want to fetch and parse: no CSS/JS noise, just the facts,
// and it can never drift from the graphical version since both come from
// the same extracted apiref_html.

function decodeEntities(s) {
  return s
    .replace(/&nbsp;/g, " ")
    .replace(/&mdash;/g, "—")
    .replace(/&rarr;/g, "->")
    .replace(/&times;/g, "x")
    .replace(/&#(\d+);/g, (_, code) => String.fromCodePoint(parseInt(code, 10)))
    .replace(/&amp;/g, "&")
    .replace(/&lt;/g, "<")
    .replace(/&gt;/g, ">")
    .replace(/&quot;/g, '"')
    .replace(/&#39;/g, "'");
}

function stripTags(s) {
  return decodeEntities(
    s
      .replace(/<code>(.*?)<\/code>/gs, "`$1`")
      .replace(/<a\s+[^>]*href="([^"]*)"[^>]*>(.*?)<\/a>/gs, "[$2]($1)")
      .replace(/<[^>]+>/g, "")
  ).trim().replace(/\s+/g, " ");
}

// Like stripTags, but preserves newlines (only collapses horizontal
// whitespace) — used on the whole document after headings/paragraphs have
// already inserted their own \n\n structure, so that structure survives.
function stripRemainingTags(s) {
  return decodeEntities(
    s
      .replace(/<code>(.*?)<\/code>/gs, "`$1`")
      .replace(/<a\s+[^>]*href="([^"]*)"[^>]*>(.*?)<\/a>/gs, "[$2]($1)")
      .replace(/<[^>]+>/g, "")
  ).split("\n").map((line) => line.replace(/[ \t]+/g, " ").trim()).join("\n");
}

function tableToMarkdown(tableHtml) {
  const rows = [...tableHtml.matchAll(/<tr>([\s\S]*?)<\/tr>/g)].map((m) => m[1]);
  if (!rows.length) return "";
  const cellsOf = (row) => [...row.matchAll(/<t[hd][^>]*>([\s\S]*?)<\/t[hd]>/g)].map((m) => (stripTags(m[1]) || " ").replace(/\|/g, "\\|"));
  const header = cellsOf(rows[0]);
  let md = "| " + header.join(" | ") + " |\n";
  md += "| " + header.map(() => "---").join(" | ") + " |\n";
  for (let i = 1; i < rows.length; i++) {
    md += "| " + cellsOf(rows[i]).join(" | ") + " |\n";
  }
  return md;
}

function htmlToMarkdown(html) {
  let src = html
    .replace(/<script[\s\S]*?<\/script>/g, "")
    .replace(/<\/span>\s*<span/g, "</span> <span") // adjacent pills (e.g. GET+POST on one row)
    .replace(/<div id="fx_list"[^>]*>Loading\.\.\.<\/div>/, "_(fetched live from device — see GET /api/effects)_")
    .replace(/<div id="tr_list"[^>]*>Loading\.\.\.<\/div>/, "_(fetched live from device — see GET /api/transitions)_");
  // Pull out and convert tables first (placeholder tokens so later generic
  // tag-stripping doesn't mangle them).
  const tables = [];
  src = src.replace(/<table>([\s\S]*?)<\/table>/g, (_, inner) => {
    tables.push(tableToMarkdown(inner));
    return ` TABLE${tables.length - 1} `;
  });
  // Code blocks (.block divs) -> fenced code, stripping the copy button.
  const blocks = [];
  src = src.replace(/<div class="block">([\s\S]*?)<\/div>/g, (_, inner) => {
    const code = decodeEntities(inner.replace(/<button[^>]*>copy<\/button>/, "")).trim();
    blocks.push("```\n" + code + "\n```");
    return ` BLOCK${blocks.length - 1} `;
  });
  src = src.replace(/<h1[^>]*>([\s\S]*?)<\/h1>/g, (_, inner) => "\n\n# " + stripTags(inner) + "\n");
  src = src.replace(/<h2[^>]*>([\s\S]*?)<\/h2>/g, (_, inner) => "\n\n## " + stripTags(inner) + "\n");
  src = src.replace(/<h3[^>]*>([\s\S]*?)<\/h3>/g, (_, inner) => "\n\n### " + stripTags(inner) + "\n");
  src = src.replace(/<p[^>]*>([\s\S]*?)<\/p>/g, (_, inner) => "\n" + stripTags(inner) + "\n");
  src = src.replace(/<div[^>]*>|<\/div>/g, "\n");
  src = stripRemainingTags(src);
  src = src.replace(/ TABLE(\d+) /g, (_, i) => "\n\n" + tables[i]);
  src = src.replace(/ BLOCK(\d+) /g, (_, i) => "\n\n" + blocks[i] + "\n");
  return src.replace(/\n{3,}/g, "\n\n").trim();
}

const MD_HEADER = `# dpx_tc002_frm API Reference

Generated from the firmware's own /api-ref page (usermods/dpx_matrix/dpx_html.h)
— always matches what's actually implemented. Point [IP] at your device's
address (dpx-tc002.local or its IP). Graphical version: https://dubpixel.github.io/dpx_tc002_frm/api/

`;

const mdOut = MD_HEADER + htmlToMarkdown(bodyStripped) + "\n";
const MD_FILE = path.join(OUT_DIR, "llms.txt");
fs.writeFileSync(MD_FILE, mdOut);
console.log("Wrote " + MD_FILE + " (" + mdOut.length + " bytes)");
