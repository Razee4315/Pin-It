import type { ReactNode, RefObject } from 'react';

interface TitlebarProps {
  shortcutsOpen: boolean;
  onToggleShortcuts: () => void;
  onMinimize: () => void;
  onClose: () => void;
  /** Wrapper ref used by App for outside-click dismissal of the popover */
  popoverWrapperRef: RefObject<HTMLDivElement | null>;
  /** The shortcuts popover, rendered when open */
  children?: ReactNode;
}

export function Titlebar({
  shortcutsOpen,
  onToggleShortcuts,
  onMinimize,
  onClose,
  popoverWrapperRef,
  children,
}: TitlebarProps) {
  return (
    <header className="titlebar" data-tauri-drag-region>
      <div className="titlebar-left" data-tauri-drag-region>
        <img src="/logo.png" alt="PinIt" width="14" height="14" />
        <span>PinIt</span>
      </div>
      <div className="titlebar-right">
        <div className="shortcuts-popover-wrapper" ref={popoverWrapperRef}>
          <button
            className="titlebar-btn"
            onClick={onToggleShortcuts}
            title="Shortcut settings"
            aria-expanded={shortcutsOpen}
            aria-controls="shortcuts-panel"
          >
            <svg aria-hidden="true" width="11" height="11" viewBox="0 0 16 16" fill="currentColor">
              <path d="M8 0a1 1 0 00-1 1v.1A5.96 5.96 0 005.05 1.9l-.07-.07a1 1 0 00-1.41 0L2.63 2.76a1 1 0 000 1.41l.07.07A5.96 5.96 0 001.9 6.19H1a1 1 0 00-1 1v1.62a1 1 0 001 1h.1a5.96 5.96 0 00.8 1.95l-.07.07a1 1 0 000 1.41l.94.94a1 1 0 001.41 0l.07-.07c.57.37 1.23.64 1.95.8V15a1 1 0 001 1h1.62a1 1 0 001-1v-.1a5.96 5.96 0 001.95-.8l.07.07a1 1 0 001.41 0l.94-.94a1 1 0 000-1.41l-.07-.07c.37-.57.64-1.23.8-1.95H15a1 1 0 001-1V7.19a1 1 0 00-1-1h-.1a5.96 5.96 0 00-.8-1.95l.07-.07a1 1 0 000-1.41l-.94-.94a1 1 0 00-1.41 0l-.07.07A5.96 5.96 0 009.81 1.1V1a1 1 0 00-1-1H8zM8 5a3 3 0 110 6 3 3 0 010-6z"/>
            </svg>
          </button>
          {children}
        </div>
        <button className="titlebar-btn" onClick={onMinimize} title="Minimize">
          <svg aria-hidden="true" width="10" height="1" viewBox="0 0 10 1">
            <rect width="10" height="1" fill="currentColor" />
          </svg>
        </button>
        <button className="titlebar-btn close" onClick={onClose} title="Close">
          <svg aria-hidden="true" width="10" height="10" viewBox="0 0 10 10">
            <path d="M1 1L9 9M9 1L1 9" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" />
          </svg>
        </button>
      </div>
    </header>
  );
}
