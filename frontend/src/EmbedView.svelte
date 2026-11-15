<script>
  import { onMount } from 'svelte';
  import { getEmbedPaste } from './lib/api.js';
  import CodeBlock from './CodeBlock.svelte';
  export let pasteId;
  let paste = null, error = '', loading = true;
  onMount(() => {
    let active = true;
    getEmbedPaste(pasteId).then(result => {if (active) paste = result;})
      .catch(e => {if (active) error = e.message;}).finally(() => {if (active) loading = false;});
    return () => {active = false;};
  });
</script>
<div class="embed-view">
  {#if loading}<p class="muted" role="status">Loading snippet…</p>{:else if error}<p class="muted" role="alert">{error}</p>{:else if paste}<div class="embed-header"><strong>{paste.title || 'Untitled'}</strong><a href="{window.location.origin}/#/paste/{paste.id}" target="_blank" rel="noopener noreferrer">Open in PasteBox ↗</a></div><CodeBlock content={paste.content} language={paste.language}/>{/if}
</div>
