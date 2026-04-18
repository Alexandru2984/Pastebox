import { describe, it, expect, vi, beforeEach } from 'vitest';
import { toasts, addToast, removeToast, success, error, info } from '../lib/toast.js';
import { get } from 'svelte/store';

describe('Toast Store', () => {
  beforeEach(() => {
    toasts.set([]);
  });

  it('starts empty', () => {
    expect(get(toasts)).toEqual([]);
  });

  it('addToast adds a toast', () => {
    addToast('Hello', 'info', 0);
    const t = get(toasts);
    expect(t).toHaveLength(1);
    expect(t[0].message).toBe('Hello');
    expect(t[0].type).toBe('info');
  });

  it('removeToast removes by id', () => {
    const id = addToast('A', 'info', 0);
    addToast('B', 'info', 0);
    expect(get(toasts)).toHaveLength(2);
    removeToast(id);
    expect(get(toasts)).toHaveLength(1);
    expect(get(toasts)[0].message).toBe('B');
  });

  it('success() creates success toast', () => {
    vi.useFakeTimers();
    success('Yay');
    const t = get(toasts);
    expect(t).toHaveLength(1);
    expect(t[0].type).toBe('success');
    expect(t[0].message).toBe('Yay');
    vi.useRealTimers();
  });

  it('error() creates error toast', () => {
    vi.useFakeTimers();
    error('Bad');
    expect(get(toasts)[0].type).toBe('error');
    vi.useRealTimers();
  });

  it('info() creates info toast', () => {
    vi.useFakeTimers();
    info('FYI');
    expect(get(toasts)[0].type).toBe('info');
    vi.useRealTimers();
  });

  it('auto-removes after duration', () => {
    vi.useFakeTimers();
    addToast('Temp', 'info', 1000);
    expect(get(toasts)).toHaveLength(1);
    vi.advanceTimersByTime(1100);
    expect(get(toasts)).toHaveLength(0);
    vi.useRealTimers();
  });
});
