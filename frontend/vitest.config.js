import { defineConfig } from 'vitest/config';
import { svelte } from '@sveltejs/vite-plugin-svelte';

export default defineConfig({
  plugins: [svelte({
    compilerOptions: {
      compatibility: { componentApi: 4 }
    }
  })],
  test: {
    environment: 'jsdom',
    globals: true,
    include: ['src/**/*.{test,spec}.{js,ts}'],
  },
  resolve: {
    conditions: ['browser']
  }
});
