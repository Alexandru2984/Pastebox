<script>
  import { createEventDispatcher, onMount, tick } from 'svelte';
  import { getPaste, forkPaste, updatePaste, getRawUrl } from './lib/api.js';
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
  let passwordInputEl;

  // Edit mode
  let editing = false;
  let editTitle = '';
  let editContent = '';
  let editLanguage = '';
  let editVisibility = '';
  let saving = false;

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
        await tick();
        if (passwordInputEl) passwordInputEl.focus();
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

  function startEdit() {
    editTitle = paste.title;
    editContent = paste.content;
    editLanguage = paste.language;
    editVisibility = paste.visibility || 'public';
    editing = true;
  }

  function cancelEdit() {
    editing = false;
  }

  async function saveEdit() {
    saving = true;
    try {
      await updatePaste(pasteId, {
        title: editTitle,
        content: editContent,
        language: editLanguage,
        visibility: editVisibility
      }, currentPassword);
      success('Paste updated!');
      editing = false;
      await loadPaste();
    } catch (e) {
      toastError(e.message);
    } finally {
      saving = false;
    }
  }

  function handlePasswordSubmit() {
    loadPaste(passwordInput);
  }

  function handlePasswordKeydown(e) {
    if (e.key === 'Escape') {
      dispatch('navigate', { view: 'create' });
    }
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
  <div class="flex items-center justify-center py-20" role="status" aria-live="polite">
    <div class="text-[var(--text-secondary)] text-lg">Loading paste...</div>
  </div>
{:else if passwordRequired}
  <div class="flex flex-col items-center justify-center py-20 gap-4" role="dialog" aria-label="Password required">
    <div class="text-5xl" aria-hidden="true">🔒</div>
    <h2 class="text-xl text-[var(--text-primary)]">This paste is password protected</h2>
    <form on:submit|preventDefault={handlePasswordSubmit} class="flex gap-2 w-full max-w-sm">
      <label class="sr-only" for="pw-unlock">Password</label>
      <input bind:this={passwordInputEl} bind:value={passwordInput} id="pw-unlock" type="password"
        placeholder="Enter password" on:keydown={handlePasswordKeydown}
        aria-label="Enter password to unlock paste"
        class="flex-1 bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-4 py-2.5
          text-[var(--text-primary)] placeholder-[var(--text-secondary)]
          focus:border-[var(--accent)] focus:outline-none text-sm" />
      <button type="submit"
        class="px-4 py-2.5 bg-[var(--accent)] text-[var(--bg-primary)] rounded-lg cursor-pointer
          border-none font-medium text-sm hover:bg-[var(--accent-hover)]"
        aria-label="Unlock paste">
        Unlock
      </button>
    </form>
  </div>
{:else if error}
  <div class="flex flex-col items-center justify-center py-20 gap-4" role="alert">
    <div class="text-6xl" aria-hidden="true">😕</div>
    <div class="text-xl text-[var(--text-secondary)]">{error}</div>
    <button on:click={() => dispatch('navigate', { view: 'create' })}
      class="px-4 py-2 bg-[var(--accent)] text-[var(--bg-primary)] rounded-lg cursor-pointer border-none font-medium text-sm">
      Create a new paste
    </button>
  </div>
{:else if paste}
  <article class="space-y-4" aria-label="Paste: {paste.title}">
    {#if paste.burned}
      <div class="bg-orange-900/30 border border-orange-500 text-orange-300 px-4 py-3 rounded-lg text-sm flex items-center gap-2"
        role="alert">
        <span aria-hidden="true">🔥</span> This paste has been burned. It will not be available after you leave this page.
      </div>
    {/if}

    <!-- Header -->
    <header class="flex flex-col sm:flex-row sm:items-start justify-between gap-4">
      <div>
        <div class="flex items-center gap-2 mb-1">
          <h2 class="text-2xl font-semibold text-[var(--text-primary)]">{paste.title}</h2>
          {#if paste.visibility !== 'public'}
            <span class="text-xs px-2 py-0.5 rounded bg-[var(--bg-tertiary)] text-[var(--text-secondary)]"
              aria-label="{paste.visibility} paste">
              {paste.visibility === 'private' ? '🔒 Private' : '🔗 Unlisted'}
            </span>
          {/if}
          {#if paste.has_password}
            <span class="text-xs px-2 py-0.5 rounded bg-[var(--bg-tertiary)] text-[var(--text-secondary)]"
              aria-label="Password protected">🔑</span>
          {/if}
        </div>
        <div class="flex flex-wrap items-center gap-2 text-sm text-[var(--text-secondary)]">
          <span class="px-2 py-0.5 rounded bg-[var(--bg-tertiary)] text-[var(--accent)] font-mono text-xs">
            {paste.language}
          </span>
          <span>{formatDate(paste.created_at)}</span>
          <span aria-hidden="true">·</span>
          <span>{paste.views} views</span>
          <span aria-hidden="true">·</span>
          <span>{formatSize(paste.content.length)}</span>
          {#if paste.expires_at}
            <span aria-hidden="true">·</span>
            <span class="text-orange-400">⏰ Expires {formatDate(paste.expires_at)}</span>
          {/if}
          {#if paste.parent_id}
            <span aria-hidden="true">·</span>
            <button on:click={() => dispatch('navigate', { view: 'view', id: paste.parent_id })}
              class="text-[var(--accent)] hover:underline bg-transparent border-none cursor-pointer p-0 text-sm"
              aria-label="View parent paste {paste.parent_id}">
              Forked from {paste.parent_id}
            </button>
          {/if}
        </div>
        {#if paste.tags && paste.tags.length > 0}
          <div class="flex gap-1.5 mt-2" role="list" aria-label="Tags">
            {#each paste.tags as tag}
              <span class="text-xs px-2 py-0.5 rounded-full bg-[var(--accent)]/20 text-[var(--accent)]" role="listitem">
                #{tag}
              </span>
            {/each}
          </div>
        {/if}
      </div>

      <div class="flex flex-wrap gap-2" role="toolbar" aria-label="Paste actions">
        <button on:click={copyToClipboard}
          aria-label={copied ? 'Copied!' : 'Copy to clipboard'}
          class="px-3 py-1.5 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-xs
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors cursor-pointer
            {copied ? 'border-[var(--green)] text-[var(--green)]' : ''}">
          {copied ? '✓ Copied!' : '📋 Copy'}
        </button>
        <button on:click={copyLink} aria-label="Copy share link"
          class="px-3 py-1.5 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-xs
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors cursor-pointer">
          🔗 Share
        </button>
        <a href={getRawUrl(pasteId)} target="_blank" rel="noopener" aria-label="View raw paste"
          class="px-3 py-1.5 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-xs
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors no-underline">
          📄 Raw
        </a>
        {#if !paste.burned}
          <button on:click={startEdit} aria-label="Edit paste"
            class="px-3 py-1.5 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-xs
              font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors cursor-pointer">
            ✏️ Edit
          </button>
        {/if}
        <button on:click={handleFork} aria-label="Fork paste"
          class="px-3 py-1.5 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-xs
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors cursor-pointer">
          🍴 Fork
        </button>
        <a href="data:text/plain;charset=utf-8,{encodeURIComponent(paste.content)}"
          download="{paste.title}.{paste.language === 'plaintext' ? 'txt' : paste.language}"
          aria-label="Download paste as file"
          class="px-3 py-1.5 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-xs
            font-medium text-[var(--text-primary)] hover:border-[var(--accent)] transition-colors no-underline">
          ⬇ Download
        </a>
      </div>
    </header>

    {#if editing}
      <!-- Edit mode -->
      <section class="space-y-3 bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg p-4"
        aria-label="Edit paste">
        <label class="sr-only" for="edit-title">Title</label>
        <input id="edit-title" bind:value={editTitle} type="text" placeholder="Title"
          class="w-full bg-[var(--bg-primary)] border border-[var(--border)] rounded-lg px-4 py-2.5
            text-[var(--text-primary)] text-sm focus:border-[var(--accent)] focus:outline-none" />
        <label class="sr-only" for="edit-content">Content</label>
        <textarea id="edit-content" bind:value={editContent} rows="18" spellcheck="false"
          class="w-full bg-[var(--bg-primary)] border border-[var(--border)] rounded-lg px-4 py-3
            text-[var(--text-primary)] font-mono text-sm leading-relaxed
            focus:border-[var(--accent)] focus:outline-none resize-y"></textarea>
        <div class="flex gap-2 justify-end">
          <button on:click={cancelEdit}
            class="px-4 py-2 bg-[var(--bg-tertiary)] border border-[var(--border)] rounded-lg text-sm
              text-[var(--text-primary)] cursor-pointer hover:border-[var(--accent)]">
            Cancel
          </button>
          <button on:click={saveEdit} disabled={saving}
            class="px-4 py-2 bg-[var(--accent)] text-[var(--bg-primary)] rounded-lg text-sm font-medium
              cursor-pointer border-none hover:bg-[var(--accent-hover)] disabled:opacity-50">
            {saving ? 'Saving...' : 'Save Changes'}
          </button>
        </div>
      </section>
    {:else}
      <!-- Code block with line numbers -->
      <div class="relative rounded-lg overflow-hidden border border-[var(--border)]" role="region" aria-label="Code content">
        <div class="flex items-center justify-between bg-[var(--bg-tertiary)] px-4 py-2 border-b border-[var(--border)]">
          <span class="text-xs font-mono text-[var(--text-secondary)]">
            {paste.content.split('\n').length} lines
          </span>
          <span class="text-xs font-mono text-[var(--text-secondary)]">{paste.id}</span>
        </div>
        <div class="flex overflow-x-auto">
          <div class="select-none text-right pr-3 pl-3 py-4 text-xs font-mono leading-relaxed
            text-[var(--text-secondary)] bg-[var(--bg-tertiary)]/50 border-r border-[var(--border)]"
            style="min-width: 3rem;" aria-hidden="true"
          >
            {#each getLineNumbers(paste.content) as num}
              <div>{num}</div>
            {/each}
          </div>
          <pre class="m-0 flex-1 overflow-x-auto"><code bind:this={codeEl}
            class="language-{paste.language}"
            style="padding: 1rem !important; font-size: 0.8125rem; line-height: 1.625;"
            tabindex="0" aria-label="Code content"
          ></code></pre>
        </div>
      </div>
    {/if}

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
  </article>
{/if}

<style>
  .sr-only {
    position: absolute; width: 1px; height: 1px; padding: 0; margin: -1px;
    overflow: hidden; clip: rect(0,0,0,0); white-space: nowrap; border: 0;
  }
</style>
