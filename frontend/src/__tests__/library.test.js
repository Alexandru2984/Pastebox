import { describe, it, expect, beforeEach } from 'vitest';
import { rememberPaste, getLibrary, forgetPaste, toggleFavorite, ownerToken } from '../lib/library.js';

describe('Browser library', () => {
  beforeEach(() => localStorage.clear());
  it('persists ownership and metadata without saving content or reading passwords', () => {
    rememberPaste({ id: 'ownerCase', title: 'A title', owner_token: 'a'.repeat(64), content: 'private body', password: 'reading secret' });
    const saved = localStorage.getItem('pastebox-library-v1');
    expect(saved).not.toContain('private body');
    expect(saved).not.toContain('reading secret');
    expect(ownerToken('ownerCase')).toBe('a'.repeat(64));
  });
  it('keeps ownership when a paste is favorited and never deletes a server record', () => {
    rememberPaste({ id: 'favoriteCase', owner_token: 'b'.repeat(64) });
    expect(toggleFavorite({ id: 'favoriteCase', title: 'Saved' })).toBe(true);
    expect(ownerToken('favoriteCase')).toBe('b'.repeat(64));
    forgetPaste('favoriteCase');
    expect(getLibrary()).toEqual([]);
  });
  it('reflects cleared browser data and another tab’s removals', () => {
    rememberPaste({ id: 'clearCase', title: 'Saved' });
    localStorage.clear();
    expect(getLibrary()).toEqual([]);
  });
});
