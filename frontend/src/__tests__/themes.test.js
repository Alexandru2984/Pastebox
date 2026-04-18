import { describe, it, expect } from 'vitest';
import { themes, applyTheme, getStoredTheme, initTheme } from '../lib/themes.js';

describe('Themes', () => {
  it('has 4 themes', () => {
    expect(Object.keys(themes)).toHaveLength(4);
    expect(themes).toHaveProperty('dark');
    expect(themes).toHaveProperty('light');
    expect(themes).toHaveProperty('monokai');
    expect(themes).toHaveProperty('dracula');
  });

  it('each theme has required properties', () => {
    const required = ['name', 'bgPrimary', 'bgSecondary', 'bgTertiary', 'border',
      'textPrimary', 'textSecondary', 'accent', 'accentHover', 'green', 'red'];
    for (const [key, theme] of Object.entries(themes)) {
      for (const prop of required) {
        expect(theme).toHaveProperty(prop);
      }
    }
  });

  it('getStoredTheme defaults to dark', () => {
    localStorage.clear();
    expect(getStoredTheme()).toBe('dark');
  });

  it('applyTheme sets CSS variables and saves to localStorage', () => {
    applyTheme('dracula');
    const root = document.documentElement;
    expect(root.style.getPropertyValue('--bg-primary')).toBe('#282a36');
    expect(root.style.getPropertyValue('--accent')).toBe('#bd93f9');
    expect(localStorage.getItem('pastebox-theme')).toBe('dracula');
  });

  it('applyTheme falls back to dark for unknown theme', () => {
    applyTheme('nonexistent');
    const root = document.documentElement;
    expect(root.style.getPropertyValue('--bg-primary')).toBe('#0d1117');
  });

  it('initTheme applies stored theme', () => {
    localStorage.setItem('pastebox-theme', 'monokai');
    initTheme();
    expect(document.documentElement.style.getPropertyValue('--bg-primary')).toBe('#272822');
  });
});
