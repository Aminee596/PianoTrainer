import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";
import { viteSingleFile } from "vite-plugin-singlefile";

// One self-contained index.html (scripts, styles and fonts inlined): the app
// embeds it and serves it to the web view, see src/app/WebMenu.cpp.
export default defineConfig({
  plugins: [react(), tailwindcss(), viteSingleFile()],
  server: { port: 5173, strictPort: true },
  build: { outDir: "dist", emptyOutDir: true },
});
