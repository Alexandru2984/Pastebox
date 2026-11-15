<script>
  import { createEventDispatcher, onMount } from 'svelte';
  import { createPaste } from './lib/api.js';
  import { LANGUAGES } from './lib/languages.js';
  import { MAX_BYTES, byteLength, formatSize } from './lib/format.js';
  import { validatePaste, TEMPLATES, languageForFile } from './lib/editor.js';
  import { success, error as toastError } from './lib/toast.js';
  import Icon from './Icon.svelte';
  const dispatch = createEventDispatcher();
  const DRAFT_KEY = 'pastebox-public-draft-v1';
  let title = '', content = '', language = 'plaintext', visibility = 'public', expiresIn = '', password = '', tags = '';
  let burnAfterRead = false, submitting = false, error = '', passwordError = '', template = '', wrap = false;
  let indentWithTab = true;
  let editor, fileInput, rememberDraft = false, draftOffered = null, draftStatus = '', mounted = false, draftTimer;
  $: bytes = byteLength(content);
  $: safeDraft = visibility === 'public' && !password && !burnAfterRead;
  $: if (mounted) scheduleDraft(content, title, language, tags, rememberDraft, safeDraft);
  onMount(() => {
    try {
      const saved = JSON.parse(localStorage.getItem(DRAFT_KEY) || 'null');
      if (saved && typeof saved.content === 'string' && byteLength(saved.content) <= MAX_BYTES && typeof saved.title === 'string' && typeof saved.tags === 'string' && LANGUAGES.includes(saved.language)) draftOffered = saved;
    } catch {}
    mounted = true;
    return () => clearTimeout(draftTimer);
  });
  function scheduleDraft(body, name, lang, labels, enabled, safe) {
    clearTimeout(draftTimer);
    if (!safe || (!enabled && !draftOffered)) {
      try { localStorage.removeItem(DRAFT_KEY); } catch {}
      draftStatus = ''; return;
    }
    if (!enabled || draftOffered) return;
    draftStatus = 'Saving…';
    draftTimer = setTimeout(() => {
      try {
        if (body && byteLength(body) <= MAX_BYTES) localStorage.setItem(DRAFT_KEY, JSON.stringify({content:body,title:name,language:lang,tags:labels}));
        else localStorage.removeItem(DRAFT_KEY);
        draftStatus = body ? 'Draft saved on this device' : '';
      } catch { draftStatus = 'Draft could not be saved'; }
    },500);
  }
  function restoreDraft() {
    title = draftOffered.title; content = draftOffered.content; language = draftOffered.language; tags = draftOffered.tags;
    rememberDraft = true; draftOffered = null; success('Public draft restored.');
  }
  function discardDraft() { draftOffered = null; try { localStorage.removeItem(DRAFT_KEY); } catch {} }
  function validatePassword() {
    passwordError = password && byteLength(password) < 4 ? 'Min 4 characters (4 UTF-8 bytes)' : byteLength(password) > 256 ? 'Max 256 bytes' : '';
    return !passwordError;
  }
  function handleKeydown(event) {
    if ((event.ctrlKey || event.metaKey) && (event.key === 'Enter' || event.key.toLowerCase() === 's')) { event.preventDefault(); handleSubmit(); }
    if (event.key === 'Escape') indentWithTab = false;
    if (event.key === 'Tab' && !event.shiftKey && indentWithTab) {
      event.preventDefault();
      const start = event.target.selectionStart, end = event.target.selectionEnd;
      const insert = event.shiftKey ? '' : '  ';
      if (!event.shiftKey) {
        content = content.slice(0,start) + insert + content.slice(end);
        requestAnimationFrame(() => { editor.selectionStart = editor.selectionEnd = start + 2; });
      }
    }
  }
  function clearEditor() {
    if (content && !window.confirm('Clear the editor and discard its draft?')) return;
    title = ''; content = ''; tags = ''; error = ''; template = ''; discardDraft();
  }
  function handleGlobalKeydown(event) {
    if ((event.ctrlKey || event.metaKey) && event.shiftKey && event.key.toLowerCase() === 'n') { event.preventDefault(); clearEditor(); }
  }
  function applyTemplate() {
    if (!template) return;
    const selected = TEMPLATES[template];
    if (content.trim() && !window.confirm('Replace the current editor content with this template?')) { template = ''; return; }
    title = selected.title; content = selected.content; language = selected.language; template = ''; editor?.focus();
  }
  function formatJson() {
    try { content = JSON.stringify(JSON.parse(content),null,2); language = 'json'; success('JSON formatted.'); }
    catch { toastError('This content is not valid JSON. Your text has been kept.'); }
  }
  async function importFile(event) {
    const file = event.target.files?.[0];
    if (!file) return;
    try {
      if (file.size > MAX_BYTES) throw new Error('Choose a text file smaller than 512 KB.');
      const value = await file.text();
      if (value.includes('\0')) throw new Error('Choose a text or code file rather than a binary file.');
      if (content.trim() && !window.confirm('Replace the editor content with this file?')) return;
      content = value; title = file.name; language = languageForFile(file.name); success('File imported into the editor.');
    } catch (e) { toastError(e.message); }
    finally { event.target.value = ''; }
  }
  async function handleSubmit() {
    if (submitting) return;
    if (!validatePassword()) { error = ''; return; }
    error = validatePaste({content,title,password,tags});
    if (error) return;
    submitting = true;
    try {
      const result = await createPaste({ title:title || 'Untitled',content,language,visibility,expires_in:expiresIn,password,burn_after_read:burnAfterRead,tags:[...new Set(tags.split(',').map(tag => tag.trim()).filter(Boolean))] });
      if (result.owner_saved === false) toastError('Owner key is available for this session only. Copy it from Library before closing this tab.');
      success('Your paste is ready to share.');
      clearTimeout(draftTimer); rememberDraft = false; discardDraft(); content = '';
      dispatch('navigate',{view:'view',id:result.id});
    } catch (e) { error = e.message || 'Could not create this paste. Please retry.'; }
    finally { submitting = false; }
  }
</script>
<svelte:window on:keydown={handleGlobalKeydown}/>
<section aria-label="Create a paste">
  <div class="page-heading create-heading"><div><div class="eyebrow"><span class="status-dot"></span> SMALL SNIPPETS. BIG POSSIBILITIES.</div><h1>A good idea<br/>deserves a <span class="heading-highlight">link.</span></h1><p class="subheading">Drop your code, notes, or next big thing.<br class="desktop-break"/> Share it in seconds. Keep creating.</p></div><div class="hero-note"><span class="hero-note-symbol">{`{ }`}</span><div>No sign-up.<br/>Just your next idea.</div><span class="hero-note-line"></span></div></div>
  {#if draftOffered}<div class="notice draft-offer"><span>You have an unfinished public draft on this device.</span><div><button class="text-button" on:click={restoreDraft}>Restore draft</button><button class="text-button" on:click={discardDraft}>Discard</button></div></div>{/if}
  {#if error}<div class="notice error" role="alert">{error}</div>{/if}
  <form class="composer-layout" on:submit|preventDefault={handleSubmit}>
    <div class="composer-main">
      <div class="panel editor-panel">
        <div class="panel-heading"><span class="panel-label"><Icon name="code" size={16}/> The canvas <span class="muted">/</span> <span class="muted">{language}</span></span><label class="template-select"><span class="sr-only">Start from a template</span><select bind:value={template} on:change={applyTemplate}><option value="">Start with a template</option>{#each Object.keys(TEMPLATES) as key}<option value={key}>{key}</option>{/each}</select></label></div>
        <div class="editor-title"><label for="paste-title" class="sr-only">Title</label><input id="paste-title" bind:value={title} placeholder="Give it a name (optional)" maxlength="200" aria-label="Paste title"/><span class="pill">.{language === 'plaintext' ? 'txt' : language}</span></div>
        <div class="editor-surface"><span class="editor-gutter" aria-hidden="true">01</span><label for="paste-content" class="sr-only">Paste content</label><textarea id="paste-content" bind:this={editor} bind:value={content} on:keydown={handleKeydown} on:blur={() => indentWithTab = true} aria-describedby="editor-help" aria-label="Paste content" aria-required="true" rows="17" spellcheck="false" wrap={wrap ? 'soft' : 'off'} placeholder="// A blank canvas for your next idea.&#10;// Paste something worth sharing…" class:wrap-text={wrap}></textarea></div>
        <div class="editor-status"><div><span>{formatSize(bytes)} / 512 KB</span><span>{content.split('\n').length} lines</span>{#if bytes > MAX_BYTES}<span class="error-text">Too large</span>{/if}</div><div><button type="button" class="text-button" on:click={() => wrap = !wrap} aria-pressed={wrap} aria-label="Toggle word wrap"><Icon name="wrap" size={13}/> Wrap</button><button type="button" class="text-button" on:click={clearEditor}>Clear</button></div></div>
      </div>
      <div class="editor-tools"><div><input type="file" bind:this={fileInput} class="sr-only" aria-label="Import code file" on:change={importFile}/><button type="button" class="text-button" on:click={() => fileInput.click()}><Icon name="file" size={14}/> Import a file</button><span class="muted">·</span><button type="button" class="text-button" on:click={formatJson}>{`{ }`} Format JSON</button></div><span id="editor-help" class="muted">Tab to indent · Esc, then Tab to leave · <kbd>Ctrl</kbd> + <kbd>Enter</kbd> to share</span></div>
      <div class="feature-strip"><span><Icon name="code" size={15}/> 30+ languages</span><span><Icon name="lock" size={14}/> You're in control</span><span><Icon name="clock" size={14}/> Here for a moment, or longer</span></div>
    </div>
    <aside class="composer-sidebar" aria-label="Paste settings">
      <div class="panel settings-panel"><div class="panel-heading"><span class="panel-label">Make it yours</span><span class="muted" style="font-size:11px">01 — SETTINGS</span></div><div class="settings-body">
        <label class="field"><span>Language</span><select bind:value={language} aria-label="Language">{#each LANGUAGES as lang}<option value={lang}>{lang === 'plaintext' ? 'Plain text' : lang}</option>{/each}</select></label>
        <label class="field"><span>Visibility</span><select bind:value={visibility} aria-label="Paste visibility"><option value="public">Public · anyone can discover</option><option value="unlisted">Unlisted · link access</option><option value="private">Private · only your owner key</option></select><small>{visibility === 'private' ? 'Only your owner key can open this paste. A reading password does not grant private access.' : visibility === 'unlisted' ? 'Hidden from Explore. Anyone with the link can read unless you set a password.' : 'Your title and tags appear in Explore. Avoid sharing personal information.'}</small></label>
        <label class="field"><span>Expiration</span><select bind:value={expiresIn} aria-label="Paste expiration"><option value="">Keep forever</option><option value="1h">After 1 hour</option><option value="24h">After 24 hours</option><option value="7d">After 7 days</option><option value="30d">After 30 days</option></select></label>
        {#if visibility !== 'private'}<label class="field"><span>Password <span class="muted">(optional)</span></span><input type="password" bind:value={password} on:blur={validatePassword} placeholder="Add a little privacy" aria-label="Password protection" maxlength="256" autocomplete="new-password" aria-invalid={Boolean(passwordError)} aria-describedby={passwordError ? 'pw-error' : undefined}/>{#if passwordError}<small id="pw-error" class="error-text">{passwordError}</small>{/if}</label>{/if}
        <label class="field"><span>Tags <span class="muted">(optional)</span></span><input bind:value={tags} placeholder="api, tutorial, useful" aria-label="Tags" maxlength="500"/><small>Comma separated · up to 10 tags</small></label>
        <label class="checkbox-row"><input type="checkbox" bind:checked={burnAfterRead} aria-label="Burn after read"/><span><strong>Burn after read</strong><small>Reveal once. Then it’s gone.</small></span><Icon name="clock" size={15}/></label>
      </div></div>
      <label class="draft-toggle"><input type="checkbox" bind:checked={rememberDraft} disabled={!safeDraft} aria-label="Save public draft on this device"/><span>Save public draft on this device<small>{safeDraft ? draftStatus || 'Optional. Stored only in this browser.' : 'Drafts are off for protected, unlisted, or one-time content.'}</small></span></label>
      <button class="button primary create-button" type="submit" disabled={submitting || bytes > MAX_BYTES} aria-label={submitting ? 'Creating paste...' : 'Create paste'}>{submitting ? 'Creating…' : 'Create Paste'}<Icon name="arrow" size={16}/></button><p class="create-footnote">Your owner key stays in this browser.<br/>Back it up from your Library.</p>
    </aside>
  </form>
</section>
