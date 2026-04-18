<script>
  import { createPaste } from './lib/api.js';
  import { LANGUAGES } from './lib/languages.js';
  import PasteView from './PasteView.svelte';
  import PasteList from './PasteList.svelte';

  let currentView = 'create';
  let currentPasteId = null;

  let title = '';
  let content = '';
  let language = 'plaintext';
  let submitting = false;
  let error = '';

  function checkRoute() {
    const hash = window.location.hash;
    if (hash.startsWith('#/paste/')) {
      currentPasteId = hash.replace('#/paste/', '');
      currentView = 'view';
    } else if (hash === '#/list') {
      currentView = 'list';
    } else {
      currentView = 'create';
    }
  }

  checkRoute();
  window.addEventListener('hashchange', checkRoute);

  function navigate(view, id = null) {
    if (view === 'view' && id) {
      window.location.hash = `#/paste/${id}`;
    } else if (view === 'list') {
      window.location.hash = '#/list';
    } else {
      window.location.hash = '#/';
    }
  }

  async function handleSubmit() {
    if (!content.trim()) {
      error = 'Paste content cannot be empty';
      return;
    }
    error = '';
    submitting = true;
    try {
      const result = await createPaste({
        title: title || 'Untitled',
        content,
        language
      });
      title = '';
      content = '';
      language = 'plaintext';
      navigate('view', result.id);
    } catch (e) {
      error = e.message;
    } finally {
      submitting = false;
    }
  }

  function handleKeydown(e) {
    if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
      handleSubmit();
    }
  }
</script>

<div class="min-h-screen flex flex-col">
  <header class="border-b border-[var(--border)] px-6 py-4">
    <div class="max-w-5xl mx-auto flex items-center justify-between">
      <button
        class="flex items-center gap-2 text-xl font-bold text-[var(--accent)] hover:text-[var(--accent-hover)] transition-colors cursor-pointer bg-transparent border-none"
        on:click={() => navigate('create')}
      >
        <svg xmlns="http://www.w3.org/2000/svg" class="w-6 h-6" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">
          <path stroke-linecap="round" stroke-linejoin="round" d="M10 20l4-16m4 4l4 4-4 4M6 16l-4-4 4-4" />
        </svg>
        PasteBox
      </button>
      <nav class="flex gap-4">
        <button
          class="px-4 py-2 rounded-lg text-sm font-medium transition-colors cursor-pointer border-none
            {currentView === 'create'
              ? 'bg-[var(--accent)] text-[var(--bg-primary)]'
              : 'bg-[var(--bg-tertiary)] text-[var(--text-secondary)] hover:text-[var(--text-primary)]'}"
          on:click={() => navigate('create')}
        >
          + New Paste
        </button>
        <button
          class="px-4 py-2 rounded-lg text-sm font-medium transition-colors cursor-pointer border-none
            {currentView === 'list'
              ? 'bg-[var(--accent)] text-[var(--bg-primary)]'
              : 'bg-[var(--bg-tertiary)] text-[var(--text-secondary)] hover:text-[var(--text-primary)]'}"
          on:click={() => navigate('list')}
        >
          Recent
        </button>
      </nav>
    </div>
  </header>

  <main class="flex-1 max-w-5xl mx-auto w-full px-6 py-8">
    {#if currentView === 'create'}
      <div class="space-y-6">
        <h2 class="text-2xl font-semibold text-[var(--text-primary)]">Create New Paste</h2>

        {#if error}
          <div class="bg-red-900/30 border border-[var(--red)] text-[var(--red)] px-4 py-3 rounded-lg">
            {error}
          </div>
        {/if}

        <div class="flex gap-4">
          <input
            bind:value={title}
            type="text"
            placeholder="Title (optional)"
            class="flex-1 bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-4 py-3
              text-[var(--text-primary)] placeholder-[var(--text-secondary)] focus:border-[var(--accent)]
              focus:outline-none transition-colors"
          />
          <select
            bind:value={language}
            class="bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-4 py-3
              text-[var(--text-primary)] focus:border-[var(--accent)] focus:outline-none transition-colors
              cursor-pointer"
          >
            {#each LANGUAGES as lang}
              <option value={lang}>{lang}</option>
            {/each}
          </select>
        </div>

        <textarea
          bind:value={content}
          on:keydown={handleKeydown}
          placeholder="Paste your code here... (Ctrl+Enter to submit)"
          rows="20"
          class="w-full bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg px-4 py-3
            text-[var(--text-primary)] placeholder-[var(--text-secondary)] font-mono text-sm
            focus:border-[var(--accent)] focus:outline-none transition-colors resize-y"
          spellcheck="false"
        ></textarea>

        <div class="flex justify-between items-center">
          <span class="text-sm text-[var(--text-secondary)]">
            {content.length} characters · {content.split('\n').length} lines
          </span>
          <button
            on:click={handleSubmit}
            disabled={submitting}
            class="px-6 py-3 bg-[var(--accent)] text-[var(--bg-primary)] font-semibold rounded-lg
              hover:bg-[var(--accent-hover)] transition-colors disabled:opacity-50 cursor-pointer border-none"
          >
            {submitting ? 'Creating...' : 'Create Paste'}
          </button>
        </div>
      </div>

    {:else if currentView === 'view'}
      <PasteView pasteId={currentPasteId} on:navigate={(e) => navigate(e.detail)} />

    {:else if currentView === 'list'}
      <PasteList on:view={(e) => navigate('view', e.detail)} />
    {/if}
  </main>

  <footer class="border-t border-[var(--border)] px-6 py-4 text-center text-sm text-[var(--text-secondary)]">
    PasteBox · Built with Drogon/C++ & Svelte
  </footer>
</div>
