import { test as base, expect } from '@playwright/test';
import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { setTimeout as delay } from 'node:timers/promises';

const preview = fileURLToPath(new URL('../../scripts/preview.py', import.meta.url));
const binary = path.resolve(process.env.PASTEBOX_TEST_BINARY || '/tmp/pastebox-audit-build/backend');
const publicDir = path.resolve(process.env.PASTEBOX_TEST_PUBLIC || '/tmp/pastebox-audit-public');

// Isolate process-wide quotas as well as data. Every test uses the production
// throttles, with no retries, quota bypass or dependency on a previous test.
export const test = base.extend({
  baseURL: async ({}, use, testInfo) => {
    const server = spawn('python3', [preview, '--binary', binary, '--public', publicDir], {
      detached: true,
      stdio: ['ignore', 'pipe', 'pipe']
    });
    let logs = '';
    const capture = data => { logs = (logs + data).slice(-64 * 1024); };
    server.stdout.on('data', capture);
    server.stderr.on('data', capture);
    const lines = createInterface({ input: server.stdout });
    const exited = new Promise(resolve => server.once('exit', resolve));
    // Register error handling before waiting for startup, including spawn errors.
    const origin = new Promise((resolve, reject) => {
      lines.on('line', line => {
        const match = line.match(/^Preview: (http:\/\/127\.0\.0\.1:\d+)$/);
        if (match) resolve(match[1]);
      });
      server.once('error', reject);
      server.once('exit', code => reject(new Error(`Test backend exited (${code}).\n${logs}`)));
    });
    let startupTimer;
    try {
      const url = await Promise.race([
        origin,
        new Promise((_, reject) => { startupTimer = setTimeout(() => reject(new Error(`Test backend startup timed out.\n${logs}`)), 15000); })
      ]);
      clearTimeout(startupTimer);
      const deadline = Date.now() + 15000;
      while (true) {
        try {
          const response = await fetch(`${url}/api/health`, { signal: AbortSignal.timeout(1000) });
          if (response.ok && (await response.json()).status === 'ok') break;
        } catch { /* Readiness probe only; test operations themselves never retry. */ }
        if (server.exitCode !== null || Date.now() >= deadline) throw new Error(`Test backend is not ready.\n${logs}`);
        await delay(50);
      }
      await use(url);
    } finally {
      clearTimeout(startupTimer);
      lines.close();
      // Kill the group so even an interrupted preview cannot leave its backend.
      if (server.pid) {
        try { process.kill(-server.pid, 'SIGTERM'); } catch (error) { if (error.code !== 'ESRCH') throw error; }
        let shutdownTimer;
        try {
          await Promise.race([exited, new Promise(resolve => { shutdownTimer = setTimeout(resolve, 5000); })]);
          if (server.exitCode === null && server.signalCode === null) {
            try { process.kill(-server.pid, 'SIGKILL'); } catch (error) { if (error.code !== 'ESRCH') throw error; }
            await exited;
          }
        } finally { clearTimeout(shutdownTimer); }
      }
      if (testInfo.status !== testInfo.expectedStatus && logs) console.error(logs);
    }
  }
});

export { expect };
