<script>
  import { onMount } from 'svelte';
  import { themes, applyTheme, getStoredTheme } from './lib/themes.js';
  let currentTheme = 'dark';
  onMount(() => { currentTheme = getStoredTheme(); });
</script>
<label class="theme-control"><span aria-hidden="true">◐</span><span class="sr-only">Color theme</span><select aria-label="Color theme" bind:value={currentTheme} on:change={() => applyTheme(currentTheme)}>{#each Object.entries(themes) as [key, theme]}<option value={key}>{theme.name}</option>{/each}</select></label>
