import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/svelte';
import CreatePaste from '../CreatePaste.svelte';

// Mock api module
vi.mock('../lib/api.js', () => ({
  createPaste: vi.fn()
}));

// Mock toast module
vi.mock('../lib/toast.js', () => ({
  success: vi.fn(),
  error: vi.fn()
}));

import { createPaste } from '../lib/api.js';

describe('CreatePaste', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('renders the form with all fields', () => {
    render(CreatePaste);

    expect(screen.getByRole('textbox', { name: 'Paste title' })).toBeTruthy();
    expect(screen.getByLabelText(/paste content/i)).toBeTruthy();
    expect(screen.getByLabelText(/^language$/i)).toBeTruthy();
    expect(screen.getByLabelText(/visibility/i)).toBeTruthy();
    expect(screen.getByRole('button', { name: /create paste/i })).toBeTruthy();
  });

  it('has a content textarea', () => {
    render(CreatePaste);
    const textarea = screen.getByLabelText(/paste content/i);
    expect(textarea.tagName).toBe('TEXTAREA');
  });

  it('shows language select with options', () => {
    render(CreatePaste);
    const select = screen.getByLabelText(/^language$/i);
    expect(select.tagName).toBe('SELECT');
    const options = select.querySelectorAll('option');
    expect(options.length).toBeGreaterThan(5);
  });

  it('shows visibility select', () => {
    render(CreatePaste);
    const select = screen.getByLabelText(/visibility/i);
    const options = select.querySelectorAll('option');
    const values = Array.from(options).map(o => o.value);
    expect(values).toContain('public');
    expect(values).toContain('unlisted');
    expect(values).toContain('private');
  });

  it('has password field', () => {
    render(CreatePaste);
    const pw = screen.getByLabelText(/password.*protect/i);
    expect(pw).toBeTruthy();
    expect(pw.type).toBe('password');
  });

  it('has tags field', () => {
    render(CreatePaste);
    const tags = screen.getByRole('textbox', { name: 'Tags' });
    expect(tags).toBeTruthy();
  });

  it('has burn after read checkbox', () => {
    render(CreatePaste);
    const burn = screen.getByLabelText(/burn after read/i);
    expect(burn).toBeTruthy();
    expect(burn.type).toBe('checkbox');
  });

  it('validates password minimum length', async () => {
    render(CreatePaste);
    const pw = screen.getByLabelText(/password.*protect/i);
    const textarea = screen.getByLabelText(/paste content/i);

    await fireEvent.input(textarea, { target: { value: 'test content' } });
    await fireEvent.input(pw, { target: { value: 'ab' } });
    await fireEvent.click(screen.getByRole('button', { name: /create paste/i }));

    expect(screen.getByText(/min 4 characters/i)).toBeTruthy();
    expect(createPaste).not.toHaveBeenCalled();
  });

  it('shows error when content is empty', async () => {
    render(CreatePaste);
    await fireEvent.click(screen.getByRole('button', { name: /create paste/i }));

    // Should show validation error or not call API
    expect(createPaste).not.toHaveBeenCalled();
  });

  it('calls createPaste on valid submit', async () => {
    createPaste.mockResolvedValueOnce({ id: 'test123', title: 'Test' });

    render(CreatePaste);
    const textarea = screen.getByLabelText(/paste content/i);
    const title = screen.getByRole('textbox', { name: 'Paste title' });

    await fireEvent.input(title, { target: { value: 'My Paste' } });
    await fireEvent.input(textarea, { target: { value: 'console.log("hello")' } });
    await fireEvent.click(screen.getByRole('button', { name: /create paste/i }));

    expect(createPaste).toHaveBeenCalledTimes(1);
    const callArgs = createPaste.mock.calls[0][0];
    expect(callArgs.title).toBe('My Paste');
    expect(callArgs.content).toBe('console.log("hello")');
  });

  it('has keyboard shortcut hint', () => {
    render(CreatePaste);
    // Ctrl+Enter shortcut should be mentioned somewhere
    const body = document.body.textContent;
    expect(body).toContain('Ctrl');
  });
});

it('prevents duplicate submission while a create is in progress', async () => {
  vi.clearAllMocks();
  createPaste.mockReturnValueOnce(new Promise(() => {}));
  render(CreatePaste);
  await fireEvent.input(screen.getByLabelText(/paste content/i), { target: { value: 'my snippet' } });
  await fireEvent.keyDown(screen.getByLabelText(/paste content/i), { key: 'Enter', ctrlKey: true });
  await fireEvent.keyDown(screen.getByLabelText(/paste content/i), { key: 'Enter', ctrlKey: true });
  expect(createPaste).toHaveBeenCalledTimes(1);
});

it('disables draft persistence for protected content', async () => {
  render(CreatePaste);
  await fireEvent.input(screen.getByLabelText(/password protection/i), { target: { value: 'secret password' } });
  expect(screen.getByLabelText(/save public draft/i).disabled).toBe(true);
});
