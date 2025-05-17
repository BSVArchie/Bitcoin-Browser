import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [
    react({
      include: "**/*.{jsx,tsx,js,ts}",
    }),
  ],

  clearScreen: false,

  server: {
    open: false,
    host: "localhost", // ✅ Force localhost to work with CEF
    port: 5137,
    strictPort: true,
    cors: true, // ✅ Enable CORS for embedded shell requests
    watch: {
      ignored: ["**/src-tauri/**"],
    },
  },

  resolve: {
    extensions: [".mjs", ".js", ".ts", ".jsx", ".tsx", ".json"],
    preserveSymlinks: true,
  },
});
