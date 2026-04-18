import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import PasteList from '../PasteList.svelte';

// Mock api module
vi.mock('../lib/api.js', () => ({
  listPastes: vi.fn()
}));

import { listPastes } from '../lib/api.js';

const mockPastes = [
  { id: 'abc123', title: 'Hello World', language: 'javascript', size: 256, visibility: 'public', view_count: 5, created_at: new Date().toISOString(), tags: ['js', 'demo'] },
  { id: 'def456', title: 'Python Script', language: 'python', size: 1024, visibility: 'public', view_count: 12, created_at: new Date().toISOString(), tags: ['python'] },
  { id: 'ghi789', title: '', language: 'plaintext', size: 50, visibility: 'public', view_count: 0, created_at: new Date().toISOString(), tags: [] }
];

describe('PasteList', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('renders loading state initially', () => {
    listPastes.mockReturnValue(new Promise(() => {})); // never resolves
    render(PasteList);
    expect(screen.getByText(/loading/i)).toBeTruthy();
  });

  it('renders paste items after loading', async () => {
    listPastes.mockResolvedValueOnce({ data: mockPastes, page: 1, has_more: false });
    render(PasteList);

    await waitFor(() => {
      expect(screen.getByText('Hello World')).toBeTruthy();
      expect(screen.getByText('Python Script')).toBeTruthy();
    });
  });

  it('shows error on failed load', async () => {
    listPastes.mockRejectedValueOnce(new Error('Network error'));
    render(PasteList);

    await waitFor(() => {
      expect(screen.getByText(/failed to load/i)).toBeTruthy();
    });
  });

  it('renders paste items with correct structure', async () => {
    listPastes.mockResolvedValueOnce({ data: mockPastes, page: 1, has_more: false });
    render(PasteList);

    await waitFor(() => {
      // All 3 pastes should be rendered as list items
      const items = screen.getAllByRole('listitem');
      expect(items.length).toBe(3);
    });
  });

  it('displays language badges', async () => {
    listPastes.mockResolvedValueOnce({ data: mockPastes, page: 1, has_more: false });
    render(PasteList);

    await waitFor(() => {
      expect(screen.getByText('javascript')).toBeTruthy();
      expect(screen.getByText('python')).toBeTruthy();
    });
  });

  it('shows pagination when has_more', async () => {
    listPastes.mockResolvedValueOnce({ data: mockPastes, page: 1, has_more: true });
    render(PasteList);

    await waitFor(() => {
      expect(screen.getByRole('button', { name: /next/i })).toBeTruthy();
    });
  });

  it('hides prev button on page 1', async () => {
    listPastes.mockResolvedValueOnce({ data: mockPastes, page: 1, has_more: true });
    render(PasteList);

    await waitFor(() => {
      expect(screen.queryByRole('button', { name: /prev/i })).toBeNull();
    });
  });

  it('shows prev button on page 2+', async () => {
    listPastes.mockResolvedValueOnce({ data: mockPastes, page: 2, has_more: false });
    render(PasteList);

    await waitFor(() => {
      expect(screen.getByRole('button', { name: /prev/i })).toBeTruthy();
    });
  });

  it('shows tags on pastes', async () => {
    listPastes.mockResolvedValueOnce({ data: mockPastes, page: 1, has_more: false });
    render(PasteList);

    await waitFor(() => {
      const body = document.body.textContent;
      expect(body).toContain('js');
      expect(body).toContain('demo');
    });
  });

  it('shows empty state when no pastes', async () => {
    listPastes.mockResolvedValueOnce({ data: [], page: 1, has_more: false });
    render(PasteList);

    await waitFor(() => {
      expect(screen.getByText(/no pastes/i)).toBeTruthy();
    });
  });
});
