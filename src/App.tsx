import { useState, useEffect } from 'react';
import { listen } from '@tauri-apps/api/event';
import { Window } from '@tauri-apps/api/window';
import { getPinnedWindows, unpinWindow, setWindowOpacity } from './commands';
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

function App() {
  const [pinnedWindows, setPinnedWindows] = useState<PinnedWindow[]>([]);
  const [shortcutsOpen, setShortcutsOpen] = useState(false);

  useEffect(() => {
    refreshPinnedWindows();

    const unlistenPin = listen<boolean>('pin-toggled', () => {
      refreshPinnedWindows();
    });

    const unlistenOpacity = listen<number>('opacity-changed', () => {
      refreshPinnedWindows();
    });

    return () => {
      unlistenPin.then((fn) => fn());
      unlistenOpacity.then((fn) => fn());
    };
  }, []);

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
                      <div className="window-info" title={win.title}>
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
      </main>
    </div>
  );
}

export default App;
