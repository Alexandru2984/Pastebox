import { it, expect } from 'vitest';
import { render, screen, waitFor } from '@testing-library/svelte';
import CodeBlock from '../CodeBlock.svelte';

it('keeps code visible immediately and treats script markup as text after highlighting', async () => {
  const content = '<script>window.stolen = true</script>\n<p>hello</p>';
  render(CodeBlock,{ props:{content,language:'html'} });
  const box = screen.getByRole('textbox',{name:'Code content'});
  expect(box.textContent).toBe(content);
  await waitFor(() => expect(box.querySelector('.hljs-tag')).toBeTruthy());
  expect(box.querySelector('script')).toBeNull();
  expect(box.textContent).toBe(content);
});

it('bounds the line number DOM for very large pastes', () => {
  render(CodeBlock,{props:{content:'\n'.repeat(10000),language:'plaintext'}});
  expect(document.querySelector('.line-numbers')).toBeNull();
  expect(screen.getByRole('textbox').textContent.length).toBe(10000);
});
