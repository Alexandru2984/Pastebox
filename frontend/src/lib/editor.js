import { byteLength, MAX_BYTES } from './format.js';
export const TEMPLATES = {
  javascript: { title:'A little JavaScript', language:'javascript', content:'// Small ideas, ready to share.\nconst greet = (name) => `Hello, ${name}!`;\n\nconsole.log(greet("world"));\n' },
  python: { title:'A little Python', language:'python', content:'def greet(name: str) -> str:\n    return f"Hello, {name}!"\n\n\nif __name__ == "__main__":\n    print(greet("world"))\n' },
  json: { title:'A JSON starting point', language:'json', content:'{\n  "name": "my-project",\n  "version": "1.0.0",\n  "ideas": ["build", "share", "repeat"]\n}\n' },
  markdown: { title:'Notes worth sharing', language:'markdown', content:'# A good idea\n\nA short explanation goes here.\n\n## Next steps\n\n- [ ] Try it out\n- [ ] Share what you learn\n\n```js\nconsole.log("Hello, world!");\n```\n' },
  bash: { title:'A shell starting point', language:'bash', content:'#!/usr/bin/env bash\nset -euo pipefail\n\nmain() {\n  printf \'Hello, world!\\n\'\n}\n\nmain "$@"\n' }
};
export function validatePaste({ content, title = '', password = '', tags = '' }) {
  if (!content.trim()) return 'Paste content cannot be empty';
  if (byteLength(content) > MAX_BYTES) return 'Content exceeds the 512 KB limit';
  if (byteLength(title) > 200) return 'Title exceeds the 200-byte limit';
  if (password && byteLength(password) < 4) return 'Min 4 characters (4 UTF-8 bytes)';
  if (byteLength(password) > 256) return 'Password exceeds the 256-byte limit';
  const list = tags.split(',').map(tag => tag.trim()).filter(Boolean);
  if (list.length > 10 || list.some(tag => byteLength(tag) > 50 || /[\x00-\x1f\x7f]/.test(tag))) return 'Use up to 10 tags, each at most 50 bytes';
  return '';
}
export function languageForFile(name) {
  const ext = name.split('.').pop().toLowerCase();
  return ({js:'javascript',ts:'typescript',py:'python',rs:'rust',go:'go',c:'c',cpp:'cpp',h:'c',cs:'csharp',java:'java',rb:'ruby',php:'php',html:'html',css:'css',json:'json',yml:'yaml',yaml:'yaml',md:'markdown',sh:'bash',sql:'sql',xml:'xml',lua:'lua',swift:'swift',kt:'kotlin',cob:'cobol',cbl:'cobol'})[ext] || 'plaintext';
}
