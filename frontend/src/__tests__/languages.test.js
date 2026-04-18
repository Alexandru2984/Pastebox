import { describe, it, expect } from 'vitest';
import { LANGUAGES } from '../lib/languages.js';

describe('Languages', () => {
  it('exports a non-empty array', () => {
    expect(Array.isArray(LANGUAGES)).toBe(true);
    expect(LANGUAGES.length).toBeGreaterThan(20);
  });

  it('includes common languages', () => {
    const expected = ['javascript', 'python', 'typescript', 'rust', 'go', 'java', 'c', 'cpp', 'html', 'css', 'sql', 'bash', 'plaintext'];
    for (const lang of expected) {
      expect(LANGUAGES).toContain(lang);
    }
  });

  it('includes plaintext for default', () => {
    expect(LANGUAGES).toContain('plaintext');
  });

  it('has no duplicates', () => {
    const set = new Set(LANGUAGES);
    expect(set.size).toBe(LANGUAGES.length);
  });

  it('all entries are lowercase strings', () => {
    for (const lang of LANGUAGES) {
      expect(typeof lang).toBe('string');
      expect(lang).toBe(lang.toLowerCase());
    }
  });
});
