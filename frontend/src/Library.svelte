<script>
  import { createEventDispatcher } from 'svelte';
  import { getLibrary, forgetPaste } from './lib/library.js';
  import { restoreOwnerKey } from './lib/api.js';
  import { copyText, timeAgo } from './lib/format.js';
  import { success, error as toastError } from './lib/toast.js';
  import Icon from './Icon.svelte';
  const dispatch = createEventDispatcher();
  let items = getLibrary(), tab = 'all', query = '', importId = '', importKey = '', importing = false;
  let removeId = '', removeDialog;
  $: filtered = items.filter(item => (tab === 'all' || (tab === 'owned' && item.owner_token) || (tab === 'favorites' && item.favorite)) && (item.title || '').toLowerCase().includes(query.toLowerCase()));
  function askRemove(id) { removeId = id; removeDialog.showModal(); }
  function remove() { forgetPaste(removeId); items = getLibrary(); removeDialog.close(); success('Removed from this browser’s library.'); }
  async function copyKey(item) {
    try { await copyText(item.owner_token); success('Owner key copied. Keep it somewhere private.'); } catch (e) { toastError(e.message); }
  }
  async function restore() {
    if (importing) return;
    importing = true;
    try {
      let id = importId.trim();
      if (id.includes('://')) {
        const url = new URL(id);
        if (url.origin !== location.origin) throw new Error('Use a PasteBox link from this site.');
        id = url.hash.match(/^#\/paste\/([a-zA-Z0-9]{1,64})$/)?.[1] || '';
      }
      const result = await restoreOwnerKey(id, importKey.trim());
      items = getLibrary(); importId = ''; importKey = '';
      if (!result.owner_saved) toastError('Key verified, but browser storage is unavailable. Access is limited to this session.');
      else success('Owner key verified. Welcome back.');
    } catch (e) { toastError(e.message); }
    finally { importing = false; }
  }
</script>
<section aria-label="Your library">
  <div class="page-heading"><div><div class="eyebrow"><Icon name="library" size={13}/> YOUR LITTLE CORNER</div><h1>Keep the good stuff.</h1><p class="subheading">Your creations and saved snippets, right where you left them.</p></div><a class="button primary" href="#/"><Icon name="plus" size={15}/> New paste</a></div>
  <details class="panel import-panel"><summary>Have an owner key? Restore access to a paste.</summary><form on:submit|preventDefault={restore}><label class="field"><span>Paste link or ID</span><input bind:value={importId} required placeholder="Paste a link or an ID" maxlength="300"/></label><label class="field"><span>Owner key</span><input type="password" bind:value={importKey} required minlength="64" maxlength="64" autocomplete="off" spellcheck="false" placeholder="Your 64-character key"/><small>A read password is different from an owner key. The server verifies ownership without opening one-time content.</small></label><button class="button" disabled={importing}>{importing ? 'Verifying…' : 'Restore access'}</button></form></details>
  <div class="filter-bar"><div class="tab-bar" aria-label="Library filters">{#each [['all','All snippets'],['owned','Created by you'],['favorites','Favorites']] as [key,label]}<button on:click={() => tab = key} aria-pressed={tab === key}>{label}</button>{/each}</div><label class="search-field"><span class="sr-only">Search your library</span><Icon name="search" size={16}/><input type="search" bind:value={query} placeholder="Find a saved snippet…"/></label></div>
  {#if filtered.length === 0}<div class="empty-state"><Icon name="library" size={32}/><h2>A home for your snippets.</h2><p>Create a paste to add it here automatically, or favorite a snippet while reading. No account needed.</p><a class="button" href="#/">Create your first paste <Icon name="arrow" size={14}/></a></div>
  {:else}<ul class="paste-grid" aria-label="Saved snippets">{#each filtered as item (item.id)}<li><article class="paste-card"><div class="paste-card-head"><span class="language-label"><span class="language-dot"></span>{item.language}</span><span class="pill">{item.owner_token ? 'Yours' : 'Saved'}{item.favorite ? ' · ★' : ''}</span></div><h2 class="card-title"><button on:click={() => dispatch('view',item.id)}>{item.title || 'Untitled'}</button></h2><div class="card-details"><span>{item.visibility}</span><span>Saved {timeAgo(item.saved_at)}</span></div><div class="tag-list">{#if item.owner_token}<button class="button small" on:click={() => copyKey(item)}><Icon name="key" size={13}/> Copy owner key</button>{/if}<button class="text-button" on:click={() => askRemove(item.id)} aria-label="Remove {item.title} from library">Remove</button></div></article></li>{/each}</ul>{/if}
  <p class="library-note"><Icon name="lock" size={17}/><span>This library lives in this browser. Clearing browser data removes saved owner keys. Back up a paste’s key to manage it on another device. Read passwords and paste contents are never saved in the library.</span></p>
  <dialog bind:this={removeDialog} aria-labelledby="remove-title"><h2 id="remove-title">Remove from this browser?</h2><p class="muted">The server copy stays online. Save your owner key first if you want to manage the paste later.</p><div class="form-row"><button class="button" on:click={() => removeDialog.close()}>Cancel</button><button class="button danger" on:click={remove}>Remove from library</button></div></dialog>
</section>
