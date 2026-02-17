/**
 * Tauri command bindings for Always on Top functionality
 */
import { invoke } from '@tauri-apps/api/core';
import type { PinnedWindow } from './types';

/** Toggle pin state on the foreground window */
export async function togglePinForeground(): Promise<boolean> {
    return invoke<boolean>('toggle_pin_foreground');
}

/** Pin a specific window by its handle */
export async function pinWindow(hwnd: number): Promise<boolean> {
    return invoke<boolean>('pin_window', { hwnd });
}

/** Unpin a specific window by its handle */
export async function unpinWindow(hwnd: number): Promise<boolean> {
    return invoke<boolean>('unpin_window', { hwnd });
}

/** Get list of all pinned windows */
export async function getPinnedWindows(): Promise<PinnedWindow[]> {
    return invoke<PinnedWindow[]>('get_pinned_windows');
}

/** Adjust opacity of foreground window */
export async function adjustOpacity(delta: number): Promise<number> {
    return invoke<number>('adjust_opacity', { delta });
}

/** Set opacity of a specific pinned window */
export async function setWindowOpacity(hwnd: number, percent: number): Promise<void> {
    return invoke<void>('set_window_opacity', { hwnd, percent });
}

/** Check if a window is currently topmost */
export async function isWindowTopmost(hwnd: number): Promise<boolean> {
    return invoke<boolean>('is_window_topmost', { hwnd });
}

/** Bring a pinned window to focus */
export async function focusWindow(hwnd: number): Promise<void> {
    return invoke<void>('focus_window', { hwnd });
}

/** Get count of pinned windows */
export async function getPinnedCount(): Promise<number> {
    return invoke<number>('get_pinned_count');
}

/** Check if auto-start is enabled */
export async function getAutoStart(): Promise<boolean> {
    return invoke<boolean>('get_auto_start');
}

/** Enable or disable auto-start */
export async function setAutoStart(enabled: boolean): Promise<void> {
    return invoke<void>('set_auto_start', { enabled });
}

/** Get sound enabled setting */
export async function getSoundEnabled(): Promise<boolean> {
    return invoke<boolean>('get_sound_enabled');
}

/** Set sound enabled setting */
export async function setSoundEnabled(enabled: boolean): Promise<void> {
    return invoke<void>('set_sound_enabled', { enabled });
}

/** Get whether user has seen the tray notice */
export async function getHasSeenTrayNotice(): Promise<boolean> {
    return invoke<boolean>('get_has_seen_tray_notice');
}

/** Mark tray notice as seen */
export async function setHasSeenTrayNotice(): Promise<void> {
    return invoke<void>('set_has_seen_tray_notice');
}
