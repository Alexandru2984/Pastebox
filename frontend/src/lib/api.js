const BASE = '';

export async function createPaste(data) {
  const res = await fetch(`${BASE}/api/pastes`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
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

export async function listPastes(tag = '') {
  const params = tag ? `?tag=${encodeURIComponent(tag)}` : '';
  const res = await fetch(`${BASE}/api/pastes${params}`);
  if (!res.ok) throw new Error('Failed to load pastes');
  return res.json();
}

export async function deletePaste(id) {
  const res = await fetch(`${BASE}/api/pastes/${id}`, { method: 'DELETE' });
  const json = await res.json();
  if (!res.ok) throw new Error(json.error || 'Failed to delete');
  return json;
}

export async function forkPaste(id, data = {}, password = '') {
  const headers = { 'Content-Type': 'application/json' };
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
