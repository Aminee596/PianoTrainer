import { X } from "lucide-react";
import { useEffect, useState } from "react";
import { Home } from "./pages/Home";
import { Library } from "./pages/Library";
import { Settings } from "./pages/Settings";
import { Start } from "./pages/Start";
import { StoreProvider, useStore } from "./store";

export default function App() {
  const [started, setStarted] = useState(false);
  return (
    <StoreProvider>
      <Screens />
      {!started && <Start onContinue={() => setStarted(true)} />}
    </StoreProvider>
  );
}

function Screens() {
  const { page } = useStore();
  return (
    <div className="flex h-full flex-col">
      {page === "home" && <Home />}
      {page === "library" && <Library />}
      {page === "settings" && <Settings />}
      <Toast />
    </div>
  );
}

function Toast() {
  const { error, clearError } = useStore();
  useEffect(() => {
    if (!error) return;
    const timer = setTimeout(clearError, 6000);
    return () => clearTimeout(timer);
  }, [error, clearError]);

  if (!error) return null;
  return (
    <div role="alert" className="fixed bottom-6 left-1/2 z-50 flex max-w-[560px] -translate-x-1/2 items-start gap-3 rounded-lg border border-line-strong bg-raised px-4 py-3 shadow-2xl shadow-black/50">
      <div className="caps mt-0.5 text-bad">Error</div>
      <div className="whitespace-pre-line text-ink">{error}</div>
      <button type="button" aria-label="Dismiss" onClick={clearError} className="ml-2 text-ink-3 hover:text-ink">
        <X size={16} />
      </button>
    </div>
  );
}
