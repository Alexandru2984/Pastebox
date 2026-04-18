import { describe, it, expect, vi, beforeEach } from 'vitest';

// Mock fetch globally
const mockFetch = vi.fn();
global.fetch = mockFetch;

// Must import after mocking fetch
const { createPaste, getPaste, listPastes, deletePaste, forkPaste, getRawUrl } = await import('../lib/api.js');

describe('API Client', () => {
  beforeEach(() => {
    mockFetch.mockReset();
  });

  describe('createPaste', () => {
    it('sends POST with JSON body', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ id: 'abc123', title: 'Test' })
      });

      const result = await createPaste({ title: 'Test', content: 'hello', language: 'plaintext' });

      expect(mockFetch).toHaveBeenCalledWith('/api/pastes', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ title: 'Test', content: 'hello', language: 'plaintext' })
      });
      expect(result.id).toBe('abc123');
    });

    it('throws on error response', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: false,
        json: () => Promise.resolve({ error: 'Content is empty' })
      });

      await expect(createPaste({ content: '' })).rejects.toThrow('Content is empty');
    });
  });

  describe('getPaste', () => {
    it('sends GET request', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ id: 'xyz', content: 'code' })
      });

      const result = await getPaste('xyz');
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes/xyz', { headers: {} });
      expect(result.content).toBe('code');
    });

    it('sends X-Password header when provided', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ id: 'xyz', content: 'secret' })
      });

      await getPaste('xyz', 'mypass');
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes/xyz', {
        headers: { 'X-Password': 'mypass' }
      });
    });

    it('sets passwordRequired on 403', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: false,
        status: 403,
        json: () => Promise.resolve({ error: 'Password required', password_required: true })
      });

      try {
        await getPaste('xyz');
        expect.unreachable();
      } catch (e) {
        expect(e.passwordRequired).toBe(true);
        expect(e.status).toBe(403);
      }
    });
  });

  describe('listPastes', () => {
    it('sends GET request', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve([{ id: 'a' }, { id: 'b' }])
      });

      const result = await listPastes();
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes');
      expect(result).toHaveLength(2);
    });

    it('adds tag filter to URL', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve([])
      });

      await listPastes('python');
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes?tag=python');
    });
  });

  describe('deletePaste', () => {
    it('sends DELETE request', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ message: 'Deleted' })
      });

      await deletePaste('abc');
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes/abc', { method: 'DELETE' });
    });
  });

  describe('forkPaste', () => {
    it('sends POST with password header', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ id: 'fork123' })
      });

      const result = await forkPaste('orig', { title: 'Fork' }, 'pass');
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes/orig/fork', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'X-Password': 'pass' },
        body: JSON.stringify({ title: 'Fork' })
      });
      expect(result.id).toBe('fork123');
    });
  });

  describe('getRawUrl', () => {
    it('returns correct URL', () => {
      expect(getRawUrl('abc')).toBe('/api/pastes/abc/raw');
    });
  });
});
