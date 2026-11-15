<script>
  import { diffLines } from './lib/diff.js';
  import CodeBlock from './CodeBlock.svelte';
  export let before = '';
  export let after = '';
  export let beforeLanguage = 'plaintext';
  export let afterLanguage = 'plaintext';
  $: comparison = diffLines(before,after);
  $: added = comparison.rows.filter(row => row.type === 'added').length;
  $: removed = comparison.rows.filter(row => row.type === 'removed').length;
</script>
<div class="revision-diff" aria-label="Revision comparison">
  {#if comparison.mode === 'large'}<p class="field-hint">This comparison is large. Both versions are shown separately to keep scrolling responsive.</p><h3>Selected version</h3><CodeBlock content={before} language={beforeLanguage}/><h3>Current version</h3><CodeBlock content={after} language={afterLanguage}/>
  {:else}<div class="diff-legend"><span>Selected version → current version</span><span class="diff-count-added">+{added}</span><span class="diff-count-removed">−{removed}</span></div>
    {#if !added && !removed}<p class="field-hint">The content is identical. The title, language or tags may differ.</p>{/if}
    <div class="diff-scroll" role="textbox" aria-readonly="true" aria-multiline="true" aria-label="Line changes" tabindex="0">{#each comparison.rows as row}<div class="diff-row" class:diff-added={row.type === 'added'} class:diff-removed={row.type === 'removed'}><span class="diff-number" aria-hidden="true">{row.left || ''}</span><span class="diff-number" aria-hidden="true">{row.right || ''}</span><span class="diff-sign" aria-hidden="true">{row.type === 'added' ? '+' : row.type === 'removed' ? '−' : ' '}</span><span class="sr-only">{row.type === 'added' ? 'Added: ' : row.type === 'removed' ? 'Removed: ' : ''}</span><code>{row.text || ' '}</code></div>{/each}</div>
  {/if}
</div>
