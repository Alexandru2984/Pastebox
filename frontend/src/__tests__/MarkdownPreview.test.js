import { it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, cleanup } from '@testing-library/svelte';
import { tick } from 'svelte';
import MarkdownPreview from '../MarkdownPreview.svelte';
let workers;
class FakeWorker {
  constructor() {workers.push(this);}
  postMessage = vi.fn();
  terminate = vi.fn();
}
beforeEach(() => {workers = [];vi.useFakeTimers();vi.stubGlobal('Worker',FakeWorker);});
afterEach(async () => {await cleanup();vi.useRealTimers();vi.unstubAllGlobals();});

it('terminates the parser when its work exceeds the time budget', async () => {
  render(MarkdownPreview,{props:{content:'# source'}});
  await tick();
  workers[0].onmessage({data:{ready:true}});
  expect(workers[0].postMessage).toHaveBeenCalledWith({content:'# source'});
  await vi.advanceTimersByTimeAsync(1501);
  expect(workers[0].terminate).toHaveBeenCalled();
  expect(screen.getByRole('alert').textContent).toContain('Preview took too long');
});
it('does not start a worker for an oversized draft', async () => {
  render(MarkdownPreview,{props:{content:'x'.repeat(128*1024+1)}});
  await tick();
  expect(workers).toHaveLength(0);
  expect(screen.getByRole('alert').textContent).toContain('128 KB');
});
it('terminates an in-flight parser when the preview closes', async () => {
  const result = render(MarkdownPreview,{props:{content:'# source'}});
  await tick();
  await result.unmount();
  expect(workers[0].terminate).toHaveBeenCalled();
});
