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
  let passwordError = '';

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

  function validatePassword() {
    if (password && password.length < 4) {
      passwordError = 'Min 4 characters';
      return false;
    }
    if (password.length > 256) {
      passwordError = 'Max 256 characters';
      return false;
    }
    passwordError = '';
    return true;
  }

  async function handleSubmit() {
    if (!content.trim()) { error = 'Paste content cannot be empty'; return; }
    if (password && !validatePassword()) return;
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

<section class="space-y-5" aria-label="Create new paste">
  <h2 class="text-2xl font-semibold text-[var(--text-primary)]">Create New Paste</h2>

  {#if error}
    <div class="bg-red-900/30 border border-[var(--red)] text-[var(--red)] px-4 py-3 rounded-lg text-sm"
      role="alert" aria-live="assertive">
      {error}
    </div>
  {/if}

  <form on:submit|preventDefault={handleSubmit} aria-label="Paste creation form">
    <!-- Title + Language row -->
    <div class="flex flex-col sm:flex-row gap-3 mb-4">
      <label class="sr-only" for="paste-title">Title</label>
      <input id="paste-title" bind:value={title} type="text" placeholder="Title (optional)"
        aria-label="Paste title"
        class="flex-1 bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-4 py-2.5
          text-[var(--text-primary)] placeholder-[var(--text-secondary)] focus:border-[var(--accent)]
          focus:outline-none transition-colors text-sm" />
      <div class="flex gap-2 items-center">
        <label class="sr-only" for="paste-language">Language</label>
        <select id="paste-language" bind:value={language} disabled={autoDetect}
          aria-label="Programming language"
          class="bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-3 py-2.5
            text-[var(--text-primary)] focus:border-[var(--accent)] focus:outline-none transition-colors
            cursor-pointer text-sm disabled:opacity-50">
          {#each LANGUAGES as lang}
            <option value={lang}>{lang}</option>
          {/each}
        </select>
        <label class="flex items-center gap-1.5 text-xs text-[var(--text-secondary)] whitespace-nowrap cursor-pointer">
          <input type="checkbox" bind:checked={autoDetect}
            aria-label="Auto-detect language"
            class="accent-[var(--accent)]" />
          Auto
        </label>
      </div>
    </div>

    <!-- Options row -->
    <div class="flex flex-wrap gap-3 items-center mb-4">
      <label class="sr-only" for="paste-visibility">Visibility</label>
      <select id="paste-visibility" bind:value={visibility}
        aria-label="Paste visibility"
        class="bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-3 py-2
          text-[var(--text-primary)] text-sm focus:border-[var(--accent)] focus:outline-none cursor-pointer">
        <option value="public">🌍 Public</option>
        <option value="unlisted">🔗 Unlisted</option>
        <option value="private">🔒 Private</option>
      </select>

      <label class="sr-only" for="paste-expiry">Expiration</label>
      <select id="paste-expiry" bind:value={expiresIn}
        aria-label="Paste expiration"
        class="bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-3 py-2
          text-[var(--text-primary)] text-sm focus:border-[var(--accent)] focus:outline-none cursor-pointer">
        <option value="">⏰ Never expires</option>
        <option value="1h">1 hour</option>
        <option value="24h">24 hours</option>
        <option value="7d">7 days</option>
        <option value="30d">30 days</option>
      </select>

      <div class="relative">
        <label class="sr-only" for="paste-password">Password</label>
        <input id="paste-password" bind:value={password} on:blur={validatePassword} type="password"
          placeholder="🔑 Password (optional)"
          aria-label="Password protection"
          aria-describedby={passwordError ? 'pw-error' : undefined}
          aria-invalid={passwordError ? 'true' : undefined}
          class="bg-[var(--bg-secondary)] border rounded-lg px-3 py-2
            text-[var(--text-primary)] placeholder-[var(--text-secondary)] text-sm
            focus:border-[var(--accent)] focus:outline-none transition-colors w-44
            {passwordError ? 'border-[var(--red)]' : 'border-[var(--border)]'}" />
        {#if passwordError}
          <span id="pw-error" class="absolute -bottom-4 left-0 text-xs text-[var(--red)]">{passwordError}</span>
        {/if}
      </div>

      <label class="flex items-center gap-1.5 text-sm text-[var(--text-secondary)] cursor-pointer">
        <input type="checkbox" bind:checked={burnAfterRead}
          aria-label="Burn after read"
          class="accent-[var(--accent)]" />
        🔥 Burn after read
      </label>
    </div>

    <!-- Tags -->
    <label class="sr-only" for="paste-tags">Tags</label>
    <input id="paste-tags" bind:value={tags} type="text"
      placeholder="Tags (comma separated, e.g. python, tutorial, snippet)"
      aria-label="Tags"
      class="w-full bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-4 py-2.5
        text-[var(--text-primary)] placeholder-[var(--text-secondary)] text-sm
        focus:border-[var(--accent)] focus:outline-none transition-colors mb-4" />

    <!-- Editor -->
    <label class="sr-only" for="paste-content">Paste content</label>
    <textarea id="paste-content" bind:value={content} on:keydown={handleKeydown}
      placeholder="Paste your code here... (Ctrl+Enter or Ctrl+S to submit)"
      rows="18" spellcheck="false"
      aria-label="Paste content"
      aria-required="true"
      class="w-full bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-4 py-3
        text-[var(--text-primary)] placeholder-[var(--text-secondary)] font-mono text-sm leading-relaxed
        focus:border-[var(--accent)] focus:outline-none transition-colors resize-y mb-4"
    ></textarea>

    <!-- Footer -->
    <div class="flex flex-col sm:flex-row justify-between items-start sm:items-center gap-3">
      <div class="flex flex-wrap gap-3 text-xs text-[var(--text-secondary)]" aria-live="polite">
        <span>{content.length.toLocaleString()} chars</span>
        <span aria-hidden="true">·</span>
        <span>{content.split('\n').length} lines</span>
        {#if content.length > 512 * 1024}
          <span class="text-[var(--red)]" role="alert">⚠ Exceeds 512KB limit</span>
        {/if}
      </div>
      <div class="flex gap-2">
        <span class="text-xs text-[var(--text-secondary)] self-center hidden sm:inline" aria-hidden="true">
          Ctrl+Shift+N: clear · Ctrl+Enter: submit
        </span>
        <button type="submit" disabled={submitting}
          class="px-5 py-2.5 bg-[var(--accent)] text-[var(--bg-primary)] font-semibold rounded-lg
            hover:bg-[var(--accent-hover)] transition-colors disabled:opacity-50 cursor-pointer border-none text-sm"
          aria-label={submitting ? 'Creating paste...' : 'Create paste'}>
          {submitting ? 'Creating...' : 'Create Paste'}
        </button>
      </div>
    </div>
  </form>
</section>

<style>
  .sr-only {
    position: absolute; width: 1px; height: 1px; padding: 0; margin: -1px;
    overflow: hidden; clip: rect(0,0,0,0); white-space: nowrap; border: 0;
  }
</style>
