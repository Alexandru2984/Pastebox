<script>
  import { highlight } from './lib/highlight.js';
  export let content = '';
  export let language = 'plaintext';
  export let wrap = false;
  export let showLines = true;
  export let fontSize = 13;
  $: lineCount = content.split('\n').length;
  $: numbers = showLines && !wrap && lineCount <= 2000 ? Array.from({length:lineCount},(_,index) => index+1).join('\n') : '';
</script>
<div class="code-scroll" class:code-wrap={wrap} style="--code-font-size:{fontSize}px">
  {#if numbers}<pre class="line-numbers" aria-hidden="true">{numbers}</pre>{/if}
  <div class="code-pre" tabindex="0" role="textbox" aria-readonly="true" aria-multiline="true" aria-label="Code content"><pre><code use:highlight={{content,language}}>{content}</code></pre></div>
</div>
