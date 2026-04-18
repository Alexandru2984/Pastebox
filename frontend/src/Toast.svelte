<script>
  import { toasts, removeToast } from './lib/toast.js';
</script>

<div class="fixed top-4 right-4 z-50 flex flex-col gap-2 pointer-events-none" style="max-width: 400px;">
  {#each $toasts as toast (toast.id)}
    <div
      class="pointer-events-auto px-4 py-3 rounded-lg shadow-lg flex items-center justify-between gap-3 animate-slide-in
        {toast.type === 'success' ? 'bg-green-900/90 border border-[var(--green)] text-[var(--green)]' :
         toast.type === 'error' ? 'bg-red-900/90 border border-[var(--red)] text-[var(--red)]' :
         'bg-[var(--bg-tertiary)] border border-[var(--border)] text-[var(--text-primary)]'}"
    >
      <span class="text-sm">
        {#if toast.type === 'success'}✓{:else if toast.type === 'error'}✕{:else}ℹ{/if}
        {toast.message}
      </span>
      <button
        on:click={() => removeToast(toast.id)}
        class="text-[var(--text-secondary)] hover:text-[var(--text-primary)] bg-transparent border-none cursor-pointer text-lg"
      >×</button>
    </div>
  {/each}
</div>

<style>
  @keyframes slide-in {
    from { transform: translateX(100%); opacity: 0; }
    to { transform: translateX(0); opacity: 1; }
  }
  .animate-slide-in { animation: slide-in 0.3s ease-out; }
</style>
