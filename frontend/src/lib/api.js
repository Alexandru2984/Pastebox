import { ownerToken, rememberPaste, forgetPaste } from './library.js';

const BASE = '';

function withOwner(id, headers = {}) {
  const token = ownerToken(id);
  return token ? { ...headers, 'X-Owner-Token': token } : headers;
}

const CSRF_HEADERS = { 'X-Requested-With': 'PasteBox' };

export async function createPaste(data) {
  const res = await fetch(`${BASE}/api/pastes`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', ...CSRF_HEADERS },
    body: JSON.stringify(data)
  });
  const json = await res.json();
  if (!res.ok) throw new Error(json.error || 'Failed to create paste');
  if (json.owner_token) json.owner_saved = rememberPaste(json);
  return json;
}

export async function getPaste(id, password = '', confirmBurn = false) {
  const headers = withOwner(id);
  if (confirmBurn) headers['X-Confirm-Burn'] = 'yes';
  if (password) headers['X-Password'] = password;
  const res = await fetch(`${BASE}/api/pastes/${encodeURIComponent(id)}`, { headers });
  const json = await res.json();
  if (!res.ok) {
    const err = new Error(json.error || 'Failed to get paste');
    err.passwordRequired = json.password_required || false;
    err.ownerRequired = json.owner_required || false;
    err.burnRequired = json.burn_confirmation_required || false;
    err.status = res.status;
    throw err;
  }
  return json;
}

export async function listPastes(tag = '', page = 1, limit = 50, filters = {}) {
  const params = new URLSearchParams();
  if (tag) params.set('tag', tag);
  if (page > 1) params.set('page', String(page));
  if (limit !== 50) params.set('limit', String(limit));
  for (const key of ['q', 'language', 'sort']) if (filters[key]) params.set(key, filters[key]);
  const qs = params.toString();
  const res = await fetch(`${BASE}/api/pastes${qs ? '?' + qs : ''}`);
  if (!res.ok) throw new Error('Failed to load pastes');
  return res.json();
}

export async function updatePaste(id, data, password = '') {
  const headers = withOwner(id, { 'Content-Type': 'application/json', ...CSRF_HEADERS });
  if (password) headers['X-Password'] = password;
  const res = await fetch(`${BASE}/api/pastes/${encodeURIComponent(id)}`, {
    method: 'PUT',
    headers,
    body: JSON.stringify(data)
  });
  const json = await res.json();
  if (!res.ok) {
    const err = new Error(json.error || 'Failed to update paste');
    err.passwordRequired = json.password_required || false;
    err.ownerRequired = json.owner_required || false;
    err.burnRequired = json.burn_confirmation_required || false;
    throw err;
  }
  return json;
}

export async function deletePaste(id, password = '') {
  const headers = withOwner(id, { ...CSRF_HEADERS });
  if (password) headers['X-Password'] = password;
  const res = await fetch(`${BASE}/api/pastes/${encodeURIComponent(id)}`, { method: 'DELETE', headers });
  const json = await res.json();
  if (!res.ok) throw new Error(json.error || 'Failed to delete');
  forgetPaste(id);
  return json;
}

export async function forkPaste(id, data = {}, password = '') {
  const headers = withOwner(id, { 'Content-Type': 'application/json', ...CSRF_HEADERS });
  if (password) headers['X-Password'] = password;
  const res = await fetch(`${BASE}/api/pastes/${encodeURIComponent(id)}/fork`, {
    method: 'POST',
    headers,
    body: JSON.stringify(data)
  });
  const json = await res.json();
  if (!res.ok) throw new Error(json.error || 'Failed to fork paste');
  if (json.owner_token) json.owner_saved = rememberPaste(json);
  return json;
}

export function getRawUrl(id) {
  return `${BASE}/api/pastes/${encodeURIComponent(id)}/raw`;
}

export async function getRawPaste(id, password = '') {
  const headers = withOwner(id);
  if (password) headers['X-Password'] = password;
  const res = await fetch(getRawUrl(id), { headers });
  if (!res.ok) throw new Error('Raw content is unavailable');
  return res.text();
}
