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
