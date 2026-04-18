const BASE = '';

export async function createPaste(data) {
  const res = await fetch(`${BASE}/api/pastes`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  });
  if (!res.ok) throw new Error(await res.text());
  return res.json();
}

export async function getPaste(id) {
  const res = await fetch(`${BASE}/api/pastes/${id}`);
  if (!res.ok) throw new Error(await res.text());
  return res.json();
}

export async function listPastes() {
  const res = await fetch(`${BASE}/api/pastes`);
  if (!res.ok) throw new Error(await res.text());
  return res.json();
}

export async function deletePaste(id) {
  const res = await fetch(`${BASE}/api/pastes/${id}`, { method: 'DELETE' });
  if (!res.ok) throw new Error(await res.text());
  return res.json();
}
