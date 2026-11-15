export const MAX_BYTES = 512 * 1024;
export function byteLength(text) { return new TextEncoder().encode(text).length; }
export function formatSize(bytes = 0) { return bytes < 1024 ? `${bytes} B` : `${(bytes / 1024).toFixed(1)} KB`; }
export function parseDate(text) {
  return new Date(/[zZ]$|[+-]\d\d:\d\d$/.test(text) ? text : text.replace(' ', 'T') + 'Z');
}
export function timeAgo(text) {
  const seconds = Math.max(0, Math.floor((Date.now() - parseDate(text)) / 1000));
  if (!Number.isFinite(seconds)) return '';
  if (seconds < 60) return 'just now';
  if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
  if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ago`;
  return `${Math.floor(seconds / 86400)}d ago`;
}
export async function copyText(text) {
  if (!navigator.clipboard?.writeText) throw new Error('Clipboard unavailable. Select and copy the text manually.');
  await navigator.clipboard.writeText(text);
}
export const EXTENSIONS = { plaintext:'txt', auto:'txt', cobol:'cob', javascript:'js', typescript:'ts', python:'py', ruby:'rb', rust:'rs', cpp:'cpp', csharp:'cs', bash:'sh', shell:'sh', markdown:'md', yaml:'yml', dockerfile:'Dockerfile', makefile:'Makefile' };
export function safeFilename(title, language) {
  const base = (title || 'paste').normalize('NFKC').replace(/[^\p{L}\p{N}_. -]/gu,'_').replace(/^\.+/, '').slice(0,100) || 'paste';
  const extension = EXTENSIONS[language] || language.replace(/[^a-z0-9]/gi,'').slice(0,12) || 'txt';
  return base.toLowerCase().endsWith(`.${extension.toLowerCase()}`) || (['Dockerfile','Makefile'].includes(extension) && base === extension) ? base : `${base}.${extension}`;
}
