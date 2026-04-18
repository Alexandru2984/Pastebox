<script>
  import { createEventDispatcher, onMount } from 'svelte';
  import { getPaste } from './lib/api.js';
  import hljs from 'highlight.js';
  import 'highlight.js/styles/github-dark.css';

  export let pasteId;

  const dispatch = createEventDispatcher();

  let paste = null;
  let loading = true;
  let error = '';
  let copied = false;
  let codeEl;

  $: if (pasteId) loadPaste();

  async function loadPaste() {
    loading = true;
    error = '';
    try {
      paste = await getPaste(pasteId);
    } catch (e) {
      error = 'Paste not found';
    } finally {
      loading = false;
    }
  }

  $: if (paste && codeEl) {
    setTimeout(() => {
      codeEl.textContent = paste.content;
      delete codeEl.dataset.highlighted;
      hljs.highlightElement(codeEl);
    }, 0);
  }

  async function copyToClipboard() {
    if (!paste) return;
    await navigator.clipboard.writeText(paste.content);
    copied = true;
    setTimeout(() => copied = false, 2000);
  }

  function copyLink() {
    navigator.clipboard.writeText(window.location.href);
  }

  function formatDate(dateStr) {
    return new Date(dateStr + 'Z').toLocaleString();
  }

  function formatSize(bytes) {
    if (bytes < 1024) return `${bytes} B`;
    return `${(bytes / 1024).toFixed(1)} KB`;
  }
</script>

{#if loading}
  <div class="flex items-center justify-center py-20">
    <div class="text-[var(--text-secondary)] text-lg">Loading paste...</div>
  </div>
{:else if error}
  <div class="flex flex-col items-center justify-center py-20 gap-4">
    <div class="text-6xl">😕</div>
    <div class="text-xl text-[var(--text-secondary)]">{error}</div>
    <button
      on:click={() => dispatch('navigate', 'create')}
      class="px-4 py-2 bg-[var(--accent)] text-[var(--bg-primary)] rounded-lg cursor-pointer border-none font-medium"
    >
      Create a new paste
    </button>
  </div>
{:else if paste}
  <div class="space-y-4">
    <!-- Paste header -->
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-2xl font-semibold text-[var(--text-primary)] mb-1">{paste.title}</h2>
        <div class="flex items-center gap-3 text-sm text-[var(--text-secondary)]">
          <span class="px-2 py-0.5 rounded bg-[var(--bg-tertiary)] text-[var(--accent)] font-mono text-xs">
            {paste.language}
          </span>
          <span>{formatDate(paste.created_at)}</span>
          <span>·</span>
          <span>{paste.views} views</span>
          <span>·</span>
          <span>{formatSize(paste.content.length)}</span>
        </div>
      </div>
      <div class="flex gap-2">
        <button
          on:click={copyToClipboard}
          class="px-4 py-2 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-sm
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors cursor-pointer
            {copied ? 'border-[var(--green)] text-[var(--green)]' : ''}"
        >
          {copied ? '✓ Copied!' : '📋 Copy'}
        </button>
        <button
          on:click={copyLink}
          class="px-4 py-2 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-sm
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors cursor-pointer"
        >
          🔗 Share
        </button>
        <a
          href="data:text/plain;charset=utf-8,{encodeURIComponent(paste.content)}"
          download="{paste.title}.{paste.language === 'plaintext' ? 'txt' : paste.language}"
          class="px-4 py-2 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-sm
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors
            no-underline flex items-center"
        >
          ⬇ Download
        </a>
      </div>
    </div>

    <!-- Code block -->
    <div class="relative rounded-lg overflow-hidden border border-[var(--border)]">
      <div class="flex items-center justify-between bg-[var(--bg-tertiary)] px-4 py-2 border-b border-[var(--border)]">
        <span class="text-xs font-mono text-[var(--text-secondary)]">
          {paste.content.split('\n').length} lines
        </span>
        <span class="text-xs font-mono text-[var(--text-secondary)]">
          {paste.id}
        </span>
      </div>
      <pre class="m-0 overflow-x-auto"><code
        bind:this={codeEl}
        class="language-{paste.language}"
      ></code></pre>
    </div>
  </div>
{/if}
