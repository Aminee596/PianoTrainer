import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import App from "./App";
import "./index.css";

// Dropping a file anywhere must never navigate the web view away from the app.
for (const type of ["dragover", "drop"])
  window.addEventListener(type, (e) => e.preventDefault());

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
