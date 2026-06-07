/**
 * TypeScript types mirroring Rust backend structs
 */

/** Information about a pinned window */
export interface PinnedWindow {
  /** Window handle (as number) */
  hwnd: number;
  /** Window title at time of pinning */
  title: string;
  /** Process name */
  process_name: string;
  /** Current opacity (0-255) */
  opacity: number;
  /** Original opacity before modification */
  original_opacity: number | null;
}

/** A window eligible for pinning (matches Rust PinnableWindow) */
export interface PinnableWindow {
  hwnd: number;
  title: string;
  process_name: string;
}

/** Keyboard shortcut configuration (matches Rust ShortcutConfig) */
export interface ShortcutConfig {
  toggle_pin: string;
  opacity_up: string;
  opacity_down: string;
  toggle_window: string;
}

/** Human-readable labels for each shortcut action */
export const SHORTCUT_LABELS: Record<keyof ShortcutConfig, string> = {
  toggle_pin: 'Pin/Unpin',
  opacity_up: 'Opacity +',
  opacity_down: 'Opacity -',
  toggle_window: 'Show/Hide',
};

/** Backend event names — must match src-tauri/src/events.rs */
export const EVENTS = {
  PIN_TOGGLED: 'pin-toggled',
  PIN_ERROR: 'pin-error',
  OPACITY_CHANGED: 'opacity-changed',
  WINDOW_DESTROYED: 'window-destroyed',
  SHORTCUTS_UPDATED: 'shortcuts-updated',
} as const;

/** Payload of the pin-toggled backend event */
export interface PinToggledPayload {
  is_pinned: boolean;
  title: string;
  process_name: string;
}

/** A toast notification entry */
export interface ToastData {
  id: number;
  message: string;
  type: 'pin' | 'unpin' | 'error';
}
