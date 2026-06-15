import { create } from 'zustand';

// Shallow compare for arrays and primitives
function shallowEqual(a, b) {
  if (Object.is(a, b)) return true;
  if (Array.isArray(a) && Array.isArray(b)) {
    if (a.length !== b.length) return false;
    for (let i = 0; i < a.length; i++) {
      if (!Object.is(a[i], b[i])) return false;
    }
    return true;
  }
  return false;
}

// Global panel state store — mirrors PyQt PanelStateManager
export const usePanelState = create((set, get) => ({
  values: {},

  get: (key, defaultValue = null) => {
    const v = get().values[key];
    return v !== undefined ? v : defaultValue;
  },

  set: (key, value) => {
    const current = get().values[key];
    // Skip if value hasn't actually changed (prevents unnecessary re-renders)
    if (shallowEqual(current, value)) return;
    set((state) => ({
      values: { ...state.values, [key]: value },
    }));
  },

  has: (key) => {
    return get().values[key] !== undefined;
  },
}));

// Hook to subscribe to a specific key with stable references
export function usePanelValue(key, defaultValue = null) {
  return usePanelState((state) => {
    const v = state.values[key];
    return v !== undefined ? v : defaultValue;
  }, shallowEqual);
}

export function useSetPanelValue() {
  return usePanelState((state) => state.set);
}
