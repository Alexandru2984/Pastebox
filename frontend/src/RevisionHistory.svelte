<script>
  import { createEventDispatcher, onMount, onDestroy } from 'svelte';
  import { listRevisions, getRevision, restoreRevision, clearRevisions } from './lib/api.js';
  import { parseDate, formatSize } from './lib/format.js';
  import { success } from './lib/toast.js';
  import CodeBlock from './CodeBlock.svelte';
  import DiffView from './DiffView.svelte';
  import Icon from './Icon.svelte';
  export let paste;
  const dispatch = createEventDispatcher();
  let items = [], loading = true, error = '', selected = null, selectedNumber = null, selecting = false, comparing = true;
  let restoring = false, clearing = false, clearDialog, clearError = '', currentRevision = paste.revision, requestNumber = 0;
  onMount(loadHistory);
  onDestroy(() => requestNumber++);
  async function loadHistory() {
    const request = ++requestNumber; loading = true; error = ''; selecting = false; selected = null; selectedNumber = null;
    try {
      const result = await listRevisions(paste.id);
      if (request !== requestNumber) return;
      items = result.data; currentRevision = result.current_revision;
    } catch (e) {if (request === requestNumber) error = e.message;}
    finally {if (request === requestNumber) loading = false;}
  }
  async function selectRevision(number) {
    const request = ++requestNumber; selectedNumber = number; selecting = true; error = ''; selected = null;
    try {const result = await getRevision(paste.id,number); if (request === requestNumber) selected = result;}
    catch (e) {if (request === requestNumber) error = e.message;}
    finally {if (request === requestNumber) selecting = false;}
  }
  async function restore() {
    if (!selected || restoring || currentRevision !== paste.revision) return;
    restoring = true; error = '';
    try {await restoreRevision(paste.id,selected.revision,paste.revision); success('Version restored as a new revision. Privacy settings were kept.'); dispatch('restored');}
    catch (e) {error = e.status === 412 ? 'A newer version was saved. Reload the paste before restoring.' : e.message;}
    finally {restoring = false;}
  }
  async function clear() {
    if (clearing) return;
    clearing = true; clearError = '';
    try {await clearRevisions(paste.id,paste.revision); items = []; selected = null; selectedNumber = null; clearDialog.close(); success('Saved history cleared. The current paste is unchanged.');}
    catch (e) {clearError = e.message;}
    finally {clearing = false;}
  }
</script>
<section class="panel revision-panel" aria-label="Version history">
  <div class="panel-heading"><span class="panel-label"><Icon name="clock" size={16}/> Version history <span class="pill">Current v{paste.revision}</span></span><button class="text-button" on:click={() => dispatch('close')}>Close history</button></div>
  <div class="revision-body">
    <p class="field-hint">Only your owner key can read these snapshots. Up to 10 earlier versions are kept. Restoring copies content, title, language and tags; your current access settings stay in place.</p>
    {#if loading}<p role="status">Loading saved versions…</p>{:else if error}<div class="notice error" role="alert">{error}<button class="text-button" on:click={loadHistory}>Retry history</button></div>{/if}
    {#if currentRevision !== paste.revision}<div class="notice warning">A newer version exists. Reload before restoring or clearing history. <button class="text-button" on:click={() => dispatch('reload')}>Reload current paste</button></div>{/if}
    {#if !loading && !items.length && !error}<p class="muted">No earlier versions yet. Your next save will keep a snapshot here.</p>{/if}
    {#if items.length}<div class="revision-layout"><ol class="revision-list" aria-label="Saved versions">{#each items as item (item.revision)}<li><button class="revision-choice" class:active={selectedNumber === item.revision} on:click={() => selectRevision(item.revision)} disabled={restoring || clearing} aria-pressed={selectedNumber === item.revision} aria-label="View version {item.revision}"><strong>v{item.revision} · {item.title || 'Untitled'}</strong><span>{parseDate(item.saved_at).toLocaleString()}</span><small>{item.language} · {formatSize(item.size)}</small></button></li>{/each}</ol>
      <div class="revision-content">
        {#if selecting}<p role="status">Loading version…</p>{:else if selected}<div class="revision-actions"><div class="tab-bar"><button aria-pressed={comparing} on:click={() => comparing = true}>Compare with current</button><button aria-pressed={!comparing} on:click={() => comparing = false}>View saved content</button></div><button class="button small" disabled={restoring || clearing || currentRevision !== paste.revision} on:click={restore}>Restore v{selected.revision}</button></div><h3>{selected.title || 'Untitled'}</h3>{#if selected.tags?.length}<div class="tag-list">{#each selected.tags as tag}<span class="tag">#{tag}</span>{/each}</div>{/if}
          {#if comparing}<DiffView before={selected.content} after={paste.content} beforeLanguage={selected.language} afterLanguage={paste.language}/>{:else}<CodeBlock content={selected.content} language={selected.language}/>{/if}
        {:else}<p class="muted">Choose an earlier version to read or compare it.</p>{/if}
      </div></div><button class="text-button error-text" on:click={() => {clearError = '';clearDialog.showModal();}} disabled={restoring || clearing || currentRevision !== paste.revision}>Clear saved history</button>{/if}
  </div>
  <dialog bind:this={clearDialog} aria-labelledby="clear-history-title"><h2 id="clear-history-title">Clear saved versions?</h2><p class="muted">This removes earlier versions from the active application. Your current content stays. Backups may still contain previous snapshots.</p>{#if clearError}<p class="error-text" role="alert">{clearError}</p>{/if}<div class="form-row"><button class="button" on:click={() => clearDialog.close()} disabled={clearing}>Keep history</button><button class="button danger" on:click={clear} disabled={clearing}>{clearing ? 'Clearing…' : 'Clear history permanently'}</button></div></dialog>
</section>
