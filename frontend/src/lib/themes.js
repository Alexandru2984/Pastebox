export const themes = {
  dark: {
    name: 'Dark',
    bgPrimary: '#0d1117',
    bgSecondary: '#161b22',
    bgTertiary: '#21262d',
    border: '#30363d',
    textPrimary: '#e6edf3',
    textSecondary: '#8b949e',
    accent: '#58a6ff',
    accentHover: '#79c0ff',
    green: '#3fb950',
    red: '#f85149',
    hljsTheme: 'github-dark'
  },
  light: {
    name: 'Light',
    bgPrimary: '#ffffff',
    bgSecondary: '#f6f8fa',
    bgTertiary: '#e1e4e8',
    border: '#d0d7de',
    textPrimary: '#1f2328',
    textSecondary: '#656d76',
    accent: '#0969da',
    accentHover: '#0550ae',
    green: '#1a7f37',
    red: '#cf222e',
    hljsTheme: 'github'
  },
  monokai: {
    name: 'Monokai',
    bgPrimary: '#272822',
    bgSecondary: '#1e1f1c',
    bgTertiary: '#3e3d32',
    border: '#49483e',
    textPrimary: '#f8f8f2',
    textSecondary: '#75715e',
    accent: '#a6e22e',
    accentHover: '#b6f23e',
    green: '#a6e22e',
    red: '#f92672',
    hljsTheme: 'monokai'
  },
  dracula: {
    name: 'Dracula',
    bgPrimary: '#282a36',
    bgSecondary: '#21222c',
    bgTertiary: '#343746',
    border: '#44475a',
    textPrimary: '#f8f8f2',
    textSecondary: '#6272a4',
    accent: '#bd93f9',
    accentHover: '#caa8fc',
    green: '#50fa7b',
    red: '#ff5555',
    hljsTheme: 'dracula'
  }
};

export function applyTheme(themeName) {
  const theme = themes[themeName] || themes.dark;
  const root = document.documentElement;
  root.style.setProperty('--bg-primary', theme.bgPrimary);
  root.style.setProperty('--bg-secondary', theme.bgSecondary);
  root.style.setProperty('--bg-tertiary', theme.bgTertiary);
  root.style.setProperty('--border', theme.border);
  root.style.setProperty('--text-primary', theme.textPrimary);
  root.style.setProperty('--text-secondary', theme.textSecondary);
  root.style.setProperty('--accent', theme.accent);
  root.style.setProperty('--accent-hover', theme.accentHover);
  root.style.setProperty('--green', theme.green);
  root.style.setProperty('--red', theme.red);
  localStorage.setItem('pastebox-theme', themeName);
}

export function getStoredTheme() {
  return localStorage.getItem('pastebox-theme') || 'dark';
}

export function initTheme() {
  applyTheme(getStoredTheme());
}
