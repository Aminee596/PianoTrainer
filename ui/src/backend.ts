// Talks to the C++ side through the bridge JUCE injects into the web view
// (window.__JUCE__). Opened in a normal browser (npm run dev), a mock backend
// with sample data stands in, so the screens can be designed without the app.

import type { Meter, Reply, State } from "./types";
import { createMock } from "./mock";

type Listener = (payload: any) => void;

interface JuceBackend {
  addEventListener(eventId: string, fn: Listener): unknown;
  removeEventListener(handle: unknown): void;
  emitEvent(eventId: string, payload: unknown): void;
}

const juce = (window as any).__JUCE__ as
  | { backend?: JuceBackend; initialisationData?: { __juce__functions?: string[] } }
  | undefined;

export const isNative = !!juce?.backend && Array.isArray(juce.initialisationData?.__juce__functions);

/** Names of the native functions (WebMenu.cpp registers the same list). */
export type FunctionName =
  | "ready" | "openPiece" | "continuePractice" | "addPieces" | "importFiles" | "toggleFavourite"
  | "removePiece" | "removeMissing" | "removeDuplicates" | "updateSettings" | "resetSettings" | "setDriver" | "setInput" | "reloadPiano" | "setPianoSound" | "openSoundsFolder" | "openLogFolder"
  | "startCalibration" | "calibrationBack" | "stopCalibration" | "clearCalibration";

export interface Events { state: State; meter: Meter; navigate: { page: string; section: string; from?: string } }

const mock = isNative ? null : createMock();

let nextId = 1;
const pending = new Map<number, (result: Reply) => void>();

if (isNative) {
  juce!.backend!.addEventListener("__juce__complete", ({ promiseId, result }: { promiseId: number; result: Reply }) => {
    pending.get(promiseId)?.(result);
    pending.delete(promiseId);
  });
}

export function call(name: FunctionName, ...args: unknown[]): Promise<Reply> {
  if (!isNative) return mock!.call(name, args);
  const id = nextId++;
  return new Promise((resolve) => {
    pending.set(id, resolve);
    juce!.backend!.emitEvent("__juce__invoke", { name, params: args, resultId: id });
  });
}

/** Subscribes to an event from the app; returns the unsubscribe function. */
export function on<K extends keyof Events>(event: K, fn: (payload: Events[K]) => void): () => void {
  if (!isNative) return mock!.on(event, fn as Listener);
  const handle = juce!.backend!.addEventListener(event, fn as Listener);
  return () => juce!.backend!.removeEventListener(handle);
}
