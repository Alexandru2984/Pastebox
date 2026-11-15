const KEY = 'pastebox-library-v1';
const memory = new Map();

export function getLibrary() {
  try {
    const saved = JSON.parse(localStorage.getItem(KEY) || '[]');
    if (Array.isArray(saved)) {
      for (const item of saved) {
        if (item && typeof item.id === 'string' && /^[a-zA-Z0-9]{1,64}$/.test(item.id)) memory.set(item.id, item);
      }
    }
  } catch { /* Browser storage can be disabled. The current session still works. */ }
  return [...memory.values()].sort((a, b) => (b.saved_at || '').localeCompare(a.saved_at || ''));
}
function persist() {
  try {
    localStorage.setItem(KEY, JSON.stringify([...memory.values()]));
    return true;
  } catch { return false; }
}
export function rememberPaste(paste) {
  getLibrary();
  const previous = memory.get(paste.id) || {};
  memory.set(paste.id, {
    ...previous, id: paste.id, title: paste.title || 'Untitled', language: paste.language || 'plaintext',
    visibility: paste.visibility || 'public', saved_at: new Date().toISOString(),
    ...(paste.owner_token ? { owner_token: paste.owner_token } : {})
  });
  return persist();
}
export function ownerToken(id) { return getLibrary().find(item => item.id === id)?.owner_token || ''; }
export function forgetPaste(id) { getLibrary(); memory.delete(id); return persist(); }
export function isFavorite(id) { return Boolean(getLibrary().find(item => item.id === id)?.favorite); }
export function toggleFavorite(paste) {
  rememberPaste(paste);
  const item = memory.get(paste.id); item.favorite = !item.favorite; persist(); return item.favorite;
}
export function importOwnerKey(id, token) {
  if (!/^[a-zA-Z0-9]{1,64}$/.test(id) || !/^[a-f0-9]{64}$/.test(token)) throw new Error('Invalid owner key');
  return rememberPaste({ id, owner_token: token });
}
