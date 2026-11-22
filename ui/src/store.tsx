import { createContext, useCallback, useContext, useEffect, useState, type ReactNode } from "react";
import { call, on, type FunctionName } from "./backend";
import type { Meter, Page, State } from "./types";

interface Store {
  state: State;
  page: Page;
  section: string;
  go: (page: Page, section?: string) => void;
  /** Leaves a menu page: back to the practice screen if it was opened from there, else Home. */
  back: () => void;
  backLabel: string;
  /** Calls the app and takes the state it returns; errors show as a toast. */
  act: (name: FunctionName, ...args: unknown[]) => Promise<boolean>;
  error: string;
  clearError: () => void;
}

const StoreContext = createContext<Store | null>(null);

export function useStore(): Store {
  const s = useContext(StoreContext);
  if (!s) throw new Error("useStore outside StoreProvider");
  return s;
}

export function StoreProvider({ children }: { children: ReactNode }) {
  const [state, setState] = useState<State | null>(null);
  const [page, setPage] = useState<Page>("home");
  const [section, setSection] = useState("");
  const [from, setFrom] = useState("");
  const [error, setError] = useState("");
  const clearError = useCallback(() => setError(""), []);

  useEffect(() => {
    const offState = on("state", setState);
    const offNav = on("navigate", ({ page, section, from }) => {
      setPage(page as Page);
      setSection(section);
      setFrom(from ?? "");
    });
    call("ready").then((r) => {
      setState(r.state);
      setPage(r.state.page);
      setSection(r.state.section);
    });
    return () => {
      offState();
      offNav();
    };
  }, []);

  if (!state) return null;

  const store: Store = {
    state,
    page,
    section,
    go: (p, s = "") => {
      setPage(p);
      setSection(s);
      if (p === "home") setFrom("");
    },
    back: () => {
      if (from === "practice" && state.current) {
        call("continuePractice");
        return;
      }
      setFrom("");
      setPage("home");
      setSection("");
    },
    backLabel: from === "practice" && state.current ? "Practice" : "Home",
    act: async (name, ...args) => {
      const r = await call(name, ...args);
      setState(r.state);
      if (r.error) setError(r.error);
      return r.ok;
    },
    error,
    clearError,
  };
  return <StoreContext.Provider value={store}>{children}</StoreContext.Provider>;
}

/** Live input level and heard note (15 updates a second). */
export function useMeter(): Meter {
  const [meter, setMeter] = useState<Meter>({ levelDb: -100, note: "" });
  useEffect(() => on("meter", setMeter), []);
  return meter;
}
