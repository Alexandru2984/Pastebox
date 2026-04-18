<script>
  import { createEventDispatcher, onMount } from 'svelte';
  import { listPastes } from './lib/api.js';

  const dispatch = createEventDispatcher();

  let pastes = [];
  let loading = true;
  let error = '';

  onMount(async () => {
    try {
      pastes = await listPastes();
    } catch (e) {
      error = 'Failed to load pastes';
    } finally {
      loading = false;
    }
  });

  function formatDate(dateStr) {
    const d = new Date(dateStr + 'Z');
    const now = new Date();
    const diff = Math.floor((now - d) / 1000);
    if (diff < 60) return 'just now';
    if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
    if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
    return `${Math.floor(diff / 86400)}d ago`;
  }

  function formatSize(bytes) {
    if (bytes < 1024) return `${bytes} B`;
    return `${(bytes / 1024).toFixed(1)} KB`;
  }

  const langColors = {
    javascript: '#f1e05a',
    typescript: '#3178c6',
    python: '#3572a5',
    rust: '#dea584',
    go: '#00add8',
    c: '#555555',
    cpp: '#f34b7d',
    java: '#b07219',
    ruby: '#701516',
    php: '#4f5d95',
    html: '#e34c26',
    css: '#563d7c',
    sql: '#e38c00',
    bash: '#89e051',
    json: '#292929',
    plaintext: '#8b949e'
  };
</script>

<div class="space-y-6">
  <h2 class="text-2xl font-semibold text-[var(--text-primary)]">Recent Pastes</h2>

  {#if loading}
    <div class="flex justify-center py-12">
      <div class="text-[var(--text-secondary)]">Loading...</div>
    </div>
  {:else if error}
    <div class="text-[var(--red)] py-8 text-center">{error}</div>
  {:else if pastes.length === 0}
    <div class="flex flex-col items-center py-16 gap-4 text-[var(--text-secondary)]">
      <div class="text-5xl">📝</div>
      <p>No pastes yet. Create the first one!</p>
    </div>
  {:else}
    <div class="space-y-2">
      {#each pastes as paste}
        <button
          on:click={() => dispatch('view', paste.id)}
          class="w-full text-left bg-[var(--bg-secondary)] border border-[var(--border)] rounded-lg p-4
            hover:border-[var(--accent)] transition-colors cursor-pointer group"
        >
          <div class="flex items-center justify-between">
            <div class="flex items-center gap-3">
              <span
                class="w-3 h-3 rounded-full inline-block"
                style="background-color: {langColors[paste.language] || '#8b949e'}"
              ></span>
              <span class="font-medium text-[var(--text-primary)] group-hover:text-[var(--accent)] transition-colors">
                {paste.title}
              </span>
              <span class="text-xs font-mono px-2 py-0.5 rounded bg-[var(--bg-tertiary)] text-[var(--text-secondary)]">
                {paste.language}
              </span>
            </div>
            <div class="flex items-center gap-4 text-xs text-[var(--text-secondary)]">
              <span>{formatSize(paste.size)}</span>
              <span>{formatDate(paste.created_at)}</span>
            </div>
          </div>
        </button>
      {/each}
    </div>
  {/if}
</div>
