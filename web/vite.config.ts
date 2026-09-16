import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  server: {
    // Listen on the LAN: /pair is opened from the phone that reads the OLED.
    host: true,
    // High ports on purpose: nothing else on the machine tends to claim them.
    port: 43111,
    strictPort: true,
    // The display shows the machine's Bonjour name, not an IP.
    allowedHosts: ['.local'],
    proxy: {
      '/api': {
        target: 'http://localhost:43110',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/api/, ''),
      },
    },
  },
})
