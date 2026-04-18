const BASE = '';

const CSRF_HEADERS = { 'X-Requested-With': 'PasteBox' };

export async function createPaste(data) {
  const res = await fetch(`${BASE}/api/pastes`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', ...CSRF_HEADERS },
    body: JSON.stringify(data)
  });
  const json = await res.json();
  if (!res.ok) throw new Error(json.error || 'Failed to create paste');
  return json;
}

export async function getPaste(id, password = '') {
  const headers = {};
  if (password) headers['X-Password'] = password;
  const res = await fetch(`${BASE}/api/pastes/${id}`, { headers });
  const json = await res.json();
  if (!res.ok) {
    const err = new Error(json.error || 'Failed to get paste');
    err.passwordRequired = json.password_required || false;
    err.status = res.status;
    throw err;
  }
  return json;
}

export async function listPastes(tag = '', page = 1, limit = 50) {
  const params = new URLSearchParams();
  if (tag) params.set('tag', tag);
  if (page > 1) params.set('page', String(page));
  if (limit !== 50) params.set('limit', String(limit));
  const qs = params.toString();
  const res = await fetch(`${BASE}/api/pastes${qs ? '?' + qs : ''}`);
  if (!res.ok) throw new Error('Failed to load pastes');
  return res.json();
}

export async function updatePaste(id, data, password = '') {
  const headers = { 'Content-Type': 'application/json', ...CSRF_HEADERS };
  if (password) headers['X-Password'] = password;
  const res = await fetch(`${BASE}/api/pastes/${id}`, {
    method: 'PUT',
    headers,
    body: JSON.stringify(data)
  });
  const json = await res.json();
  if (!res.ok) {
    const err = new Error(json.error || 'Failed to update paste');
    err.passwordRequired = json.password_required || false;
    throw err;
  }
  return json;
}

export async function deletePaste(id, password = '') {
  const headers = { ...CSRF_HEADERS };
  if (password) headers['X-Password'] = password;
  const res = await fetch(`${BASE}/api/pastes/${id}`, { method: 'DELETE', headers });
  const json = await res.json();
  if (!res.ok) throw new Error(json.error || 'Failed to delete');
  return json;
}

export async function forkPaste(id, data = {}, password = '') {
  const headers = { 'Content-Type': 'application/json', ...CSRF_HEADERS };
  if (password) headers['X-Password'] = password;
  const res = await fetch(`${BASE}/api/pastes/${id}/fork`, {
    method: 'POST',
    headers,
    body: JSON.stringify(data)
  });
  const json = await res.json();
  if (!res.ok) throw new Error(json.error || 'Failed to fork paste');
  return json;
}

export function getRawUrl(id) {
  return `${BASE}/api/pastes/${id}/raw`;
}
