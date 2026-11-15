import { describe, it, expect } from 'vitest';
import { diffLines } from '../lib/diff.js';

describe('Bounded line comparison', () => {
  it.each([
    ['a\nb\nc','a\nchanged\nc'],
    ['repeat\nrepeat\nlast','repeat\nlast\nrepeat'],
    ['','hello'],
    ['hello\n','hello'],
    ['x\ny','x\ny'],
    ['<script>alert(1)</script>','safe'],
    ['one\ntwo','one\nnew\ntwo\nlast']
  ])('reconstructs both versions without dropping repeated or trailing lines', (before,after) => {
    const result = diffLines(before,after);
    expect(result.mode).toBe('diff');
    expect(result.rows.filter(row => row.type !== 'added').map(row => row.text).join('\n')).toBe(before);
    expect(result.rows.filter(row => row.type !== 'removed').map(row => row.text).join('\n')).toBe(after);
    expect(result.rows.filter(row => row.left !== null).map(row => row.left)).toEqual(before.split('\n').map((_,i) => i+1));
    expect(result.rows.filter(row => row.right !== null).map(row => row.right)).toEqual(after.split('\n').map((_,i) => i+1));
  });
  it('falls back before allocating quadratic work for unrelated large snippets', () => {
    expect(diffLines(Array(600).fill('old').join('\n'),Array(600).fill('new').join('\n')).mode).toBe('large');
    expect(diffLines('a'.repeat(200001),'x').mode).toBe('large');
  });
  it('can compare a small change inside a longer mostly identical snippet', () => {
    const lines = Array.from({length:1100},(_,i) => `line ${i}`);
    const before = lines.join('\n'); lines[550] = 'changed';
    const result = diffLines(before,lines.join('\n'));
    expect(result.mode).toBe('diff');
    expect(result.rows.filter(row => row.type === 'added')).toHaveLength(1);
    expect(result.rows.filter(row => row.type === 'removed')).toHaveLength(1);
  });
});
