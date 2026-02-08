import { useState, useEffect } from 'react';
import { listen } from '@tauri-apps/api/event';
import { Window } from '@tauri-apps/api/window';
import { getPinnedWindows, unpinWindow, setWindowOpacity } from './commands';
import type { PinnedWindow } from './types';
import './App.css';

type Theme = 'paper' | 'dark';

function App() {
  const [pinnedWindows, setPinnedWindows] = useState<PinnedWindow[]>([]);
  const [theme, setTheme] = useState<Theme>('dark');

  useEffect(() => {
    const savedTheme = localStorage.getItem('pinit-theme') as Theme;
    if (savedTheme) setTheme(savedTheme);

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

  useEffect(() => {
    localStorage.setItem('pinit-theme', theme);
    document.documentElement.setAttribute('data-theme', theme);
  }, [theme]);

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

  function toggleTheme() {
    setTheme(theme === 'dark' ? 'paper' : 'dark');
  }

  return (
    <div className="app-wrapper" data-theme={theme}>
      {/* Custom Titlebar */}
      <header className="titlebar" data-tauri-drag-region>
        <div className="titlebar-left" data-tauri-drag-region>
          <img src="/logo.svg" alt="PinIt" width="14" height="14" />
          <span>PinIt</span>
        </div>
        <div className="titlebar-right">
          <button
            className={`theme-toggle ${theme}`}
            onClick={toggleTheme}
            title={theme === 'dark' ? 'Switch to Paper' : 'Switch to Dark'}
          >
            <div className="bulb" />
          </button>
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
        <section className="shortcuts-info">
          <h2>Shortcuts</h2>
          <div className="shortcut-list">
            <div className="shortcut-item">
              <div className="keys">
                <kbd>Win</kbd><span>+</span><kbd>Ctrl</kbd><span>+</span><kbd>T</kbd>
              </div>
              <span className="desc">Pin/Unpin window</span>
            </div>
            <div className="shortcut-item">
              <div className="keys">
                <kbd>Win</kbd><span>+</span><kbd>Ctrl</kbd><span>+</span><kbd>=</kbd><span>/</span><kbd>-</kbd>
              </div>
              <span className="desc">Adjust opacity</span>
            </div>
          </div>
        </section>

        <section className="pinned-windows">
          <h2>Pinned ({pinnedWindows.length})</h2>
          {pinnedWindows.length === 0 ? (
            <div className="empty-state">
              <p>No windows pinned</p>
              <span>Use <kbd>Win</kbd>+<kbd>Ctrl</kbd>+<kbd>T</kbd></span>
            </div>
          ) : (
            <ul className="window-list">
              {pinnedWindows.map((win) => (
                <li key={win.hwnd} className="window-item">
                  <div className="window-info">
                    <span className="window-title">{win.title || 'Untitled'}</span>
                    <span className="window-process">{win.process_name}</span>
                  </div>
                  <div className="window-controls">
                    <input
                      type="range"
                      min={20}
                      max={100}
                      value={Math.round((win.opacity / 255) * 100)}
                      onChange={(e) => handleOpacityChange(win.hwnd, parseInt(e.target.value))}
                      title={`${Math.round((win.opacity / 255) * 100)}%`}
                    />
                    <button
                      className="unpin-btn"
                      onClick={() => handleUnpin(win.hwnd)}
                      title="Unpin"
                    >
                      ×
                    </button>
                  </div>
                </li>
              ))}
            </ul>
          )}
        </section>
      </main>
    </div>
  );
}

export default App;
