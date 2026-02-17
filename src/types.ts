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

/** Keyboard shortcut configuration (matches Rust ShortcutConfig) */
export interface ShortcutConfig {
  toggle_pin: string;
  opacity_up: string;
  opacity_down: string;
  toggle_window: string;
}

/** Default shortcut values */
export const DEFAULT_SHORTCUTS: ShortcutConfig = {
  toggle_pin: 'super+ctrl+KeyT',
  opacity_up: 'super+ctrl+Equal',
  opacity_down: 'super+ctrl+Minus',
  toggle_window: 'super+ctrl+KeyP',
};

/** Human-readable labels for each shortcut action */
export const SHORTCUT_LABELS: Record<keyof ShortcutConfig, string> = {
  toggle_pin: 'Pin/Unpin',
  opacity_up: 'Opacity +',
  opacity_down: 'Opacity -',
  toggle_window: 'Show/Hide',
};
