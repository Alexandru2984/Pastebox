<script>
  import { createPaste } from './lib/api.js';
  import { LANGUAGES } from './lib/languages.js';
  import { success, error as toastError } from './lib/toast.js';
  import { createEventDispatcher } from 'svelte';

  const dispatch = createEventDispatcher();

  let title = '';
  let content = '';
  let language = 'plaintext';
  let visibility = 'public';
  let expiresIn = '';
  let password = '';
  let burnAfterRead = false;
  let tags = '';
  let submitting = false;
  let error = '';
  let autoDetect = false;

  function handleKeydown(e) {
    if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
      handleSubmit();
    }
    if ((e.ctrlKey || e.metaKey) && e.key === 's') {
      e.preventDefault();
      handleSubmit();
    }
  }

  function handleGlobalKeydown(e) {
    if ((e.ctrlKey || e.metaKey) && e.shiftKey && e.key === 'N') {
      e.preventDefault();
      title = ''; content = ''; language = 'plaintext'; visibility = 'public';
      expiresIn = ''; password = ''; burnAfterRead = false; tags = '';
    }
  }

  async function handleSubmit() {
    if (!content.trim()) { error = 'Paste content cannot be empty'; return; }
    error = '';
    submitting = true;
    try {
      const tagList = tags.split(',').map(t => t.trim()).filter(t => t);
      const result = await createPaste({
        title: title || 'Untitled',
        content,
        language: autoDetect ? 'auto' : language,
        visibility,
        expires_in: expiresIn || undefined,
        password: password || undefined,
        burn_after_read: burnAfterRead,
        tags: tagList.length > 0 ? tagList : undefined
      });
      success('Paste created!');
      title = ''; content = ''; language = 'plaintext'; tags = ''; password = '';
      burnAfterRead = false; expiresIn = ''; visibility = 'public';
      dispatch('navigate', { view: 'view', id: result.id });
    } catch (e) {
      error = e.message;
      toastError(e.message);
    } finally {
      submitting = false;
    }
  }
</script>

<svelte:window on:keydown={handleGlobalKeydown} />

<div class="space-y-5">
  <h2 class="text-2xl font-semibold text-[var(--text-primary)]">Create New Paste</h2>

  {#if error}
    <div class="bg-red-900/30 border border-[var(--red)] text-[var(--red)] px-4 py-3 rounded-lg text-sm">
      {error}
    </div>
  {/if}

  <!-- Title + Language row -->
  <div class="flex flex-col sm:flex-row gap-3">
    <input bind:value={title} type="text" placeholder="Title (optional)"
      class="flex-1 bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-4 py-2.5
        text-[var(--text-primary)] placeholder-[var(--text-secondary)] focus:border-[var(--accent)]
        focus:outline-none transition-colors text-sm" />
    <div class="flex gap-2 items-center">
      <select bind:value={language} disabled={autoDetect}
        class="bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-3 py-2.5
          text-[var(--text-primary)] focus:border-[var(--accent)] focus:outline-none transition-colors
          cursor-pointer text-sm disabled:opacity-50">
        {#each LANGUAGES as lang}
          <option value={lang}>{lang}</option>
        {/each}
      </select>
      <label class="flex items-center gap-1.5 text-xs text-[var(--text-secondary)] whitespace-nowrap cursor-pointer">
        <input type="checkbox" bind:checked={autoDetect}
          class="accent-[var(--accent)]" />
        Auto
      </label>
    </div>
  </div>

  <!-- Options row -->
  <div class="flex flex-wrap gap-3 items-center">
    <select bind:value={visibility}
      class="bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-3 py-2
        text-[var(--text-primary)] text-sm focus:border-[var(--accent)] focus:outline-none cursor-pointer">
      <option value="public">🌍 Public</option>
      <option value="unlisted">🔗 Unlisted</option>
      <option value="private">🔒 Private</option>
    </select>

    <select bind:value={expiresIn}
      class="bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-3 py-2
        text-[var(--text-primary)] text-sm focus:border-[var(--accent)] focus:outline-none cursor-pointer">
      <option value="">⏰ Never expires</option>
      <option value="1h">1 hour</option>
      <option value="24h">24 hours</option>
      <option value="7d">7 days</option>
      <option value="30d">30 days</option>
    </select>

    <input bind:value={password} type="password" placeholder="🔑 Password (optional)"
      class="bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-3 py-2
        text-[var(--text-primary)] placeholder-[var(--text-secondary)] text-sm
        focus:border-[var(--accent)] focus:outline-none transition-colors w-44" />

    <label class="flex items-center gap-1.5 text-sm text-[var(--text-secondary)] cursor-pointer">
      <input type="checkbox" bind:checked={burnAfterRead} class="accent-[var(--accent)]" />
      🔥 Burn after read
    </label>
  </div>

  <!-- Tags -->
  <input bind:value={tags} type="text" placeholder="Tags (comma separated, e.g. python, tutorial, snippet)"
    class="w-full bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-4 py-2.5
      text-[var(--text-primary)] placeholder-[var(--text-secondary)] text-sm
      focus:border-[var(--accent)] focus:outline-none transition-colors" />

  <!-- Editor -->
  <textarea bind:value={content} on:keydown={handleKeydown}
    placeholder="Paste your code here... (Ctrl+Enter or Ctrl+S to submit)"
    rows="18" spellcheck="false"
    class="w-full bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-4 py-3
      text-[var(--text-primary)] placeholder-[var(--text-secondary)] font-mono text-sm leading-relaxed
      focus:border-[var(--accent)] focus:outline-none transition-colors resize-y"
  ></textarea>

  <!-- Footer -->
  <div class="flex flex-col sm:flex-row justify-between items-start sm:items-center gap-3">
    <div class="flex flex-wrap gap-3 text-xs text-[var(--text-secondary)]">
      <span>{content.length.toLocaleString()} chars</span>
      <span>·</span>
      <span>{content.split('\n').length} lines</span>
      {#if content.length > 512 * 1024}
        <span class="text-[var(--red)]">⚠ Exceeds 512KB limit</span>
      {/if}
    </div>
    <div class="flex gap-2">
      <span class="text-xs text-[var(--text-secondary)] self-center hidden sm:inline">
        Ctrl+Shift+N: clear · Ctrl+Enter: submit
      </span>
      <button on:click={handleSubmit} disabled={submitting}
        class="px-5 py-2.5 bg-[var(--accent)] text-[var(--bg-primary)] font-semibold rounded-lg
          hover:bg-[var(--accent-hover)] transition-colors disabled:opacity-50 cursor-pointer border-none text-sm">
        {submitting ? 'Creating...' : 'Create Paste'}
      </button>
    </div>
  </div>
</div>
