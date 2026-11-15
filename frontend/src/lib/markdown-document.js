export function markdownDocument(cleanHtml) {
  const root = getComputedStyle(document.documentElement);
  const color = (name,fallback) => {
    const value = root.getPropertyValue(name).trim();
    return /^#[a-f\d]{6}$/i.test(value) ? value : fallback;
  };
  const bg = color('--bg-secondary','#161b22'), text = color('--text-primary','#e6edf3');
  const muted = color('--text-secondary','#8b949e'), border = color('--border','#30363d'), accent = color('--accent','#58a6ff');
  return `<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'none'; style-src 'unsafe-inline'; img-src 'none'; base-uri 'none'; form-action 'none'"><meta name="referrer" content="no-referrer"><meta http-equiv="x-dns-prefetch-control" content="off"><style>
*{box-sizing:border-box}body{margin:0 auto;max-width:950px;padding:24px;background:${bg};color:${text};font:14px/1.7 -apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;overflow-wrap:anywhere}h1,h2,h3{line-height:1.3;margin:1.3em 0 .6em}h1{font-size:28px}h2{font-size:22px}h3{font-size:18px}body>:first-child{margin-top:0}a{color:${accent}}a:focus-visible{outline:2px solid ${accent};outline-offset:3px}p{margin:0 0 1em}blockquote{border-left:3px solid ${border};margin-left:0;padding-left:16px;color:${muted}}code{font:12px/1.8 ui-monospace,SFMono-Regular,Consolas,monospace}pre{padding:14px;border:1px solid ${border};border-radius:8px;overflow:auto;white-space:pre;max-width:100%}table{display:block;border-collapse:collapse;max-width:100%;overflow:auto}td,th{padding:8px 12px;border:1px solid ${border};text-align:left}hr{border:0;border-top:1px solid ${border};margin:22px 0}ul,ol{padding-left:23px}span{overflow-wrap:anywhere}@media(max-width:500px){body{padding:16px;font-size:14px}h1{font-size:24px}}
</style></head><body>${cleanHtml}</body></html>`;
}
