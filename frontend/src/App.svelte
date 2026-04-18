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
    <!-- Skip to main content link for keyboard users -->
    <a href="#main-content"
      class="sr-only focus:not-sr-only focus:absolute focus:top-2 focus:left-2 focus:z-50
        focus:px-4 focus:py-2 focus:bg-[var(--accent)] focus:text-[var(--bg-primary)] focus:rounded-lg">
      Skip to main content
    </a>

    <!-- Header -->
    <header class="border-b border-[var(--border)] bg-[var(--bg-secondary)]" role="banner">
      <div class="max-w-5xl mx-auto flex items-center justify-between px-6 py-3.5">
        <button on:click={() => navigate('create')}
          class="flex items-center gap-2.5 bg-transparent border-none cursor-pointer p-0"
          aria-label="PasteBox — go to home">
          <span class="text-2xl" aria-hidden="true">📋</span>
          <span class="text-xl font-bold text-[var(--text-primary)]">PasteBox</span>
        </button>
        <nav class="flex items-center gap-2" aria-label="Main navigation">
          <button on:click={() => navigate('create')}
            aria-current={currentView === 'create' ? 'page' : undefined}
            class="px-3 py-1.5 rounded-lg text-sm font-medium border-none cursor-pointer transition-colors
              {currentView === 'create' ? 'bg-[var(--accent)] text-[var(--bg-primary)]' : 'bg-transparent text-[var(--text-secondary)] hover:text-[var(--text-primary)]'}">
            + New
          </button>
          <button on:click={() => navigate('list')}
            aria-current={currentView === 'list' ? 'page' : undefined}
            class="px-3 py-1.5 rounded-lg text-sm font-medium border-none cursor-pointer transition-colors
              {currentView === 'list' ? 'bg-[var(--accent)] text-[var(--bg-primary)]' : 'bg-transparent text-[var(--text-secondary)] hover:text-[var(--text-primary)]'}">
            Browse
          </button>
          <div class="w-px h-5 bg-[var(--border)] mx-1" aria-hidden="true"></div>
          <ThemeSwitcher />
        </nav>
      </div>
    </header>

    <!-- Main -->
    <main id="main-content" class="max-w-5xl mx-auto px-6 py-6" role="main">
      {#if currentView === 'create'}
        <CreatePaste on:navigate={handleNav} />
      {:else if currentView === 'view'}
        <PasteView pasteId={currentId} on:navigate={handleNav} />
      {:else if currentView === 'list'}
        <PasteList on:view={(e) => navigate('view', e.detail)} />
      {/if}
    </main>

    <!-- Footer -->
    <footer class="border-t border-[var(--border)] py-4 mt-12" role="contentinfo">
      <div class="max-w-5xl mx-auto px-6 flex items-center justify-between text-xs text-[var(--text-secondary)]">
        <span>PasteBox — code sharing made simple</span>
        <div class="flex gap-4">
          <span title="Press N for new, L for list" aria-label="Keyboard shortcuts: N for new paste, L for browse list">⌨ Shortcuts</span>
        </div>
      </div>
    </footer>
  </div>

  <Toast />
{/if}

<style>
  .sr-only {
    position: absolute; width: 1px; height: 1px; padding: 0; margin: -1px;
    overflow: hidden; clip: rect(0,0,0,0); white-space: nowrap; border: 0;
  }
</style>
