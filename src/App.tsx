import { useState, useEffect, useCallback, useRef } from 'react';
import { listen } from '@tauri-apps/api/event';
import { Window } from '@tauri-apps/api/window';
import { getPinnedWindows, unpinWindow, setWindowOpacity, focusWindow, getAutoStart, setAutoStart, getSoundEnabled, setSoundEnabled, getHasSeenTrayNotice, setHasSeenTrayNotice } from './commands';
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
  type: 'pin' | 'unpin' | 'error';
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
  const [soundEnabled, setSoundEnabledState] = useState(true);
  const soundEnabledRef = useRef(true);
  const [showTrayNotice, setShowTrayNotice] = useState(false);
  const toastTimeouts = useRef<Map<number, ReturnType<typeof setTimeout>>>(new Map());

  const addToast = useCallback((message: string, type: 'pin' | 'unpin' | 'error') => {
    const id = ++toastId;
    setToasts((prev) => [...prev.slice(-2), { id, message, type }]); // Keep max 3

    const timeout = setTimeout(() => {
      setToasts((prev) => prev.filter((t) => t.id !== id));
      toastTimeouts.current.delete(id);
    }, 2000);
    toastTimeouts.current.set(id, timeout);
  }, []);

  useEffect(() => {
    refreshPinnedWindows();
    getAutoStart().then(setAutoStartState).catch(() => {});
    getSoundEnabled().then((v) => { setSoundEnabledState(v); soundEnabledRef.current = v; }).catch(() => {});

    const unlistenPin = listen<PinToggledPayload>('pin-toggled', (event) => {
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

    const unlistenOpacity = listen<number>('opacity-changed', () => {
      refreshPinnedWindows();
    });

    const unlistenDestroyed = listen('window-destroyed', () => {
      refreshPinnedWindows();
    });

    const unlistenError = listen<string>('pin-error', (event) => {
      addToast(event.payload, 'error');
    });

    return () => {
      unlistenPin.then((fn) => fn());
      unlistenOpacity.then((fn) => fn());
      unlistenDestroyed.then((fn) => fn());
      unlistenError.then((fn) => fn());
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
    const appWindow = Window.getCurrent();
    await appWindow.minimize();
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
    const appWindow = Window.getCurrent();
    await appWindow.hide();
  }

  async function dismissTrayNotice() {
    setShowTrayNotice(false);
    const appWindow = Window.getCurrent();
    await appWindow.hide();
  }

  return (
    <div className="app-wrapper">
      {/* Tray Notice Overlay */}
      {showTrayNotice && (
        <div className="tray-notice-overlay">
          <div className="tray-notice">
            <p>PinIt will keep running in the system tray.</p>
            <p className="tray-notice-hint">Right-click the tray icon to quit.</p>
            <button className="tray-notice-btn" onClick={dismissTrayNotice}>Got it</button>
          </div>
        </div>
      )}

      {/* Toast Notifications */}
      <div className="toast-container">
        {toasts.map((toast) => (
          <div key={toast.id} className={`toast toast-${toast.type}`}>
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
          <div className="shortcuts-popover-wrapper">
            <button
              className="titlebar-btn"
              onClick={() => setShortcutsOpen(!shortcutsOpen)}
              title="Keyboard shortcuts"
              aria-expanded={shortcutsOpen}
              aria-controls="shortcuts-panel"
            >
              <svg width="10" height="10" viewBox="0 0 16 16" fill="currentColor">
                <path d="M8 1a7 7 0 100 14A7 7 0 008 1zm0 2.5a1 1 0 110 2 1 1 0 010-2zM6.5 7h1.25v4.5h1.5V7H10V5.75H6.5V7z"/>
              </svg>
            </button>
            {shortcutsOpen && (
              <div className="shortcuts-popover" id="shortcuts-panel" role="region" aria-label="Keyboard shortcuts">
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
          </div>
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
        {/* Pinned Windows */}
        <section className="pinned-section">
          <h2 className="section-heading">
            <svg className="pin-heading-icon" width="11" height="11" viewBox="0 0 24 24" fill="currentColor">
              <path d="M16 12V4h1V2H7v2h1v8l-2 2v2h5.2v6h1.6v-6H18v-2l-2-2z"/>
            </svg>
            Pinned
            {pinnedWindows.length > 0 && <span className="pin-count">{pinnedWindows.length}</span>}
          </h2>

          {pinnedWindows.length === 0 ? (
            <div className="empty-state">
              <svg className="empty-state-icon" width="24" height="24" viewBox="0 0 24 24" fill="currentColor" opacity="0.3">
                <path d="M16 12V4h1V2H7v2h1v8l-2 2v2h5.2v6h1.6v-6H18v-2l-2-2z"/>
              </svg>
              <span>No windows pinned</span>
              <span className="empty-state-hint">Focus any window, then press <kbd>Win</kbd>+<kbd>Ctrl</kbd>+<kbd>T</kbd></span>
            </div>
          ) : (
            <ul className="window-list" role="list" aria-label="Pinned windows">
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
                        onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); handleFocusWindow(win.hwnd); } }}
                        tabIndex={0}
                        role="button"
                        aria-label={`Focus ${win.title || win.process_name}`}
                      >
                        <span className="window-title" title={win.title}>{win.title || 'Untitled'}</span>
                        <span className="window-process">
                          {win.process_name}
                          <svg className="focus-icon" width="8" height="8" viewBox="0 0 12 12" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
                            <path d="M7 1h4v4M11 1L6 6M5 1H2a1 1 0 00-1 1v8a1 1 0 001 1h8a1 1 0 001-1V7" />
                          </svg>
                        </span>
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
                            aria-label={`Opacity for ${win.process_name}`}
                            aria-valuemin={20}
                            aria-valuemax={100}
                            aria-valuenow={opacityPercent}
                            aria-valuetext={`${opacityPercent}%`}
                          />
                          <span
                            className="opacity-label"
                            onDoubleClick={() => handleOpacityChange(win.hwnd, 100)}
                            title="Double-click to reset to 100%"
                          >{opacityPercent}%</span>
                        </div>
                        <button
                          className="unpin-btn"
                          onClick={() => handleUnpin(win.hwnd)}
                          title="Unpin this window"
                          aria-label={`Unpin ${win.process_name}`}
                        >
                          <svg width="12" height="12" viewBox="0 0 24 24" fill="currentColor">
                            <path d="M16 12V4h1V2H7v2h1v8l-2 2v2h5.2v6h1.6v-6H18v-2l-2-2z"/>
                            <line x1="3" y1="3" x2="21" y2="21" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round"/>
                          </svg>
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
            <span className="setting-label">Sound effects</span>
            <button
              className={`toggle ${soundEnabled ? 'active' : ''}`}
              onClick={handleSoundToggle}
              title={soundEnabled ? 'Disable sound' : 'Enable sound'}
              role="switch"
              aria-checked={soundEnabled}
              aria-label="Sound effects"
            >
              <span className="toggle-knob" />
            </button>
          </div>
          <div className="setting-row">
            <span className="setting-label">Start with Windows</span>
            <button
              className={`toggle ${autoStart ? 'active' : ''}`}
              onClick={handleAutoStartToggle}
              title={autoStart ? 'Disable auto-start' : 'Enable auto-start'}
              role="switch"
              aria-checked={autoStart}
              aria-label="Start with Windows"
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
