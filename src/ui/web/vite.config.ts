import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";

export default defineConfig({
  plugins: [react(), tailwindcss()],
  build: {
    // Output to {project_root}/web/ — relative to src/ui/web/
    outDir: "../../../web",
    emptyOutDir: true,
  },
  server: {
    proxy: {
      "/api": {
        target: "http://localhost:2009",
        changeOrigin: true,
      },
    },
  },
});
