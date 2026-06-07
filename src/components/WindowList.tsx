import type { PinnedWindow, ShortcutConfig } from '../types';
import { shortcutToDisplay } from '../shortcutUtils';
import { getAvatarColor, getInitial } from '../utils/avatar';

interface WindowListProps {
  windows: PinnedWindow[];
  shortcuts: ShortcutConfig | null;
  onFocus: (hwnd: number) => void;
  onUnpin: (hwnd: number) => void;
  onOpacityChange: (hwnd: number, percent: number) => void;
}

export function WindowList({ windows, shortcuts, onFocus, onUnpin, onOpacityChange }: WindowListProps) {
  return (
    <section className="pinned-section">
      <h2 className="section-heading">
        <svg aria-hidden="true" className="pin-heading-icon" width="11" height="11" viewBox="0 0 24 24" fill="currentColor">
          <path d="M16 12V4h1V2H7v2h1v8l-2 2v2h5.2v6h1.6v-6H18v-2l-2-2z"/>
        </svg>
        Pinned
        {windows.length > 0 && <span className="pin-count">{windows.length}</span>}
      </h2>

      {windows.length === 0 ? (
        <div className="empty-state">
          <svg aria-hidden="true" className="empty-state-icon" width="24" height="24" viewBox="0 0 24 24" fill="currentColor" opacity="0.3">
            <path d="M16 12V4h1V2H7v2h1v8l-2 2v2h5.2v6h1.6v-6H18v-2l-2-2z"/>
          </svg>
          <span>No windows pinned</span>
          <span className="empty-state-hint">Focus any window, then press {shortcuts ? shortcutToDisplay(shortcuts.toggle_pin).map((k, i) => (<span key={i}>{i > 0 && '+'}<kbd>{k}</kbd></span>)) : <><kbd>Win</kbd>+<kbd>Ctrl</kbd>+<kbd>T</kbd></>}</span>
        </div>
      ) : (
        <ul className="window-list" role="list" aria-label="Pinned windows">
          {windows.map((win) => {
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
                    onClick={() => onFocus(win.hwnd)}
                    onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); onFocus(win.hwnd); } }}
                    tabIndex={0}
                    role="button"
                    aria-label={`Focus ${win.title || win.process_name}`}
                  >
                    <span className="window-title" title={win.title}>{win.title || 'Untitled'}</span>
                    <span className="window-process">
                      {win.process_name}
                      <svg aria-hidden="true" className="focus-icon" width="8" height="8" viewBox="0 0 12 12" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
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
                        onChange={(e) => onOpacityChange(win.hwnd, parseInt(e.target.value))}
                        title={`Opacity: ${opacityPercent}%`}
                        aria-label={`Opacity for ${win.process_name}`}
                        aria-valuemin={20}
                        aria-valuemax={100}
                        aria-valuenow={opacityPercent}
                        aria-valuetext={`${opacityPercent}%`}
                      />
                      <span
                        className="opacity-label"
                        onDoubleClick={() => onOpacityChange(win.hwnd, 100)}
                        title="Double-click to reset to 100%"
                      >{opacityPercent}%</span>
                    </div>
                    <button
                      className="unpin-btn"
                      onClick={() => onUnpin(win.hwnd)}
                      title="Unpin this window"
                      aria-label={`Unpin ${win.process_name}`}
                    >
                      <svg aria-hidden="true" width="12" height="12" viewBox="0 0 24 24" fill="currentColor">
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
  );
}
