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

/** Application settings */
export interface AppSettings {
  /** Hotkey configuration */
  hotkey: HotkeyConfig;
  /** Enable border overlay */
  enableBorder: boolean;
  /** Enable sound effects */
  enableSound: boolean;
  /** Border color (hex) */
  borderColor: string;
  /** Border thickness in pixels */
  borderThickness: number;
  /** Border opacity (0-100) */
  borderOpacity: number;
  /** List of excluded app names */
  excludedApps: string[];
}

/** Hotkey configuration */
export interface HotkeyConfig {
  win: boolean;
  ctrl: boolean;
  alt: boolean;
  shift: boolean;
  key: string;
}

/** Default settings */
export const DEFAULT_SETTINGS: AppSettings = {
  hotkey: {
    win: true,
    ctrl: true,
    alt: false,
    shift: false,
    key: 'T',
  },
  enableBorder: true,
  enableSound: true,
  borderColor: '#00ADEF',
  borderThickness: 3,
  borderOpacity: 100,
  excludedApps: [],
};
