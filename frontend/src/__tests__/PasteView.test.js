import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import PasteView from '../PasteView.svelte';

// Mock highlight.js
vi.mock('highlight.js', () => ({
  default: {
    highlightElement: vi.fn(),
    highlightAll: vi.fn()
  }
}));
vi.mock('highlight.js/styles/github-dark.css', () => ({}));

// Mock api module
vi.mock('../lib/api.js', () => ({
  getPaste: vi.fn(),
  forkPaste: vi.fn(),
  updatePaste: vi.fn(),
  getRawUrl: vi.fn((id) => `/api/pastes/${id}/raw`)
}));

// Mock toast module
vi.mock('../lib/toast.js', () => ({
  success: vi.fn(),
  error: vi.fn()
}));

import { getPaste, forkPaste, updatePaste } from '../lib/api.js';

const mockPaste = {
  id: 'abc123',
  title: 'Test Paste',
  content: 'console.log("hello");',
  language: 'javascript',
  visibility: 'public',
  view_count: 5,
  created_at: new Date().toISOString(),
  tags: ['demo'],
  size: 22,
  burned: false
};

describe('PasteView', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    // Mock clipboard
    Object.assign(navigator, {
      clipboard: { writeText: vi.fn().mockResolvedValue(undefined) }
    });
  });

  it('shows loading state', () => {
    getPaste.mockReturnValue(new Promise(() => {}));
    render(PasteView, { props: { pasteId: 'abc123' } });
    expect(screen.getByText(/loading/i)).toBeTruthy();
  });

  it('renders paste content after loading', async () => {
    getPaste.mockResolvedValueOnce(mockPaste);
    render(PasteView, { props: { pasteId: 'abc123' } });

    await waitFor(() => {
      expect(screen.getByText('Test Paste')).toBeTruthy();
    });
  });

  it('shows error for nonexistent paste', async () => {
    const err = new Error('Paste not found');
    err.passwordRequired = false;
    getPaste.mockRejectedValueOnce(err);
    render(PasteView, { props: { pasteId: 'nonexist' } });

    await waitFor(() => {
      expect(screen.getByText(/paste not found/i)).toBeTruthy();
    });
  });

  it('shows password prompt for protected paste', async () => {
    const err = new Error('Password required');
    err.passwordRequired = true;
    err.status = 404;
    getPaste.mockRejectedValueOnce(err);
    render(PasteView, { props: { pasteId: 'protected' } });

    await waitFor(() => {
      expect(screen.getByPlaceholderText(/password/i)).toBeTruthy();
    });
  });

  it('displays paste metadata', async () => {
    getPaste.mockResolvedValueOnce(mockPaste);
    render(PasteView, { props: { pasteId: 'abc123' } });

    await waitFor(() => {
      expect(screen.getByText(/javascript/i)).toBeTruthy();
      const body = document.body.textContent;
      expect(body).toContain('view');
    });
  });

  it('has copy button', async () => {
    getPaste.mockResolvedValueOnce(mockPaste);
    render(PasteView, { props: { pasteId: 'abc123' } });

    await waitFor(() => {
      const buttons = screen.getAllByRole('button', { name: /copy/i });
      expect(buttons.length).toBeGreaterThan(0);
    });
  });

  it('has raw button/link', async () => {
    getPaste.mockResolvedValueOnce(mockPaste);
    render(PasteView, { props: { pasteId: 'abc123' } });

    await waitFor(() => {
      expect(screen.getByRole('link', { name: /raw/i })).toBeTruthy();
    });
  });

  it('has edit button', async () => {
    getPaste.mockResolvedValueOnce(mockPaste);
    render(PasteView, { props: { pasteId: 'abc123' } });

    await waitFor(() => {
      expect(screen.getByRole('button', { name: /edit/i })).toBeTruthy();
    });
  });

  it('has fork button', async () => {
    getPaste.mockResolvedValueOnce(mockPaste);
    render(PasteView, { props: { pasteId: 'abc123' } });

    await waitFor(() => {
      expect(screen.getByRole('button', { name: /fork/i })).toBeTruthy();
    });
  });

  it('shows tags', async () => {
    getPaste.mockResolvedValueOnce(mockPaste);
    render(PasteView, { props: { pasteId: 'abc123' } });

    await waitFor(() => {
      const body = document.body.textContent;
      expect(body).toContain('demo');
    });
  });
});
