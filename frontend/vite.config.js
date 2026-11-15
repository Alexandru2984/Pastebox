import { defineConfig } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'
import tailwindcss from '@tailwindcss/vite'

export default defineConfig({
  plugins: [svelte(), tailwindcss()],
  preview: { host: '127.0.0.1' },
  server: {
    host: '127.0.0.1',
    proxy: {
      '/api': 'http://127.0.0.1:7777'
    }
  }
})
