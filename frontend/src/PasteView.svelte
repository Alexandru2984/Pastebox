<script>
  import { createEventDispatcher, onDestroy, tick } from 'svelte';
  import { getPaste, forkPaste, updatePaste, deletePaste, getRawUrl, restoreOwnerKey } from './lib/api.js';
  import { ownerToken, isFavorite, toggleFavorite, rememberPaste } from './lib/library.js';
  import { byteLength, formatSize, parseDate, copyText, safeFilename } from './lib/format.js';
  import { LANGUAGES } from './lib/languages.js';
  import { validatePaste } from './lib/editor.js';
  import { success, error as toastError } from './lib/toast.js';
  import CodeBlock from './CodeBlock.svelte';
  import Icon from './Icon.svelte';
  export let pasteId;
  const dispatch = createEventDispatcher();
  let paste = null, loading = true, error = '', passwordRequired = false, ownerRequired = false, burnRequired = false;
  let passwordInput = '', currentPassword = '', ownerInput = '', passwordInputEl, copied = false, copyTimer;
  let editing = false, editTitle = '', editContent = '', editLanguage = '', editVisibility = '', editTags = '', saving = false, forking = false, deleting = false;
  let favorite = false, wrap = false, showLines = true, fontSize = 13, deleteDialog, requestNumber = 0;
  const urls = new Set();
  $: if (pasteId) loadPaste();
  onDestroy(() => { requestNumber++; clearTimeout(copyTimer); for (const url of urls) URL.revokeObjectURL(url); });
  async function loadPaste(pw = '', confirmBurn = false) {
    const request = ++requestNumber;
    loading = true; error = ''; passwordRequired = ownerRequired = burnRequired = false;
    try {
      const result = await getPaste(pasteId,pw || currentPassword,confirmBurn);
      if (request !== requestNumber) return;
      paste = result; currentPassword = pw || currentPassword; favorite = isFavorite(pasteId);
      document.title = `${paste.title || 'Untitled'} · PasteBox`;
      if (paste.can_manage) rememberPaste({...paste,owner_token:ownerToken(pasteId)});
    } catch (e) {
      if (request !== requestNumber) return;
      passwordRequired = Boolean(e.passwordRequired); ownerRequired = Boolean(e.ownerRequired); burnRequired = Boolean(e.burnRequired);
      if (burnRequired) currentPassword = pw || currentPassword;
      error = e.message || 'Paste not found';
    } finally {
      if (request === requestNumber) {
        loading = false;
        if (passwordRequired) { await tick(); passwordInputEl?.focus(); }
      }
    }
  }
  async function copyContent() {
    try { await copyText(paste.content); copied = true; success('Copied to clipboard.'); clearTimeout(copyTimer); copyTimer = setTimeout(() => copied = false,2000); } catch (e) { toastError(e.message); }
  }
  async function share() {
    try { await copyText(`${window.location.origin}/#/paste/${paste.id}`); success('Share link copied. Your owner key stays private.'); } catch (e) { toastError(e.message); }
  }
  function contentUrl() {
    const url = URL.createObjectURL(new Blob([paste.content],{type:'text/plain;charset=utf-8'})); urls.add(url); return url;
  }
  function openRaw() { window.open(contentUrl(),'_blank','noopener,noreferrer'); }
  function download() {
    const a = document.createElement('a'); a.href = contentUrl(); a.download = safeFilename(paste.title,paste.language); a.click();
  }
  function saveFavorite() { favorite = toggleFavorite(paste); success(favorite ? 'Saved to your library.' : 'Removed from favorites.'); }
  async function copyOwnerKey() {
    try { await copyText(ownerToken(pasteId)); success('Owner key copied. Store it privately to recover access later.'); } catch (e) { toastError(e.message); }
  }
  async function restoreOwner() {
    try { await restoreOwnerKey(pasteId,ownerInput.trim()); ownerInput = ''; await loadPaste(); } catch (e) { toastError(e.message); }
  }
  async function handleFork() {
    if (forking) return;
    forking = true;
    try { const result = await forkPaste(pasteId,{},currentPassword); success('Fork created with the original privacy and expiration.'); dispatch('navigate',{view:'view',id:result.id}); } catch (e) { toastError(e.message); } finally { forking = false; }
  }
  function startEdit() {
    editTitle = paste.title; editContent = paste.content; editLanguage = paste.language; editVisibility = paste.visibility; editTags = (paste.tags || []).join(', '); editing = true;
  }
  async function saveEdit() {
    if (saving) return;
    const invalid = validatePaste({content:editContent,title:editTitle,tags:editTags});
    if (invalid) { toastError(invalid); return; }
    saving = true;
    try {
      await updatePaste(pasteId,{title:editTitle,content:editContent,language:editLanguage,visibility:editVisibility,tags:[...new Set(editTags.split(',').map(tag => tag.trim()).filter(Boolean))]});
      editing = false; success('Changes saved.'); await loadPaste();
    } catch (e) { toastError(e.message); } finally { saving = false; }
  }
  async function removePaste() {
    if (deleting) return;
    deleting = true;
    try { await deletePaste(pasteId); deleteDialog.close(); success('Paste permanently deleted.'); dispatch('navigate',{view:'library'}); }
    catch (e) { toastError(e.message); } finally { deleting = false; }
  }
</script>

{#if loading}<div class="empty-state" role="status"><Icon name="code" size={28}/><p>Loading paste…</p></div>
{:else if passwordRequired}<section class="access-card panel" aria-labelledby="unlock-title"><span class="access-icon"><Icon name="lock" size={26}/></span><div class="eyebrow">A LITTLE PRIVACY</div><h1 id="unlock-title">This one needs a key.</h1><p class="muted">Enter the read password to open this paste.</p><form on:submit|preventDefault={() => loadPaste(passwordInput)}><label class="field"><span>Password</span><input bind:this={passwordInputEl} bind:value={passwordInput} type="password" required maxlength="256" aria-label="Enter password to unlock paste" placeholder="Enter password" autocomplete="off"/></label><button class="button primary" aria-label="Unlock paste">Unlock paste <Icon name="arrow" size={15}/></button></form><p class="field-hint">{error}</p></section>
{:else if burnRequired}<section class="access-card panel" aria-labelledby="reveal-title"><span class="access-icon"><Icon name="clock" size={26}/></span><div class="eyebrow">JUST THIS ONCE</div><h1 id="reveal-title">A fleeting little secret.</h1><p class="muted">Opening this paste destroys the server copy. Save what you need before leaving this page.</p><button class="button primary" on:click={() => loadPaste(currentPassword,true)}>Reveal one-time paste <Icon name="arrow" size={15}/></button></section>
{:else if error}<section class="access-card panel" role="alert"><Icon name={ownerRequired ? 'key' : 'file'} size={28}/><h1>{ownerRequired ? 'Just for the owner.' : 'Nothing here, for now.'}</h1><p class="muted">{error}</p>{#if ownerRequired}<form on:submit|preventDefault={restoreOwner}><label class="field"><span>Owner key</span><input type="password" bind:value={ownerInput} required minlength="64" maxlength="64" autocomplete="off" placeholder="Paste your owner key"/></label><button class="button primary">Restore access</button></form>{:else}<button class="button" on:click={() => loadPaste()}>Try again</button>{/if}<a class="text-button" href="#/">Create a new paste <Icon name="arrow" size={14}/></a></section>
{:else if paste}
  <article class="paste-detail" aria-label="Paste: {paste.title}">
    <a href="#/list" class="text-button back-link">← Back to Explore</a>
    {#if paste.burned}<div class="notice warning" role="alert">This paste has been burned. Its server copy is gone. You can copy or download what you see here until you leave this page.</div>{/if}
    <div class="page-heading paste-heading"><div><div class="eyebrow"><Icon name={paste.visibility === 'private' ? 'lock' : 'file'} size={13}/>{paste.visibility === 'private' ? 'YOUR PRIVATE PASTE' : 'A LITTLE SOMETHING TO SHARE'}</div><h1>{paste.title || 'Untitled'}</h1><div class="paste-metadata"><span class="pill accent">{paste.language}</span><span>{parseDate(paste.created_at).toLocaleString()}</span><span>{paste.views || 0} views</span><span>{formatSize(byteLength(paste.content))}</span><span class="pill">{paste.visibility}</span>{#if paste.has_password}<span class="pill"><Icon name="lock" size={11}/> Password protected</span>{/if}</div></div><button class="button" on:click={saveFavorite} aria-pressed={favorite} aria-label={favorite ? 'Remove from favorites' : 'Save to favorites'}><Icon name="star" size={15}/>{favorite ? 'Saved' : 'Save'}</button></div>
    {#if paste.expires_at}<p class="field-hint">Expires {parseDate(paste.expires_at).toLocaleString()}</p>{/if}
    {#if paste.parent_id}<a class="text-button" href="#/paste/{paste.parent_id}"><Icon name="fork" size={12}/> Forked from {paste.parent_id}</a>{/if}
    <div class="viewer-toolbar" role="group" aria-label="Paste actions"><div><button class="button" on:click={copyContent} aria-label={copied ? 'Copied!' : 'Copy to clipboard'}><Icon name={copied ? 'check' : 'copy'} size={14}/>{copied ? 'Copied' : 'Copy'}</button><button class="button" on:click={share} aria-label="Copy share link">Share link <Icon name="arrow" size={13}/></button>{#if !paste.burned && !paste.burn_after_read}<button class="button" on:click={handleFork} disabled={forking} aria-label="Fork paste"><Icon name="fork" size={14}/>{forking ? 'Forking…' : 'Fork'}</button>{/if}</div><div>{#if !paste.has_password && paste.visibility !== 'private' && !paste.burned}<a href={getRawUrl(pasteId)} target="_blank" rel="noopener noreferrer" class="button" aria-label="View raw paste">Raw</a>{:else}<button class="button" on:click={openRaw} aria-label="View raw paste">Raw</button>{/if}<button class="button" on:click={download} aria-label="Download paste as file"><Icon name="download" size={14}/> Download</button></div></div>
    {#if editing}<form class="panel edit-panel" on:submit|preventDefault={saveEdit} aria-label="Edit paste"><div class="panel-heading"><span class="panel-label"><Icon name="code" size={16}/> Edit your paste</span></div><div class="edit-fields"><label class="field"><span>Title</span><input bind:value={editTitle} maxlength="200"/></label><div class="form-row"><label class="field"><span>Language</span><select bind:value={editLanguage}>{#each LANGUAGES as lang}<option value={lang}>{lang}</option>{/each}</select></label><label class="field"><span>Visibility</span><select bind:value={editVisibility}><option value="public">Public</option><option value="unlisted">Unlisted</option><option value="private">Private</option></select></label></div><label class="field"><span>Tags</span><input bind:value={editTags} maxlength="500"/></label><label class="field"><span>Content</span><textarea bind:value={editContent} rows="18" spellcheck="false" on:keydown={(event) => { if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {event.preventDefault();saveEdit();} }}></textarea></label><div class="form-row"><button type="button" class="button" on:click={() => editing = false} disabled={saving}>Cancel</button><button class="button primary" disabled={saving}>{saving ? 'Saving…' : 'Save Changes'}</button></div></div></form>
    {:else}<section class="panel code-panel" aria-label="Paste content"><div class="panel-heading"><span class="panel-label"><Icon name="code" size={15}/>{paste.content.split('\n').length} lines<span class="muted">· {paste.id}</span></span><div class="code-controls"><button class="text-button" aria-label="Toggle word wrap" aria-pressed={wrap} on:click={() => wrap = !wrap}><Icon name="wrap" size={14}/> Wrap</button><button class="text-button" aria-label="Toggle line numbers" aria-pressed={showLines} on:click={() => showLines = !showLines}># Lines</button><button class="text-button" aria-label="Decrease code font size" on:click={() => fontSize = Math.max(11,fontSize-1)}>A−</button><button class="text-button" aria-label="Increase code font size" on:click={() => fontSize = Math.min(20,fontSize+1)}>A+</button></div></div><CodeBlock content={paste.content} language={paste.language} {wrap} {showLines} {fontSize}/></section>{/if}
    {#if paste.tags?.length}<div class="tag-list viewer-tags" role="list" aria-label="Tags">{#each paste.tags as tag}<span class="tag" role="listitem">#{tag}</span>{/each}</div>{/if}
    {#if paste.can_manage && !paste.burned}<div class="owner-panel"><span><Icon name="key" size={15}/> This paste is yours. Keep your owner key safe.</span><div><button class="text-button" on:click={copyOwnerKey}>Copy owner key</button><button class="button small" on:click={startEdit} aria-label="Edit paste">Edit paste</button><button class="text-button error-text" on:click={() => deleteDialog.showModal()} aria-label="Delete paste">Delete</button></div></div>{/if}
    {#if !paste.has_password && paste.visibility === 'public' && !paste.burned && !paste.burn_after_read}<details class="embed-snippet"><summary>Embed this snippet</summary><p class="field-hint">This public snippet can be displayed on another page. Protected and one-time content cannot be embedded.</p><code>&lt;iframe src="{window.location.origin}/embed/{paste.id}" title="PasteBox snippet" width="100%" height="400" loading="lazy"&gt;&lt;/iframe&gt;</code></details>{/if}
    <dialog bind:this={deleteDialog} aria-labelledby="delete-title"><h2 id="delete-title">Delete this paste?</h2><p class="muted">The paste and its tags will be permanently deleted. This cannot be undone.</p><div class="form-row"><button class="button" on:click={() => deleteDialog.close()} disabled={deleting}>Keep paste</button><button class="button danger" on:click={removePaste} disabled={deleting}>{deleting ? 'Deleting…' : 'Permanently delete'}</button></div></dialog>
  </article>
{/if}
