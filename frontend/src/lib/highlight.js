import hljs from 'highlight.js/lib/core';
import 'highlight.js/styles/github-dark.css';
const loaders = {
  javascript: () => import('highlight.js/lib/languages/javascript'),
  typescript: () => import('highlight.js/lib/languages/typescript'),
  python: () => import('highlight.js/lib/languages/python'),
  rust: () => import('highlight.js/lib/languages/rust'),
  go: () => import('highlight.js/lib/languages/go'),
  c: () => import('highlight.js/lib/languages/c'),
  cpp: () => import('highlight.js/lib/languages/cpp'),
  java: () => import('highlight.js/lib/languages/java'),
  csharp: () => import('highlight.js/lib/languages/csharp'),
  ruby: () => import('highlight.js/lib/languages/ruby'),
  php: () => import('highlight.js/lib/languages/php'),
  swift: () => import('highlight.js/lib/languages/swift'),
  kotlin: () => import('highlight.js/lib/languages/kotlin'),
  xml: () => import('highlight.js/lib/languages/xml'),
  css: () => import('highlight.js/lib/languages/css'),
  scss: () => import('highlight.js/lib/languages/scss'),
  sql: () => import('highlight.js/lib/languages/sql'),
  bash: () => import('highlight.js/lib/languages/bash'),
  json: () => import('highlight.js/lib/languages/json'),
  yaml: () => import('highlight.js/lib/languages/yaml'),
  markdown: () => import('highlight.js/lib/languages/markdown'),
  dockerfile: () => import('highlight.js/lib/languages/dockerfile'),
  makefile: () => import('highlight.js/lib/languages/makefile'),
  lua: () => import('highlight.js/lib/languages/lua'),
  perl: () => import('highlight.js/lib/languages/perl'),
  r: () => import('highlight.js/lib/languages/r'),
  scala: () => import('highlight.js/lib/languages/scala'),
  haskell: () => import('highlight.js/lib/languages/haskell'),
  elixir: () => import('highlight.js/lib/languages/elixir'),
  nim: () => import('highlight.js/lib/languages/nim')
};
const pending = new Map();
async function load(language) {
  if (!loaders[language]) return false;
  if (!pending.has(language)) pending.set(language, loaders[language]().then(module => hljs.registerLanguage(language,module.default)));
  await pending.get(language); return true;
}
export function highlight(node, initial) {
  let revision = 0;
  async function update({ content, language }) {
    const current = ++revision;
    node.textContent = content; node.className = 'hljs';
    if (content.length > 100000 || language === 'plaintext') return;
    language = ({html:'xml',shell:'bash'})[language] || language;
    try {
      let result;
      if (language === 'auto') {
        const common = ['javascript','python','json','bash','xml','css','sql'];
        await Promise.all(common.map(load));
        if (current !== revision) return;
        result = hljs.highlightAuto(content,common);
      } else {
        if (!await load(language) || current !== revision) return;
        result = hljs.highlight(content,{language,ignoreIllegals:true});
      }
      // highlight.js escapes input before adding its own highlighting markup.
      if (current === revision) node.innerHTML = result.value;
    } catch { if (current === revision) node.textContent = content; }
  }
  update(initial);
  return {update,destroy() {revision++;}};
}
