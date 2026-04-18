<script>
  import { themes, applyTheme, getStoredTheme } from './lib/themes.js';
  import { onMount } from 'svelte';

  let currentTheme = 'dark';
  let open = false;

  onMount(() => {
    currentTheme = getStoredTheme();
    applyTheme(currentTheme);
  });

  function selectTheme(name) {
    currentTheme = name;
    applyTheme(name);
    open = false;
  }
</script>

<div class="relative">
  <button
    on:click={() => open = !open}
    class="px-3 py-2 rounded-lg bg-[var(--bg-tertiary)] border border-[var(--border)]
      text-[var(--text-secondary)] hover:text-[var(--text-primary)] hover:border-[var(--accent)]
      transition-colors cursor-pointer text-sm"
    title="Change theme"
  >
    🎨 {themes[currentTheme]?.name || 'Dark'}
  </button>

  {#if open}
    <div class="absolute right-0 mt-2 w-40 bg-[var(--bg-secondary)] border border-[var(--border)]
      rounded-lg shadow-lg overflow-hidden z-50">
      {#each Object.entries(themes) as [key, theme]}
        <button
          on:click={() => selectTheme(key)}
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
