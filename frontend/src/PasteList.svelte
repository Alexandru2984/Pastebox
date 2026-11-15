<script>
  import { createEventDispatcher, onMount } from 'svelte';
  import { listPastes } from './lib/api.js';
  import { LANGUAGES } from './lib/languages.js';
  import { timeAgo, formatSize } from './lib/format.js';
  import Icon from './Icon.svelte';
  const dispatch = createEventDispatcher();
  let pastes = [], loading = true, error = '', filterTag = '', query = '', language = '', sort = 'newest';
  let page = 1, hasMore = false, requestNumber = 0;
  onMount(() => { loadPastes(); return () => { requestNumber++; }; });
  async function loadPastes(tag = filterTag, p = 1) {
    const request = ++requestNumber;
    loading = true; error = ''; filterTag = tag;
    try {
      const result = await listPastes(tag, p, 24, { q: query.trim(), language, sort });
      if (request !== requestNumber) return;
      pastes = result.data || []; page = result.page || p; hasMore = Boolean(result.has_more);
    } catch (e) { if (request === requestNumber) error = e.message || 'Failed to load pastes'; }
    finally { if (request === requestNumber) loading = false; }
  }
</script>
<section aria-label="Paste list">
  <div class="page-heading"><div><div class="eyebrow"><span class="status-dot"></span> THE PUBLIC NOTEBOOK</div><h1>Good code travels.</h1><p class="subheading">Explore fresh ideas, handy snippets, and things worth sharing.</p></div><a href="#/" class="button"><Icon name="plus" size={15}/> Share a snippet</a></div>
  <form class="filter-bar" on:submit|preventDefault={() => loadPastes()} role="search">
    <label class="search-field"><span class="sr-only">Search public pastes</span><Icon name="search" size={17}/><input bind:value={query} maxlength="100" placeholder="Search titles and tags…" type="search"/></label>
    <select bind:value={language} on:change={() => loadPastes()} aria-label="Filter by language"><option value="">All languages</option>{#each LANGUAGES as lang}<option value={lang}>{lang}</option>{/each}</select>
    <select bind:value={sort} on:change={() => loadPastes()} aria-label="Sort pastes"><option value="newest">Newest first</option><option value="oldest">Oldest first</option><option value="popular">Most viewed</option></select>
    <button class="button" type="submit">Search</button>
  </form>
  <div class="result-meta"><span>{filterTag ? `Pastes tagged #${filterTag}` : 'Recent Pastes'}{!loading && !error ? ` · ${pastes.length} on this page` : ''}</span>{#if filterTag}<button class="text-button" on:click={() => loadPastes('')}>Clear tag filter ×</button>{/if}</div>
  {#if loading}<div class="paste-grid" role="status" aria-label="Loading pastes"><span class="sr-only">Loading…</span>{#each [1,2,3,4] as item}<div class="skeleton" aria-hidden="true"></div>{/each}</div>
  {:else if error}<div class="empty-state" role="alert"><h2>Couldn't load these snippets.</h2><p>{error}</p><button class="button" on:click={() => loadPastes(filterTag,page)}>Try again</button></div>
  {:else if pastes.length === 0}<div class="empty-state"><Icon name="search" size={30}/><h2>No pastes found.</h2><p>Try a different search or language, or share something of your own.</p><a class="button" href="#/">Create a paste <Icon name="arrow" size={15}/></a></div>
  {:else}<ul class="paste-grid" aria-label="Pastes">{#each pastes as paste (paste.id)}<li><article class="paste-card">
    <div class="paste-card-head"><span class="language-label"><span class="language-dot"></span>{paste.language}</span><span class="muted" style="font-size:10px">{#if paste.has_password}<Icon name="lock" size={13}/>{:else}PUBLIC{/if}</span></div>
    <h2 class="card-title"><button on:click={() => dispatch('view',paste.id)} aria-label="View paste: {paste.title || 'Untitled'}">{paste.title || 'Untitled'}</button></h2>
    <div class="card-details"><span><Icon name="clock" size={12}/>{timeAgo(paste.created_at)}</span><span>{formatSize(paste.size)}</span><span>{paste.views || 0} views</span>{#if paste.burn_after_read}<span>One-time</span>{/if}{#if paste.expires_at}<span>Expires</span>{/if}</div>
    {#if paste.tags?.length}<div class="tag-list">{#each paste.tags as tag}<button class="tag" on:click={() => loadPastes(tag)} aria-label="Filter by tag {tag}">#{tag}</button>{/each}</div>{/if}
  </article></li>{/each}</ul>
  <nav class="pagination" aria-label="Pagination">{#if page > 1}<button class="button small" on:click={() => loadPastes(filterTag,page-1)} aria-label="Previous page">← Previous</button>{/if}<span aria-current="page">Page {page}</span><button class="button small" on:click={() => loadPastes(filterTag,page+1)} disabled={!hasMore} aria-label="Next page">Next <Icon name="arrow" size={13}/></button></nav>{/if}
</section>
