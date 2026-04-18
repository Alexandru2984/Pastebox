<script>
  import { createEventDispatcher, onMount, tick } from 'svelte';
  import { getPaste, forkPaste, getRawUrl } from './lib/api.js';
  import { success, error as toastError } from './lib/toast.js';
  import hljs from 'highlight.js';
  import 'highlight.js/styles/github-dark.css';

  export let pasteId;

  const dispatch = createEventDispatcher();

  let paste = null;
  let loading = true;
  let error = '';
  let copied = false;
  let codeEl;
  let passwordRequired = false;
  let passwordInput = '';
  let currentPassword = '';

  $: if (pasteId) loadPaste();

  async function loadPaste(pw = '') {
    loading = true;
    error = '';
    passwordRequired = false;
    try {
      paste = await getPaste(pasteId, pw || currentPassword);
      currentPassword = pw || currentPassword;
      await tick();
      highlightCode();
    } catch (e) {
      if (e.passwordRequired) {
        passwordRequired = true;
        loading = false;
        return;
      }
      error = e.message || 'Paste not found';
    } finally {
      loading = false;
    }
  }

  function highlightCode() {
    if (!paste || !codeEl) return;
    codeEl.textContent = paste.content;
    delete codeEl.dataset.highlighted;
    if (paste.language === 'auto' || paste.language === 'plaintext') {
      hljs.highlightElement(codeEl);
    } else {
      try {
        codeEl.className = `language-${paste.language}`;
        hljs.highlightElement(codeEl);
      } catch {
        hljs.highlightElement(codeEl);
      }
    }
  }

  async function copyToClipboard() {
    if (!paste) return;
    await navigator.clipboard.writeText(paste.content);
    copied = true;
    success('Copied to clipboard!');
    setTimeout(() => copied = false, 2000);
  }

  function copyLink() {
    navigator.clipboard.writeText(window.location.href);
    success('Link copied!');
  }

  async function handleFork() {
    try {
      const result = await forkPaste(pasteId, {}, currentPassword);
      success('Paste forked!');
      dispatch('navigate', { view: 'view', id: result.id });
    } catch (e) {
      toastError(e.message);
    }
  }

  function handlePasswordSubmit() {
    loadPaste(passwordInput);
  }

  function formatDate(dateStr) {
    return new Date(dateStr + 'Z').toLocaleString();
  }

  function formatSize(bytes) {
    if (bytes < 1024) return `${bytes} B`;
    return `${(bytes / 1024).toFixed(1)} KB`;
  }

  function getLineNumbers(content) {
    const lines = content.split('\n').length;
    return Array.from({ length: lines }, (_, i) => i + 1);
  }
</script>

{#if loading}
  <div class="flex items-center justify-center py-20">
    <div class="text-[var(--text-secondary)] text-lg">Loading paste...</div>
  </div>
{:else if passwordRequired}
  <div class="flex flex-col items-center justify-center py-20 gap-4">
    <div class="text-5xl">🔒</div>
    <h2 class="text-xl text-[var(--text-primary)]">This paste is password protected</h2>
    <form on:submit|preventDefault={handlePasswordSubmit} class="flex gap-2 w-full max-w-sm">
      <input bind:value={passwordInput} type="password" placeholder="Enter password"
        autofocus
        class="flex-1 bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-4 py-2.5
          text-[var(--text-primary)] placeholder-[var(--text-secondary)]
          focus:border-[var(--accent)] focus:outline-none text-sm" />
      <button type="submit"
        class="px-4 py-2.5 bg-[var(--accent)] text-[var(--bg-primary)] rounded-lg cursor-pointer
          border-none font-medium text-sm hover:bg-[var(--accent-hover)]">
        Unlock
      </button>
    </form>
  </div>
{:else if error}
  <div class="flex flex-col items-center justify-center py-20 gap-4">
    <div class="text-6xl">😕</div>
    <div class="text-xl text-[var(--text-secondary)]">{error}</div>
    <button on:click={() => dispatch('navigate', { view: 'create' })}
      class="px-4 py-2 bg-[var(--accent)] text-[var(--bg-primary)] rounded-lg cursor-pointer border-none font-medium text-sm">
      Create a new paste
    </button>
  </div>
{:else if paste}
  <div class="space-y-4">
    {#if paste.burned}
      <div class="bg-orange-900/30 border border-orange-500 text-orange-300 px-4 py-3 rounded-lg text-sm flex items-center gap-2">
        🔥 This paste has been burned. It will not be available after you leave this page.
      </div>
    {/if}

    <!-- Header -->
    <div class="flex flex-col sm:flex-row sm:items-start justify-between gap-4">
      <div>
        <div class="flex items-center gap-2 mb-1">
          <h2 class="text-2xl font-semibold text-[var(--text-primary)]">{paste.title}</h2>
          {#if paste.visibility !== 'public'}
            <span class="text-xs px-2 py-0.5 rounded bg-[var(--bg-tertiary)] text-[var(--text-secondary)]">
              {paste.visibility === 'private' ? '🔒 Private' : '🔗 Unlisted'}
            </span>
          {/if}
          {#if paste.has_password}
            <span class="text-xs px-2 py-0.5 rounded bg-[var(--bg-tertiary)] text-[var(--text-secondary)]">🔑</span>
          {/if}
        </div>
        <div class="flex flex-wrap items-center gap-2 text-sm text-[var(--text-secondary)]">
          <span class="px-2 py-0.5 rounded bg-[var(--bg-tertiary)] text-[var(--accent)] font-mono text-xs">
            {paste.language}
          </span>
          <span>{formatDate(paste.created_at)}</span>
          <span>·</span>
          <span>{paste.views} views</span>
          <span>·</span>
          <span>{formatSize(paste.content.length)}</span>
          {#if paste.expires_at}
            <span>·</span>
            <span class="text-orange-400">⏰ Expires {formatDate(paste.expires_at)}</span>
          {/if}
          {#if paste.parent_id}
            <span>·</span>
            <button on:click={() => dispatch('navigate', { view: 'view', id: paste.parent_id })}
              class="text-[var(--accent)] hover:underline bg-transparent border-none cursor-pointer p-0 text-sm">
              Forked from {paste.parent_id}
            </button>
          {/if}
        </div>
        {#if paste.tags && paste.tags.length > 0}
          <div class="flex gap-1.5 mt-2">
            {#each paste.tags as tag}
              <span class="text-xs px-2 py-0.5 rounded-full bg-[var(--accent)]/20 text-[var(--accent)]">
                #{tag}
              </span>
            {/each}
          </div>
        {/if}
      </div>

      <div class="flex flex-wrap gap-2">
        <button on:click={copyToClipboard}
          class="px-3 py-1.5 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-xs
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors cursor-pointer
            {copied ? 'border-[var(--green)] text-[var(--green)]' : ''}">
          {copied ? '✓ Copied!' : '📋 Copy'}
        </button>
        <button on:click={copyLink}
          class="px-3 py-1.5 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-xs
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors cursor-pointer">
          🔗 Share
        </button>
        <a href={getRawUrl(pasteId)} target="_blank" rel="noopener"
          class="px-3 py-1.5 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-xs
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors no-underline">
          📄 Raw
        </a>
        <button on:click={handleFork}
          class="px-3 py-1.5 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-xs
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors cursor-pointer">
          🍴 Fork
        </button>
        <a href="data:text/plain;charset=utf-8,{encodeURIComponent(paste.content)}"
          download="{paste.title}.{paste.language === 'plaintext' ? 'txt' : paste.language}"
          class="px-3 py-1.5 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-xs
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors no-underline">
          ⬇ Download
        </a>
      </div>
    </div>

    <!-- Code block with line numbers -->
    <div class="relative rounded-lg overflow-hidden border border-[var(--border)]">
      <div class="flex items-center justify-between bg-[var(--bg-tertiary)] px-4 py-2 border-b border-[var(--border)]">
        <span class="text-xs font-mono text-[var(--text-secondary)]">
          {paste.content.split('\n').length} lines
        </span>
        <span class="text-xs font-mono text-[var(--text-secondary)]">{paste.id}</span>
      </div>
      <div class="flex overflow-x-auto">
        <div class="select-none text-right pr-3 pl-3 py-4 text-xs font-mono leading-relaxed
          text-[var(--text-secondary)] bg-[var(--bg-tertiary)]/50 border-r border-[var(--border)]"
          style="min-width: 3rem;"
        >
          {#each getLineNumbers(paste.content) as num}
            <div>{num}</div>
          {/each}
        </div>
        <pre class="m-0 flex-1 overflow-x-auto"><code bind:this={codeEl}
          class="language-{paste.language}"
          style="padding: 1rem !important; font-size: 0.8125rem; line-height: 1.625;"
        ></code></pre>
      </div>
    </div>

    <!-- Embed snippet -->
    <details class="text-sm">
      <summary class="text-[var(--text-secondary)] cursor-pointer hover:text-[var(--text-primary)]">
        Embed this paste
      </summary>
      <div class="mt-2 bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg p-3">
        <code class="text-xs text-[var(--text-secondary)] break-all">
          &lt;iframe src="{window.location.origin}/#/embed/{paste.id}" width="100%" height="400" frameborder="0"&gt;&lt;/iframe&gt;
        </code>
      </div>
    </details>
  </div>
{/if}
