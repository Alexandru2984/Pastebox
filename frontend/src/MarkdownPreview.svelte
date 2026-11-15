<script>
  import { onMount, onDestroy } from 'svelte';
  import { byteLength } from './lib/format.js';
  import { markdownDocument } from './lib/markdown-document.js';
  export let content = '';
  let mounted = false, loading = true, error = '', source = '', clean = '', worker, timer, observer, requestNumber = 0;
  $: if (mounted) renderPreview(content);
  onMount(() => {
    observer = new MutationObserver(() => {if (clean) source = markdownDocument(clean);});
    observer.observe(document.documentElement,{attributes:true,attributeFilter:['style','data-theme']});
    mounted = true;
  });
  onDestroy(() => {requestNumber++;clearTimeout(timer);worker?.terminate();observer?.disconnect();});
  function renderPreview(text) {
    const request = ++requestNumber;
    clearTimeout(timer);worker?.terminate();worker = null;source = '';clean = '';error = '';loading = true;
    if (byteLength(text) > 128 * 1024 || text.split('\n',3002).length > 3000) {error = 'Preview supports up to 128 KB and 3,000 lines. Your full source is unchanged.';loading = false;return;}
    function fail(message) {
      if (request !== requestNumber) return;
      clearTimeout(timer);worker?.terminate();worker = null;error = message;loading = false;
    }
    try {
      worker = new Worker(new URL('./lib/markdown.worker.js',import.meta.url),{type:'module'});
      timer = setTimeout(() => fail('Preview could not load. You can retry or read the source.'),10000);
      worker.onerror = event => {event.preventDefault?.();fail('Preview is unavailable. Your source is unchanged.');};
      worker.onmessage = async ({data}) => {
        if (request !== requestNumber) return;
        if (data.ready) {
          clearTimeout(timer);
          timer = setTimeout(() => fail('Preview took too long. Your source is unchanged.'),1500);
          worker.postMessage({content:text});return;
        }
        clearTimeout(timer);worker?.terminate();worker = null;
        if (data.error || typeof data.html !== 'string') {fail(data.error || 'Preview is unavailable.');return;}
        try {
          const {sanitizeMarkdownHtml} = await import('./lib/markdown-sanitize.js');
          if (request !== requestNumber) return;
          clean = sanitizeMarkdownHtml(data.html);source = markdownDocument(clean);loading = false;
        } catch {fail('This Markdown cannot be previewed safely. Your source is unchanged.');}
      };
    } catch {fail('This browser cannot start the preview. Your source is unchanged.');}
  }
</script>
<div class="markdown-preview">
  {#if loading}<p class="muted" role="status">Preparing Markdown preview…</p>{:else if error}<div class="notice warning" role="alert">{error}<button class="text-button" on:click={() => renderPreview(content)}>Retry preview</button></div>{:else}<iframe title="Markdown preview" srcdoc={source} sandbox="allow-popups allow-popups-to-escape-sandbox" referrerpolicy="no-referrer"></iframe><p class="field-hint">Images stay hidden for privacy. Web links open in a new tab.</p>{/if}
</div>
