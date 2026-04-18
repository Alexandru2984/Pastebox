<script>
  import { themes, applyTheme, getStoredTheme } from './lib/themes.js';
  import { onMount } from 'svelte';

  let currentTheme = 'dark';
  let open = false;
  let dropdownEl;

  onMount(() => {
    currentTheme = getStoredTheme();
    applyTheme(currentTheme);
  });

  function selectTheme(name) {
    currentTheme = name;
    applyTheme(name);
    open = false;
  }

  function handleKeydown(e) {
    if (!open) return;
    const entries = Object.entries(themes);
    const currentIdx = entries.findIndex(([k]) => k === currentTheme);
    if (e.key === 'Escape') { open = false; e.preventDefault(); }
    else if (e.key === 'ArrowDown') {
      e.preventDefault();
      const next = (currentIdx + 1) % entries.length;
      selectTheme(entries[next][0]);
      open = true;
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      const prev = (currentIdx - 1 + entries.length) % entries.length;
      selectTheme(entries[prev][0]);
      open = true;
    }
  }
</script>

<div class="relative" on:keydown={handleKeydown}>
  <button
    on:click={() => open = !open}
    class="px-3 py-2 rounded-lg bg-[var(--bg-tertiary)] border border-[var(--border)]
      text-[var(--text-secondary)] hover:text-[var(--text-primary)] hover:border-[var(--accent)]
      transition-colors cursor-pointer text-sm"
    title="Change theme"
    aria-label="Change theme: {themes[currentTheme]?.name || 'Dark'}"
    aria-expanded={open}
    aria-haspopup="listbox"
  >
    <span aria-hidden="true">🎨</span> {themes[currentTheme]?.name || 'Dark'}
  </button>

  {#if open}
    <div bind:this={dropdownEl}
      class="absolute right-0 mt-2 w-40 bg-[var(--bg-secondary)] border border-[var(--border)]
        rounded-lg shadow-lg overflow-hidden z-50"
      role="listbox" aria-label="Theme options">
      {#each Object.entries(themes) as [key, theme]}
        <button
          on:click={() => selectTheme(key)}
          role="option"
          aria-selected={key === currentTheme}
          class="w-full text-left px-4 py-2 text-sm transition-colors cursor-pointer border-none
            {key === currentTheme
              ? 'bg-[var(--accent)] text-[var(--bg-primary)] font-medium'
              : 'bg-transparent text-[var(--text-primary)] hover:bg-[var(--bg-tertiary)]'}"
        >
          {theme.name}
        </button>
      {/each}
    </div>
  {/if}
</div>

<svelte:window on:click={(e) => { if (open && !e.target.closest('.relative')) open = false; }} />
