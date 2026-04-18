import { writable } from 'svelte/store';

export const toasts = writable([]);

let nextId = 0;

export function addToast(message, type = 'info', duration = 3000) {
  const id = nextId++;
  toasts.update(t => [...t, { id, message, type }]);
  if (duration > 0) {
    setTimeout(() => removeToast(id), duration);
  }
  return id;
}

export function removeToast(id) {
  toasts.update(t => t.filter(toast => toast.id !== id));
}

export function success(msg) { return addToast(msg, 'success'); }
export function error(msg) { return addToast(msg, 'error', 5000); }
export function info(msg) { return addToast(msg, 'info'); }
