import { describe, it, expect } from 'vitest';
import { parseMarkdown } from '../lib/markdown-parser.js';
import { sanitizeMarkdownHtml } from '../lib/markdown-sanitize.js';
import { markdownDocument } from '../lib/markdown-document.js';
function renderSource(source) {
  const container = document.createElement('div');
  container.innerHTML = sanitizeMarkdownHtml(parseMarkdown(source));
  return container;
}

describe('Safe Markdown rendering', () => {
  it('renders headings, lists, tasks, tables and escaped fenced code', () => {
    const result = renderSource('# Heading\n\n- [x] done\n- [ ] next\n\n| A | B |\n| - | - |\n| 1 | 2 |\n\n```html\n<script>alert(1)</script>\n```');
    expect(result.querySelector('h1').textContent).toBe('Heading');
    expect(result.querySelector('table')).toBeTruthy();
    expect(result.textContent).toContain('☑');
    expect(result.querySelector('pre code').textContent).toContain('<script>alert(1)</script>');
    expect(result.querySelector('script,input')).toBeNull();
  });
  it('keeps raw HTML inert and never turns images into network resources', () => {
    const source = '<img src="https://tracker.invalid/pixel" onerror="alert(1)">\n\n<script>alert(1)</script>\n\n![private image](https://tracker.invalid/paste-id)';
    const result = renderSource(source);
    expect(result.querySelector('script,img')).toBeNull();
    expect(result.textContent).toContain('<img');
    expect(result.textContent).toContain('[Image hidden: private image]');
    expect(result.querySelector('[src]')).toBeNull();
  });
  it.each(['javascript:alert(1)','data:text/html,evil','vbscript:msgbox(1)','file:///etc/passwd','https://user:password@example.com/path'])('refuses a non-web or credential-bearing link: %s', href => {
    const result = renderSource(`[click](${href})`);
    expect(result.querySelector('a[href]')).toBeNull();
  });
  it('opens explicitly clicked web links with no opener or referrer', () => {
    const result = renderSource('[docs](https://example.com/docs)');
    const link = result.querySelector('a');
    expect(link.getAttribute('href')).toBe('https://example.com/docs');
    expect(link.getAttribute('target')).toBe('_blank');
    expect(link.getAttribute('rel')).toBe('noopener noreferrer');
    expect(link.getAttribute('referrerpolicy')).toBe('no-referrer');
  });
  it('strips executable and clobbering markup even from unexpected parser HTML', () => {
    const html = '<svg><a onload="alert(1)">x</a></svg><math><mtext>x</mtext></math><form id="parent"><input name="location"></form><p style="background:url(https://tracker.invalid)" onclick="alert(1)">safe</p><iframe srcdoc="evil"></iframe>';
    const holder = document.createElement('div'); holder.innerHTML = sanitizeMarkdownHtml(html);
    expect(holder.querySelector('svg,math,form,input,iframe,script,style,[id],[name],[onclick],[style]')).toBeNull();
    expect(holder.textContent).toContain('safe');
  });
  it('bounds UTF-8 source, line count and generated markup before DOM sanitization', () => {
    expect(() => parseMarkdown('😀'.repeat(32769))).toThrow(/128 KB/);
    expect(() => parseMarkdown('line\n'.repeat(3001))).toThrow(/3,000 lines/);
    expect(() => parseMarkdown('**a** '.repeat(6000))).toThrow(/complex/);
  });
  it('builds a self-contained document with scripts and external fetches disabled', () => {
    const doc = new DOMParser().parseFromString(markdownDocument('<p>safe</p>'),'text/html');
    expect(doc.querySelector('meta[http-equiv="Content-Security-Policy"]').content).toContain("default-src 'none'");
    expect(doc.querySelector('meta[http-equiv="Content-Security-Policy"]').content).toContain("script-src 'none'");
    expect(doc.querySelector('meta[name="referrer"]').content).toBe('no-referrer');
    expect(doc.querySelector('body').textContent).toBe('safe');
  });
});
