import { useState, useEffect } from 'react';
import { listen } from '@tauri-apps/api/event';
import { getPinnedWindows, unpinWindow, setWindowOpacity } from './commands';
import type { PinnedWindow } from './types';
import './App.css';

function App() {
  const [pinnedWindows, setPinnedWindows] = useState<PinnedWindow[]>([]);
  const [lastAction, setLastAction] = useState<string>('Ready');

  useEffect(() => {
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

  return (
    <main className="container">
      <header className="header">
        <div className="logo">
          <img src="/logo.svg" alt="PinIt" width="32" height="32" />
          <h1>PinIt</h1>
        </div>
        <p className="subtitle">Always on Top Utility</p>
      </header>

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
  );
}

export default App;
