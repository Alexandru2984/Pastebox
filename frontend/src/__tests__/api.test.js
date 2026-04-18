import { describe, it, expect, vi, beforeEach } from 'vitest';

// Mock fetch globally
const mockFetch = vi.fn();
global.fetch = mockFetch;

// Must import after mocking fetch
const { createPaste, getPaste, listPastes, deletePaste, forkPaste, updatePaste, getRawUrl } = await import('../lib/api.js');

describe('API Client', () => {
  beforeEach(() => {
    mockFetch.mockReset();
  });

  describe('createPaste', () => {
    it('sends POST with JSON body and CSRF header', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ id: 'abc123', title: 'Test' })
      });

      const result = await createPaste({ title: 'Test', content: 'hello', language: 'plaintext' });

      expect(mockFetch).toHaveBeenCalledWith('/api/pastes', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'X-Requested-With': 'PasteBox' },
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

    it('sets passwordRequired on 404 with password_required', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: false,
        status: 404,
        json: () => Promise.resolve({ error: 'Paste not found', password_required: true })
      });

      try {
        await getPaste('xyz');
        expect.unreachable();
      } catch (e) {
        expect(e.passwordRequired).toBe(true);
        expect(e.status).toBe(404);
      }
    });
  });

  describe('listPastes', () => {
    it('sends GET request with default params', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ data: [{ id: 'a' }, { id: 'b' }], page: 1 })
      });

      const result = await listPastes();
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes');
    });

    it('adds tag filter to URL', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ data: [], page: 1 })
      });

      await listPastes('python');
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes?tag=python');
    });

    it('adds page and limit to URL', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ data: [], page: 2 })
      });

      await listPastes('', 2, 10);
      const calledUrl = mockFetch.mock.calls[0][0];
      expect(calledUrl).toContain('page=2');
      expect(calledUrl).toContain('limit=10');
    });
  });

  describe('updatePaste', () => {
    it('sends PUT with CSRF header', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ id: 'abc', message: 'Paste updated' })
      });

      const result = await updatePaste('abc', { title: 'New' });
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes/abc', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json', 'X-Requested-With': 'PasteBox' },
        body: JSON.stringify({ title: 'New' })
      });
      expect(result.message).toBe('Paste updated');
    });

    it('sends password header when provided', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ id: 'abc' })
      });

      await updatePaste('abc', { title: 'X' }, 'secret');
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes/abc', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json', 'X-Requested-With': 'PasteBox', 'X-Password': 'secret' },
        body: JSON.stringify({ title: 'X' })
      });
    });
  });

  describe('deletePaste', () => {
    it('sends DELETE request with CSRF header', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ message: 'Deleted' })
      });

      await deletePaste('abc');
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes/abc', {
        method: 'DELETE',
        headers: { 'X-Requested-With': 'PasteBox' }
      });
    });

    it('sends password header when provided', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ message: 'Deleted' })
      });

      await deletePaste('abc', 'pw123');
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes/abc', {
        method: 'DELETE',
        headers: { 'X-Requested-With': 'PasteBox', 'X-Password': 'pw123' }
      });
    });
  });

  describe('forkPaste', () => {
    it('sends POST with CSRF and password headers', async () => {
      mockFetch.mockResolvedValueOnce({
        ok: true,
        json: () => Promise.resolve({ id: 'fork123' })
      });

      const result = await forkPaste('orig', { title: 'Fork' }, 'pass');
      expect(mockFetch).toHaveBeenCalledWith('/api/pastes/orig/fork', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'X-Requested-With': 'PasteBox', 'X-Password': 'pass' },
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
