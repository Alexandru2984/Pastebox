<script>
  import { onMount } from 'svelte';
  import CreatePaste from './CreatePaste.svelte';
  import PasteView from './PasteView.svelte';
  import PasteList from './PasteList.svelte';
  import Library from './Library.svelte';
  import EmbedView from './EmbedView.svelte';
  import ThemeSwitcher from './ThemeSwitcher.svelte';
  import Toast from './Toast.svelte';
  import Icon from './Icon.svelte';
  import { trackPublicPage } from './lib/analytics.js';
  import { initTheme } from './lib/themes.js';
  let currentView = 'create';
  let currentId = '';
  let shortcuts;

  function parseHash() {
    const hash = window.location.pathname.startsWith('/embed/') ? '#/embed/' + window.location.pathname.slice(7) : window.location.hash;
    const match = hash.match(/^#\/(paste|embed)\/([a-zA-Z0-9]{1,64})$/);
    if (match) { currentView = match[1] === 'paste' ? 'view' : 'embed'; currentId = match[2]; }
    else { currentView = hash === '#/list' ? 'list' : hash === '#/library' ? 'library' : 'create'; currentId = ''; }
    document.title = ({ create: 'PasteBox — A home for your next snippet', list: 'Explore snippets · PasteBox', library: 'Your library · PasteBox', view: 'View paste · PasteBox', embed: 'Embedded paste · PasteBox' })[currentView];
    const robots = document.querySelector('meta[name="robots"]');
    if (robots) robots.content = currentView === 'create' || currentView === 'list' ? 'index, follow' : 'noindex, nofollow, noarchive';
    trackPublicPage();
    window.scrollTo?.({ top: 0, behavior: 'instant' });
  }
  function navigate(view, id = '') {
    window.location.hash = view === 'view' ? `#/paste/${id}` : view === 'list' ? '#/list' : view === 'library' ? '#/library' : '#/';
  }
  function handleNav(event) { navigate(event.detail.view, event.detail.id); }
  function handleKeydown(event) {
    if (event.target.closest('input, textarea, select, [contenteditable="true"], dialog') || event.altKey || event.ctrlKey || event.metaKey) return;
    if (event.key === '?') { event.preventDefault(); shortcuts.showModal(); }
    const view = { n:'create', l:'list', b:'library' }[event.key.toLowerCase()];
    if (view) { event.preventDefault(); navigate(view); }
  }
  onMount(() => {
    initTheme(); parseHash();
    window.addEventListener('hashchange', parseHash);
    window.addEventListener('keydown', handleKeydown);
    return () => { window.removeEventListener('hashchange', parseHash); window.removeEventListener('keydown', handleKeydown); };
  });
</script>

{#if currentView === 'embed'}
  {#key currentId}<EmbedView pasteId={currentId} />{/key}
{:else}
  <div class="app-shell">
    <a class="skip-link" href="#main-content" on:click|preventDefault={() => document.getElementById('main-content').focus()}>Skip to main content</a>
    <header class="site-header">
      <div class="header-inner">
        <a href="#/" class="brand" aria-label="PasteBox home"><span class="brand-mark"><Icon name="code" size={23}/></span><span>pastebox<span class="brand-dot">.</span></span></a>
        <nav class="main-nav" aria-label="Main navigation">
          <a href="#/" aria-current={currentView === 'create' ? 'page' : undefined}><Icon name="plus" size={16}/> New paste</a>
          <a href="#/list" aria-current={currentView === 'list' ? 'page' : undefined}><Icon name="globe" size={16}/> Explore</a>
          <a href="#/library" aria-current={currentView === 'library' ? 'page' : undefined}><Icon name="library" size={16}/> Library</a>
        </nav>
        <div class="header-tools"><ThemeSwitcher /></div>
      </div>
    </header>
    <main id="main-content" class="main-container" tabindex="-1">
      {#if currentView === 'create'}<CreatePaste on:navigate={handleNav}/>
      {:else if currentView === 'view'}{#key currentId}<PasteView pasteId={currentId} on:navigate={handleNav}/>{/key}
      {:else if currentView === 'list'}<PasteList on:view={(event) => navigate('view', event.detail)}/>
      {:else if currentView === 'library'}<Library on:view={(event) => navigate('view', event.detail)}/>{/if}
    </main>
    <footer class="site-footer"><div><span class="footer-logo">pastebox.</span><span>A little less friction. A lot more sharing.</span></div><button class="text-button" on:click={() => shortcuts.showModal()}>Keyboard shortcuts <kbd>?</kbd></button></footer>
  </div>
  <dialog bind:this={shortcuts} class="shortcut-dialog" aria-labelledby="shortcuts-title">
    <h2 id="shortcuts-title">Make yourself at home.</h2><p class="muted">A few shortcuts to keep you in flow.</p>
    <dl class="shortcut-list"><div><dt>New paste</dt><dd><kbd>N</kbd></dd></div><div><dt>Explore</dt><dd><kbd>L</kbd></dd></div><div><dt>Your library</dt><dd><kbd>B</kbd></dd></div><div><dt>Create / save in editor</dt><dd><kbd>Ctrl / ⌘</kbd> + <kbd>Enter</kbd></dd></div></dl>
    <form method="dialog"><button class="button primary">Got it</button></form>
  </dialog>
  <Toast/>
{/if}
