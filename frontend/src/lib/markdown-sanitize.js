import DOMPurify from 'dompurify';
const tags = ['p','br','hr','h1','h2','h3','h4','h5','h6','strong','em','del','s','blockquote','ul','ol','li','pre','code','table','thead','tbody','tr','th','td','a','span'];
export function sanitizeMarkdownHtml(html) {
  if (typeof html !== 'string' || html.length > 512 * 1024) throw new Error('Preview is too large');
  const fragment = DOMPurify.sanitize(html,{
    ALLOWED_TAGS:tags,ALLOWED_ATTR:['href','title','align'],ALLOW_DATA_ATTR:false,ALLOW_ARIA_ATTR:false,
    RETURN_DOM_FRAGMENT:true,FORBID_TAGS:['script','style','svg','math','template','form','input','iframe','object','embed','img','audio','video','link','meta','base']
  });
  if (fragment.querySelectorAll('*').length > 5000) throw new Error('Preview is too complex');
  for (const link of fragment.querySelectorAll('a')) {
    const href = link.getAttribute('href');
    try {
      const url = new URL(href || '',window.location.origin);
      if (!href || !['http:','https:'].includes(url.protocol) || url.username || url.password) throw new Error('Non-web link');
      // Only fixed attributes and a URL validated by protocol are added after sanitization.
      link.setAttribute('href',url.href); link.setAttribute('target','_blank');
      link.setAttribute('rel','noopener noreferrer'); link.setAttribute('referrerpolicy','no-referrer');
    } catch {link.removeAttribute('href');}
  }
  const container = document.createElement('div'); container.append(fragment);
  return container.innerHTML;
}
