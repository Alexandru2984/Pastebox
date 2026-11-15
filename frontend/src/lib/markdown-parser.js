import { Marked } from 'marked';
export const MAX_PREVIEW_BYTES = 128 * 1024;
const MAX_HTML_SIZE = 512 * 1024;
function escapeHtml(text) {return text.replace(/[&<>"']/g,char => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'})[char]);}
const parser = new Marked({gfm:true,async:false,renderer:{
  html({text}) {return escapeHtml(text);},
  image({text}) {return `<span>[Image hidden: ${escapeHtml(text)}]</span>`;},
  checkbox({checked}) {return checked ? '☑ ' : '☐ ';}
}});
export function parseMarkdown(content) {
  if (typeof content !== 'string' || new TextEncoder().encode(content).length > MAX_PREVIEW_BYTES || content.split('\n',3002).length > 3000)
    throw new Error('Preview supports up to 128 KB and 3,000 lines. Your full source remains available.');
  const html = parser.parse(content);
  if (typeof html !== 'string' || html.length > MAX_HTML_SIZE) throw new Error('This preview is too large. Your full source remains available.');
  let elements = 0;
  for (const _tag of html.matchAll(/<[a-z][^>]*>/gi)) if (++elements > 5000) throw new Error('This preview is too complex. Your full source remains available.');
  return html;
}
