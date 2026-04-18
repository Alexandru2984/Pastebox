<script>
  import { onMount, tick } from 'svelte';
  import { getPaste } from './lib/api.js';
  import hljs from 'highlight.js';
  import 'highlight.js/styles/github-dark.css';

  export let pasteId;

  let paste = null;
  let loading = true;
  let error = '';
  let codeEl;

  onMount(async () => {
    try {
      paste = await getPaste(pasteId);
      await tick();
      if (codeEl && paste) {
        codeEl.textContent = paste.content;
        hljs.highlightElement(codeEl);
      }
    } catch (e) {
      if (e.passwordRequired) {
        error = 'This paste is password protected';
      } else {
        error = 'Paste not found';
      }
    } finally {
      loading = false;
    }
  });

  function getLineNumbers(content) {
    return Array.from({ length: content.split('\n').length }, (_, i) => i + 1);
  }
</script>

<div style="background: var(--bg-primary); color: var(--text-primary); font-family: monospace; margin: 0; padding: 0; min-height: 100vh;">
  {#if loading}
    <div style="padding: 2rem; text-align: center; color: var(--text-secondary);">Loading...</div>
  {:else if error}
    <div style="padding: 2rem; text-align: center; color: var(--text-secondary);">{error}</div>
  {:else if paste}
    <div style="font-size: 11px; padding: 4px 8px; background: var(--bg-tertiary); color: var(--text-secondary);
      display: flex; justify-content: space-between; border-bottom: 1px solid var(--border);">
      <span>{paste.title} · {paste.language}</span>
      <a href="{window.location.origin}/#/paste/{paste.id}"
        target="_blank" rel="noopener"
        style="color: var(--accent); text-decoration: none;">
        Open in PasteBox ↗
      </a>
    </div>
    <div style="display: flex; overflow-x: auto;">
      <div style="text-align: right; padding: 8px 6px; font-size: 12px; line-height: 1.6;
        color: var(--text-secondary); background: rgba(33,38,45,0.5); border-right: 1px solid var(--border);
        user-select: none; min-width: 2.5rem;">
        {#each getLineNumbers(paste.content) as num}
          <div>{num}</div>
        {/each}
      </div>
      <pre style="margin: 0; flex: 1; overflow-x: auto;"><code bind:this={codeEl}
        class="language-{paste.language}"
        style="padding: 8px !important; font-size: 12px; line-height: 1.6;"
      ></code></pre>
    </div>
  {/if}
</div>
