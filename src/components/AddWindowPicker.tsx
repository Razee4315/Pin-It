import { useEffect, useState } from 'react';
import type { PinnableWindow } from '../types';
import { listPinnableWindows } from '../commands';
import { getAvatarColor, getInitial } from '../utils/avatar';

interface AddWindowPickerProps {
  onPin: (hwnd: number) => void;
  onClose: () => void;
}

/** Overlay listing open windows so users can pin without the hotkey. */
export function AddWindowPicker({ onPin, onClose }: AddWindowPickerProps) {
  const [windows, setWindows] = useState<PinnableWindow[] | null>(null);

  useEffect(() => {
    listPinnableWindows()
      .then(setWindows)
      .catch((err) => {
        console.error('Failed to list windows:', err);
        setWindows([]);
      });
  }, []);

  useEffect(() => {
    function onKeyDown(e: KeyboardEvent) {
      if (e.key === 'Escape') onClose();
    }
    document.addEventListener('keydown', onKeyDown);
    return () => document.removeEventListener('keydown', onKeyDown);
  }, [onClose]);

  return (
    <div className="picker-overlay" onMouseDown={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className="picker" role="dialog" aria-label="Pin a window">
        <div className="picker-header">
          <span>Pin a window</span>
          <button className="titlebar-btn" onClick={onClose} title="Close" aria-label="Close picker">
            <svg aria-hidden="true" width="10" height="10" viewBox="0 0 10 10">
              <path d="M1 1L9 9M9 1L1 9" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" />
            </svg>
          </button>
        </div>
        <div className="picker-list">
          {windows === null ? (
            <div className="picker-empty">Loading…</div>
          ) : windows.length === 0 ? (
            <div className="picker-empty">No other windows found</div>
          ) : (
            windows.map((win) => (
              <button key={win.hwnd} className="picker-item" onClick={() => onPin(win.hwnd)}>
                <div
                  className="process-avatar picker-avatar"
                  style={{ background: getAvatarColor(win.process_name) }}
                >
                  {getInitial(win.process_name)}
                </div>
                <div className="picker-item-info">
                  <span className="picker-item-title" title={win.title}>{win.title}</span>
                  <span className="picker-item-process">{win.process_name}</span>
                </div>
              </button>
            ))
          )}
        </div>
      </div>
    </div>
  );
}
