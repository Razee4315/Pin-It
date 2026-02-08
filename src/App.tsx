import { useState, useEffect } from 'react';
import { listen } from '@tauri-apps/api/event';
import { getCurrentWindow } from '@tauri-apps/api/window';
import { getPinnedWindows, unpinWindow, setWindowOpacity } from './commands';
import type { PinnedWindow } from './types';
import './App.css';

type Theme = 'glass' | 'paper' | 'midnight';

function App() {
  const [pinnedWindows, setPinnedWindows] = useState<PinnedWindow[]>([]);
  const [lastAction, setLastAction] = useState<string>('Ready');
  const [theme, setTheme] = useState<Theme>('glass');

  useEffect(() => {
    // Load saved theme
    const savedTheme = localStorage.getItem('pinit-theme') as Theme;
    if (savedTheme) setTheme(savedTheme);

    // Initial fetch
    refreshPinnedWindows();

    // Listen for pin events
    const unlistenPin = listen<boolean>('pin-toggled', (event) => {
      setLastAction(event.payload ? 'Window pinned!' : 'Window unpinned!');
      refreshPinnedWindows();
    });

    const unlistenOpacity = listen<number>('opacity-changed', (event) => {
      setLastAction(`Opacity: ${event.payload}%`);
      refreshPinnedWindows();
    });

    const unlistenError = listen<string>('pin-error', (event) => {
      setLastAction(`Error: ${event.payload}`);
    });

    return () => {
      unlistenPin.then((fn) => fn());
      unlistenOpacity.then((fn) => fn());
      unlistenError.then((fn) => fn());
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
      setLastAction('Window unpinned');
      refreshPinnedWindows();
    } catch (err) {
      setLastAction(`Error: ${err}`);
    }
  }

  async function handleOpacityChange(hwnd: number, opacity: number) {
    try {
      await setWindowOpacity(hwnd, opacity);
      refreshPinnedWindows();
    } catch (err) {
      setLastAction(`Error: ${err}`);
    }
  }

  async function handleMinimize() {
    const appWindow = getCurrentWindow();
    await appWindow.minimize();
  }

  async function handleClose() {
    const appWindow = getCurrentWindow();
    await appWindow.hide();
  }

  return (
    <div className="app-wrapper" data-theme={theme}>
      {/* Custom Titlebar */}
      <header className="titlebar" data-tauri-drag-region>
        <div className="titlebar-title" data-tauri-drag-region>
          <img src="/logo.svg" alt="PinIt" width="16" height="16" />
          <span>PinIt</span>
        </div>
        <div className="titlebar-buttons">
          <button className="titlebar-btn minimize" onClick={handleMinimize} title="Minimize">
            <svg width="10" height="1" viewBox="0 0 10 1">
              <rect width="10" height="1" fill="currentColor" />
            </svg>
          </button>
          <button className="titlebar-btn close" onClick={handleClose} title="Close">
            <svg width="10" height="10" viewBox="0 0 10 10">
              <path d="M1 1L9 9M9 1L1 9" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" />
            </svg>
          </button>
        </div>
      </header>

      <main className="container">
        {/* Theme Selector */}
        <section className="theme-selector">
          <button
            className={`theme-btn ${theme === 'glass' ? 'active' : ''}`}
            onClick={() => setTheme('glass')}
            title="Glass Theme"
          >
            ✨
          </button>
          <button
            className={`theme-btn ${theme === 'paper' ? 'active' : ''}`}
            onClick={() => setTheme('paper')}
            title="Paper Theme"
          >
            📄
          </button>
          <button
            className={`theme-btn ${theme === 'midnight' ? 'active' : ''}`}
            onClick={() => setTheme('midnight')}
            title="Midnight Theme"
          >
            🌙
          </button>
        </section>

        <section className="status-card">
          <div className="status-indicator" />
          <span className="status-text">{lastAction}</span>
        </section>

        <section className="shortcuts-info">
          <h2>Keyboard Shortcuts</h2>
          <div className="shortcut-list">
            <div className="shortcut-item">
              <kbd>Win</kbd> + <kbd>Ctrl</kbd> + <kbd>T</kbd>
              <span>Toggle pin on focused window</span>
            </div>
            <div className="shortcut-item">
              <kbd>Win</kbd> + <kbd>Ctrl</kbd> + <kbd>=</kbd>
              <span>Increase opacity (+10%)</span>
            </div>
            <div className="shortcut-item">
              <kbd>Win</kbd> + <kbd>Ctrl</kbd> + <kbd>-</kbd>
              <span>Decrease opacity (-10%)</span>
            </div>
          </div>
        </section>

        <section className="pinned-windows">
          <h2>Pinned Windows ({pinnedWindows.length})</h2>
          {pinnedWindows.length === 0 ? (
            <div className="empty-state">
              <p>No windows pinned yet.</p>
              <p className="hint">Press <kbd>Win</kbd> + <kbd>Ctrl</kbd> + <kbd>T</kbd> on any window to pin it.</p>
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
                      title={`Opacity: ${Math.round((win.opacity / 255) * 100)}%`}
                    />
                    <button
                      className="unpin-btn"
                      onClick={() => handleUnpin(win.hwnd)}
                      title="Unpin window"
                    >
                      ✕
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
