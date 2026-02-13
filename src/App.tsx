import { useState, useEffect, useCallback, useRef } from 'react';
import { listen } from '@tauri-apps/api/event';
import { Window } from '@tauri-apps/api/window';
import { getPinnedWindows, unpinWindow, setWindowOpacity, focusWindow, getAutoStart, setAutoStart } from './commands';
import type { PinnedWindow } from './types';
import './App.css';

const AVATAR_COLORS = [
  '#e57373', '#f06292', '#ba68c8', '#9575cd', '#7986cb',
  '#64b5f6', '#4fc3f7', '#4dd0e1', '#4db6ac', '#81c784',
  '#aed581', '#ffd54f', '#ffb74d', '#ff8a65', '#a1887f',
];

function getAvatarColor(name: string): string {
  let hash = 0;
  for (let i = 0; i < name.length; i++) {
    hash = name.charCodeAt(i) + ((hash << 5) - hash);
  }
  return AVATAR_COLORS[Math.abs(hash) % AVATAR_COLORS.length];
}

function getInitial(name: string): string {
  return name.replace(/\.exe$/i, '').charAt(0).toUpperCase();
}

// --- Sound ---
function playSound(pinned: boolean) {
  try {
    const ctx = new AudioContext();
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.type = 'sine';
    // Pin = higher pitch rising, Unpin = lower pitch falling
    osc.frequency.value = pinned ? 880 : 660;
    gain.gain.value = 0.08;
    osc.start();
    gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + 0.12);
    osc.stop(ctx.currentTime + 0.12);
  } catch {
    // Audio not available, ignore
  }
}

// --- Toast types ---
interface ToastData {
  id: number;
  message: string;
  pinned: boolean;
}

interface PinToggledPayload {
  is_pinned: boolean;
  title: string;
  process_name: string;
}

let toastId = 0;

function App() {
  const [pinnedWindows, setPinnedWindows] = useState<PinnedWindow[]>([]);
  const [shortcutsOpen, setShortcutsOpen] = useState(false);
  const [toasts, setToasts] = useState<ToastData[]>([]);
  const [autoStart, setAutoStartState] = useState(false);
  const toastTimeouts = useRef<Map<number, ReturnType<typeof setTimeout>>>(new Map());

  const addToast = useCallback((message: string, pinned: boolean) => {
    const id = ++toastId;
    setToasts((prev) => [...prev.slice(-2), { id, message, pinned }]); // Keep max 3

    const timeout = setTimeout(() => {
      setToasts((prev) => prev.filter((t) => t.id !== id));
      toastTimeouts.current.delete(id);
    }, 2000);
    toastTimeouts.current.set(id, timeout);
  }, []);

  useEffect(() => {
    refreshPinnedWindows();
    getAutoStart().then(setAutoStartState).catch(() => {});

    const unlistenPin = listen<PinToggledPayload>('pin-toggled', (event) => {
      refreshPinnedWindows();
      const { is_pinned, title, process_name } = event.payload;
      const name = title && title !== 'Unknown' ? title : process_name;
      const truncated = name.length > 30 ? name.slice(0, 30) + '...' : name;
      addToast(
        is_pinned ? `Pinned: ${truncated}` : `Unpinned: ${truncated}`,
        is_pinned
      );
      playSound(is_pinned);
    });

    const unlistenOpacity = listen<number>('opacity-changed', () => {
      refreshPinnedWindows();
    });

    return () => {
      unlistenPin.then((fn) => fn());
      unlistenOpacity.then((fn) => fn());
      toastTimeouts.current.forEach((t) => clearTimeout(t));
    };
  }, [addToast]);

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

  async function handleOpacityChange(hwnd: number, opacity: number) {
    try {
      await setWindowOpacity(hwnd, opacity);
      refreshPinnedWindows();
    } catch (err) {
      console.error('Failed to set opacity:', err);
    }
  }

  async function handleFocusWindow(hwnd: number) {
    try {
      await focusWindow(hwnd);
    } catch (err) {
      console.error('Failed to focus window:', err);
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
    const appWindow = Window.getCurrent();
    await appWindow.minimize();
  }

  async function handleClose() {
    const appWindow = Window.getCurrent();
    await appWindow.hide();
  }

  return (
    <div className="app-wrapper">
      {/* Toast Notifications */}
      <div className="toast-container">
        {toasts.map((toast) => (
          <div key={toast.id} className={`toast ${toast.pinned ? 'toast-pin' : 'toast-unpin'}`}>
            <svg className="toast-icon" width="10" height="10" viewBox="0 0 24 24" fill="currentColor">
              <path d="M16 12V4h1V2H7v2h1v8l-2 2v2h5.2v6h1.6v-6H18v-2l-2-2z"/>
            </svg>
            {toast.message}
          </div>
        ))}
      </div>

      {/* Custom Titlebar */}
      <header className="titlebar" data-tauri-drag-region>
        <div className="titlebar-left" data-tauri-drag-region>
          <img src="/logo.svg" alt="PinIt" width="14" height="14" />
          <span>PinIt</span>
        </div>
        <div className="titlebar-right">
          <button className="titlebar-btn" onClick={handleMinimize} title="Minimize">
            <svg width="10" height="1" viewBox="0 0 10 1">
              <rect width="10" height="1" fill="currentColor" />
            </svg>
          </button>
          <button className="titlebar-btn close" onClick={handleClose} title="Close">
            <svg width="10" height="10" viewBox="0 0 10 10">
              <path d="M1 1L9 9M9 1L1 9" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" />
            </svg>
          </button>
        </div>
      </header>

      <main className="container">
        {/* Collapsible Shortcuts */}
        <section className="shortcuts-section">
          <button className="section-toggle" onClick={() => setShortcutsOpen(!shortcutsOpen)}>
            <span className="section-label">Shortcuts</span>
            <svg className={`chevron ${shortcutsOpen ? 'open' : ''}`} width="10" height="10" viewBox="0 0 10 10">
              <path d="M2.5 4L5 6.5L7.5 4" stroke="currentColor" strokeWidth="1.3" strokeLinecap="round" strokeLinejoin="round" fill="none" />
            </svg>
          </button>
          {shortcutsOpen && (
            <div className="shortcut-list">
              <div className="shortcut-item">
                <div className="keys">
                  <kbd>Win</kbd><span>+</span><kbd>Ctrl</kbd><span>+</span><kbd>T</kbd>
                </div>
                <span className="desc">Pin/Unpin</span>
              </div>
              <div className="shortcut-item">
                <div className="keys">
                  <kbd>Win</kbd><span>+</span><kbd>Ctrl</kbd><span>+</span><kbd>=</kbd><span>/</span><kbd>-</kbd>
                </div>
                <span className="desc">Opacity</span>
              </div>
            </div>
          )}
        </section>

        {/* Pinned Windows */}
        <section className="pinned-section">
          <h2 className="section-heading">
            <svg className="pin-heading-icon" width="11" height="11" viewBox="0 0 24 24" fill="currentColor">
              <path d="M16 12V4h1V2H7v2h1v8l-2 2v2h5.2v6h1.6v-6H18v-2l-2-2z"/>
            </svg>
            Pinned
            <span className="pin-count">{pinnedWindows.length}</span>
          </h2>

          {pinnedWindows.length === 0 ? (
            <div className="empty-state">
              <span>Press <kbd>Win</kbd>+<kbd>Ctrl</kbd>+<kbd>T</kbd> to pin a window</span>
            </div>
          ) : (
            <ul className="window-list">
              {pinnedWindows.map((win) => {
                const opacityPercent = Math.round((win.opacity / 255) * 100);
                return (
                  <li key={win.hwnd} className="window-item">
                    <div className="window-item-row">
                      <div
                        className="process-avatar"
                        style={{ background: getAvatarColor(win.process_name) }}
                      >
                        {getInitial(win.process_name)}
                      </div>
                      <div
                        className="window-info clickable"
                        title={`Click to focus: ${win.title}`}
                        onClick={() => handleFocusWindow(win.hwnd)}
                      >
                        <span className="window-title">{win.title || 'Untitled'}</span>
                        <span className="window-process">{win.process_name}</span>
                      </div>
                      <div className="window-controls">
                        <div className="opacity-control">
                          <input
                            type="range"
                            min={20}
                            max={100}
                            value={opacityPercent}
                            onChange={(e) => handleOpacityChange(win.hwnd, parseInt(e.target.value))}
                            title={`Opacity: ${opacityPercent}%`}
                          />
                          <span className="opacity-label">{opacityPercent}%</span>
                        </div>
                        <button
                          className="unpin-btn"
                          onClick={() => handleUnpin(win.hwnd)}
                          title="Unpin"
                        >
                          ×
                        </button>
                      </div>
                    </div>
                  </li>
                );
              })}
            </ul>
          )}
        </section>

        {/* Settings */}
        <section className="settings-section">
          <div className="setting-row">
            <span className="setting-label">Start with Windows</span>
            <button
              className={`toggle ${autoStart ? 'active' : ''}`}
              onClick={handleAutoStartToggle}
              title={autoStart ? 'Disable auto-start' : 'Enable auto-start'}
            >
              <span className="toggle-knob" />
            </button>
          </div>
        </section>
      </main>
    </div>
  );
}

export default App;
