import { defineConfig, devices } from '@playwright/test';
import path from 'node:path';
const binary = process.env.PASTEBOX_TEST_BINARY || '/tmp/pastebox-audit-build/backend';
const publicDir = process.env.PASTEBOX_TEST_PUBLIC || '/tmp/pastebox-audit-public';
export default defineConfig({
  testDir: './e2e',
  fullyParallel: false,
  workers: 1,
  timeout: 30000,
  use: {
    baseURL: 'http://127.0.0.1:19779',
    trace: 'retain-on-failure',
    launchOptions: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE ? { executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE } : {}
  },
  projects: [{ name:'desktop',use:{...devices['Desktop Chrome']} },{ name:'mobile',use:{...devices['Pixel 7']} }],
  webServer: {
    command: `python3 ../scripts/preview.py --binary '${path.resolve(binary).replace(/'/g,"'\\''")}' --public '${path.resolve(publicDir).replace(/'/g,"'\\''")}' --port 19779`,
    url: 'http://127.0.0.1:19779/api/health',
    reuseExistingServer: false,
    timeout: 20000
  }
});
