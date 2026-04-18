<script>
  import { onMount, onDestroy } from 'svelte';
  import CreatePaste from './CreatePaste.svelte';
  import PasteView from './PasteView.svelte';
  import PasteList from './PasteList.svelte';
  import EmbedView from './EmbedView.svelte';
  import ThemeSwitcher from './ThemeSwitcher.svelte';
  import Toast from './Toast.svelte';
  import { initTheme } from './lib/themes.js';

  let currentView = 'create';
  let currentId = '';

  function parseHash() {
    const hash = window.location.hash || '#/';
    if (hash.startsWith('#/paste/')) {
      currentView = 'view';
      currentId = hash.replace('#/paste/', '');
    } else if (hash.startsWith('#/embed/')) {
      currentView = 'embed';
      currentId = hash.replace('#/embed/', '');
    } else if (hash === '#/list') {
      currentView = 'list';
      currentId = '';
    } else {
      currentView = 'create';
      currentId = '';
    }
  }

  function navigate(view, id = '') {
    if (view === 'view' && id) {
      window.location.hash = `#/paste/${id}`;
    } else if (view === 'embed' && id) {
      window.location.hash = `#/embed/${id}`;
    } else if (view === 'list') {
      window.location.hash = '#/list';
    } else {
      window.location.hash = '#/';
    }
  }

  function handleNav(e) {
    const { view, id } = e.detail;
    navigate(view, id);
  }

  function handleKeydown(e) {
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA' || e.target.tagName === 'SELECT') return;
    if (e.key === 'n' && !e.ctrlKey && !e.metaKey) {
      e.preventDefault();
      navigate('create');
    } else if (e.key === 'l' && !e.ctrlKey && !e.metaKey) {
      e.preventDefault();
      navigate('list');
    }
  }

  onMount(() => {
    initTheme();
    parseHash();
    window.addEventListener('hashchange', parseHash);
    window.addEventListener('keydown', handleKeydown);
  });

  onDestroy(() => {
    if (typeof window !== 'undefined') {
      window.removeEventListener('hashchange', parseHash);
      window.removeEventListener('keydown', handleKeydown);
    }
  });
</script>

{#if currentView === 'embed'}
  <EmbedView pasteId={currentId} />
{:else}
  <div class="min-h-screen bg-[var(--bg-primary)] text-[var(--text-primary)]">
    <!-- Header -->
    <header class="border-b border-[var(--border)] bg-[var(--bg-secondary)]">
      <div class="max-w-5xl mx-auto flex items-center justify-between px-6 py-3.5">
        <button on:click={() => navigate('create')}
          class="flex items-center gap-2.5 bg-transparent border-none cursor-pointer p-0">
          <span class="text-2xl">📋</span>
          <span class="text-xl font-bold text-[var(--text-primary)]">PasteBox</span>
        </button>
        <nav class="flex items-center gap-2">
          <button on:click={() => navigate('create')}
            class="px-3 py-1.5 rounded-lg text-sm font-medium border-none cursor-pointer transition-colors
              {currentView === 'create' ? 'bg-[var(--accent)] text-[var(--bg-primary)]' : 'bg-transparent text-[var(--text-secondary)] hover:text-[var(--text-primary)]'}">
            + New
          </button>
          <button on:click={() => navigate('list')}
            class="px-3 py-1.5 rounded-lg text-sm font-medium border-none cursor-pointer transition-colors
              {currentView === 'list' ? 'bg-[var(--accent)] text-[var(--bg-primary)]' : 'bg-transparent text-[var(--text-secondary)] hover:text-[var(--text-primary)]'}">
            Browse
          </button>
          <div class="w-px h-5 bg-[var(--border)] mx-1"></div>
          <ThemeSwitcher />
        </nav>
      </div>
    </header>

    <!-- Main -->
    <main class="max-w-5xl mx-auto px-6 py-6">
      {#if currentView === 'create'}
        <CreatePaste on:navigate={handleNav} />
      {:else if currentView === 'view'}
        <PasteView pasteId={currentId} on:navigate={handleNav} />
      {:else if currentView === 'list'}
        <PasteList on:view={(e) => navigate('view', e.detail)} />
      {/if}
    </main>

    <!-- Footer -->
    <footer class="border-t border-[var(--border)] py-4 mt-12">
      <div class="max-w-5xl mx-auto px-6 flex items-center justify-between text-xs text-[var(--text-secondary)]">
        <span>PasteBox — code sharing made simple</span>
        <div class="flex gap-4">
          <span title="Press N for new, L for list">⌨ Shortcuts</span>
        </div>
      </div>
    </footer>
  </div>

  <Toast />
{/if}
