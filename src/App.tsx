import { useState, useEffect, useCallback, useRef } from 'react';
import { listen } from '@tauri-apps/api/event';
import { Window } from '@tauri-apps/api/window';
import { getPinnedWindows, pinWindow, unpinWindow, setWindowOpacity, focusWindow, getAutoStart, setAutoStart, getSoundEnabled, setSoundEnabled, getHasSeenTrayNotice, setHasSeenTrayNotice, getShortcutConfig, setShortcutConfig, resetShortcutConfig } from './commands';
import type { PinnedWindow, PinToggledPayload, ShortcutConfig, ToastData } from './types';
import { EVENTS, SHORTCUT_LABELS } from './types';
import { keyEventToShortcutString } from './shortcutUtils';
import { playSound } from './utils/sound';
import { Titlebar } from './components/Titlebar';
import { ShortcutsPopover } from './components/ShortcutsPopover';
import { ShortcutsReference } from './components/ShortcutsReference';
import { WindowList } from './components/WindowList';
import { SettingsSection } from './components/SettingsSection';
import { ToastStack } from './components/ToastStack';
import { TrayNotice } from './components/TrayNotice';
import { AddWindowPicker } from './components/AddWindowPicker';
import './App.css';

let toastId = 0;

function App() {
  const [pinnedWindows, setPinnedWindows] = useState<PinnedWindow[]>([]);
  const [shortcutsOpen, setShortcutsOpen] = useState(false);
  const [toasts, setToasts] = useState<ToastData[]>([]);
  const [autoStart, setAutoStartState] = useState(false);
  const [soundEnabled, setSoundEnabledState] = useState(true);
  const soundEnabledRef = useRef(true);
  const [showTrayNotice, setShowTrayNotice] = useState(false);
  const [pickerOpen, setPickerOpen] = useState(false);
  const [shortcuts, setShortcuts] = useState<ShortcutConfig | null>(null);
  const [editingKey, setEditingKey] = useState<keyof ShortcutConfig | null>(null);
  const [captureValue, setCaptureValue] = useState<string | null>(null);
  const toastTimeouts = useRef<Map<number, ReturnType<typeof setTimeout>>>(new Map());
  const opacityFlush = useRef<Map<number, { timer: ReturnType<typeof setTimeout>; latest: number }>>(new Map());
  const popoverRef = useRef<HTMLDivElement | null>(null);

  const addToast = useCallback((message: string, type: ToastData['type']) => {
    const id = ++toastId;
    setToasts((prev) => [...prev.slice(-2), { id, message, type }]); // Keep max 3

    const timeout = setTimeout(() => {
      setToasts((prev) => prev.filter((t) => t.id !== id));
      toastTimeouts.current.delete(id);
    }, 1500);
    toastTimeouts.current.set(id, timeout);
  }, []);

  useEffect(() => {
    refreshPinnedWindows();
    getAutoStart().then(setAutoStartState).catch(() => {});
    getSoundEnabled().then((v) => { setSoundEnabledState(v); soundEnabledRef.current = v; }).catch(() => {});
    getShortcutConfig().then(setShortcuts).catch(() => {});

    const unlistenPin = listen<PinToggledPayload>(EVENTS.PIN_TOGGLED, (event) => {
      refreshPinnedWindows();
      const { is_pinned, title, process_name } = event.payload;
      const name = title && title !== 'Unknown' ? title : process_name;
      const truncated = name.length > 30 ? name.slice(0, 30) + '...' : name;
      addToast(
        is_pinned ? `Pinned: ${truncated}` : `Unpinned: ${truncated}`,
        is_pinned ? 'pin' : 'unpin'
      );
      if (soundEnabledRef.current) playSound(is_pinned);
    });

    const unlistenOpacity = listen<number>(EVENTS.OPACITY_CHANGED, () => {
      refreshPinnedWindows();
    });

    const unlistenDestroyed = listen(EVENTS.WINDOW_DESTROYED, () => {
      refreshPinnedWindows();
    });

    const unlistenError = listen<string>(EVENTS.PIN_ERROR, (event) => {
      addToast(event.payload, 'error');
    });

    const unlistenShortcuts = listen(EVENTS.SHORTCUTS_UPDATED, () => {
      getShortcutConfig().then(setShortcuts).catch(() => {});
    });

    return () => {
      unlistenPin.then((fn) => fn());
      unlistenOpacity.then((fn) => fn());
      unlistenDestroyed.then((fn) => fn());
      unlistenError.then((fn) => fn());
      unlistenShortcuts.then((fn) => fn());
      toastTimeouts.current.forEach((t) => clearTimeout(t));
      opacityFlush.current.forEach((entry) => clearTimeout(entry.timer));
    };
  }, [addToast]);

  // Close the shortcuts popover on Escape or a click outside it
  useEffect(() => {
    if (!shortcutsOpen) return;

    function onKeyDown(e: KeyboardEvent) {
      if (e.key === 'Escape') setShortcutsOpen(false);
    }
    function onMouseDown(e: MouseEvent) {
      if (popoverRef.current && !popoverRef.current.contains(e.target as Node)) {
        setShortcutsOpen(false);
      }
    }
    document.addEventListener('keydown', onKeyDown);
    document.addEventListener('mousedown', onMouseDown);
    return () => {
      document.removeEventListener('keydown', onKeyDown);
      document.removeEventListener('mousedown', onMouseDown);
    };
  }, [shortcutsOpen]);

  async function refreshPinnedWindows() {
    try {
      const windows = await getPinnedWindows();
      setPinnedWindows(windows);
    } catch (err) {
      console.error('Failed to get pinned windows:', err);
    }
  }

  async function handleUnpin(hwnd: number) {
    try {
      await unpinWindow(hwnd);
      refreshPinnedWindows();
    } catch (err) {
      console.error('Failed to unpin:', err);
    }
  }

  async function handlePickWindow(hwnd: number) {
    setPickerOpen(false);
    try {
      // Toast/sound/refresh arrive via the pin-toggled event the command emits
      await pinWindow(hwnd);
    } catch (err) {
      addToast(String(err), 'error');
    }
  }

  function handleOpacityChange(hwnd: number, percent: number) {
    // Optimistic UI update — a slider drag fires dozens of change events,
    // and refreshing the whole list after each invoke caused an IPC storm
    // plus a race where the refresh reverted the thumb mid-drag.
    setPinnedWindows((prev) =>
      prev.map((w) =>
        w.hwnd === hwnd ? { ...w, opacity: Math.round((percent * 255) / 100) } : w
      )
    );

    // Throttle the backend call: apply immediately, then coalesce further
    // drag ticks into one trailing call.
    const pending = opacityFlush.current.get(hwnd);
    if (pending) {
      pending.latest = percent;
      return;
    }
    setWindowOpacity(hwnd, percent).catch((err) => {
      console.error('Failed to set opacity:', err);
      refreshPinnedWindows();
    });
    const entry = {
      latest: percent,
      timer: setTimeout(() => {
        const e = opacityFlush.current.get(hwnd);
        opacityFlush.current.delete(hwnd);
        if (e && e.latest !== percent) {
          setWindowOpacity(hwnd, e.latest).catch((err) => {
            console.error('Failed to set opacity:', err);
            refreshPinnedWindows();
          });
        }
      }, 80),
    };
    opacityFlush.current.set(hwnd, entry);
  }

  async function handleFocusWindow(hwnd: number) {
    try {
      await focusWindow(hwnd);
    } catch (err) {
      console.error('Failed to focus window:', err);
    }
  }

  async function handleSoundToggle() {
    const newValue = !soundEnabled;
    try {
      await setSoundEnabled(newValue);
      setSoundEnabledState(newValue);
      soundEnabledRef.current = newValue;
    } catch (err) {
      console.error('Failed to toggle sound:', err);
    }
  }

  async function handleAutoStartToggle() {
    const newValue = !autoStart;
    try {
      await setAutoStart(newValue);
      setAutoStartState(newValue);
    } catch (err) {
      console.error('Failed to toggle auto-start:', err);
    }
  }

  async function handleMinimize() {
    try {
      await Window.getCurrent().minimize();
    } catch (err) {
      console.error('Failed to minimize:', err);
    }
  }

  async function handleClose() {
    try {
      const seen = await getHasSeenTrayNotice();
      if (!seen) {
        setShowTrayNotice(true);
        await setHasSeenTrayNotice();
        return;
      }
    } catch {
      // If we can't check, just close
    }
    try {
      await Window.getCurrent().hide();
    } catch (err) {
      console.error('Failed to hide window:', err);
    }
  }

  async function dismissTrayNotice() {
    setShowTrayNotice(false);
    try {
      await Window.getCurrent().hide();
    } catch (err) {
      console.error('Failed to hide window:', err);
    }
  }

  function handleEditShortcut(key: keyof ShortcutConfig) {
    setEditingKey(key);
    setCaptureValue(null);
  }

  function handleCancelEdit() {
    setEditingKey(null);
    setCaptureValue(null);
  }

  function handleShortcutKeyDown(e: React.KeyboardEvent) {
    e.preventDefault();
    e.stopPropagation();
    const str = keyEventToShortcutString(e.nativeEvent);
    if (str) {
      setCaptureValue(str);
    }
  }

  async function handleSaveShortcut() {
    if (!editingKey || !captureValue || !shortcuts) return;
    // Catch duplicates here instead of surfacing a raw backend error after the fact
    const conflict = (Object.keys(SHORTCUT_LABELS) as (keyof ShortcutConfig)[]).find(
      (key) => key !== editingKey && shortcuts[key] === captureValue
    );
    if (conflict) {
      addToast(`Already used by ${SHORTCUT_LABELS[conflict]}`, 'error');
      return;
    }
    const newConfig = { ...shortcuts, [editingKey]: captureValue };
    try {
      await setShortcutConfig(newConfig);
      setShortcuts(newConfig);
      setEditingKey(null);
      setCaptureValue(null);
    } catch (err) {
      addToast(String(err), 'error');
    }
  }

  async function handleResetShortcuts() {
    try {
      const defaults = await resetShortcutConfig();
      setShortcuts(defaults);
      setEditingKey(null);
      setCaptureValue(null);
    } catch (err) {
      addToast(String(err), 'error');
    }
  }

  return (
    <div className="app-wrapper">
      {showTrayNotice && <TrayNotice onDismiss={dismissTrayNotice} />}

      {pickerOpen && <AddWindowPicker onPin={handlePickWindow} onClose={() => setPickerOpen(false)} />}

      <ToastStack toasts={toasts} />

      <Titlebar
        shortcutsOpen={shortcutsOpen}
        onToggleShortcuts={() => setShortcutsOpen(!shortcutsOpen)}
        onMinimize={handleMinimize}
        onClose={handleClose}
        popoverWrapperRef={popoverRef}
      >
        {shortcutsOpen && shortcuts && (
          <ShortcutsPopover
            shortcuts={shortcuts}
            editingKey={editingKey}
            captureValue={captureValue}
            onEdit={handleEditShortcut}
            onCancelEdit={handleCancelEdit}
            onCaptureKeyDown={handleShortcutKeyDown}
            onSave={handleSaveShortcut}
            onReset={handleResetShortcuts}
          />
        )}
      </Titlebar>

      <main className="container">
        <WindowList
          windows={pinnedWindows}
          shortcuts={shortcuts}
          onFocus={handleFocusWindow}
          onUnpin={handleUnpin}
          onOpacityChange={handleOpacityChange}
          onAddWindow={() => setPickerOpen(true)}
        />

        {shortcuts && <ShortcutsReference shortcuts={shortcuts} />}

        <SettingsSection
          soundEnabled={soundEnabled}
          autoStart={autoStart}
          onToggleSound={handleSoundToggle}
          onToggleAutoStart={handleAutoStartToggle}
        />
      </main>
    </div>
  );
}

export default App;
