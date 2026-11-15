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

export async function updatePaste(id, data, password = '', revision = null) {
  const headers = withOwner(id, { 'Content-Type': 'application/json', ...CSRF_HEADERS });
  if (password) headers['X-Password'] = password;
  if (Number.isInteger(revision) && revision > 0) headers['If-Match'] = revisionTag(id, revision);
  const res = await fetch(`${BASE}/api/pastes/${encodeURIComponent(id)}`, {
    method: 'PUT',
    headers,
    body: JSON.stringify(data)
  });
  const json = await res.json();
  if (!res.ok) {
    const err = new Error(json.error || 'Failed to update paste');
    err.status = res.status;
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

export async function restoreOwnerKey(id, token) {
  if (!/^[a-zA-Z0-9]{1,64}$/.test(id) || !/^[a-f0-9]{64}$/.test(token)) throw new Error('Invalid paste ID or owner key');
  const res = await fetch(`/api/pastes/${encodeURIComponent(id)}/ownership`, { headers: { 'X-Owner-Token': token } });
  const json = await res.json();
  if (!res.ok || !json.can_manage) throw new Error(json.error || 'This key does not own the paste');
  const saved = rememberPaste({ ...json, owner_token: token });
  return { ...json, owner_saved: saved };
}

export async function getEmbedPaste(id) {
  const res = await fetch(`/api/pastes/${encodeURIComponent(id)}/embed`);
  const json = await res.json();
  if (!res.ok) throw new Error(json.error || 'This paste cannot be embedded');
  return json;
}

function revisionTag(id, revision) { return `"paste-${id}-v${revision}"`; }
async function revisionRequest(id, path = '', { method = 'GET', revision = null, data } = {}) {
  const headers = withOwner(id);
  if (revision !== null) headers['If-Match'] = revisionTag(id,revision);
  if (data !== undefined) {headers['Content-Type'] = 'application/json';Object.assign(headers,CSRF_HEADERS);}
  if (method === 'DELETE') Object.assign(headers,CSRF_HEADERS);
  const options = {method,headers};
  if (data !== undefined) options.body = JSON.stringify(data);
  const res = await fetch(`/api/pastes/${encodeURIComponent(id)}/revisions${path}`,options);
  const json = await res.json();
  if (!res.ok) throw Object.assign(new Error(json.error || 'History request failed'),{status:res.status});
  return json;
}
export function listRevisions(id) {return revisionRequest(id);}
export function getRevision(id, number) {return revisionRequest(id,`/${encodeURIComponent(number)}`);}
export function restoreRevision(id, number, revision) {return revisionRequest(id,`/${encodeURIComponent(number)}/restore`,{method:'POST',revision,data:{}});}
export function clearRevisions(id, revision) {return revisionRequest(id,'',{method:'DELETE',revision});}
