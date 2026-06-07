import { useEffect } from 'react';

export function TrayNotice({ onDismiss }: { onDismiss: () => void }) {
  // Esc dismisses, same as the button
  useEffect(() => {
    function onKeyDown(e: KeyboardEvent) {
      if (e.key === 'Escape') onDismiss();
    }
    document.addEventListener('keydown', onKeyDown);
    return () => document.removeEventListener('keydown', onKeyDown);
  }, [onDismiss]);

  return (
    <div className="tray-notice-overlay">
      <div className="tray-notice">
        <p>PinIt will keep running in the system tray.</p>
        <p className="tray-notice-hint">Click the PinIt tray icon to reopen — right-click it to quit.</p>
        <button className="tray-notice-btn" onClick={onDismiss}>Got it</button>
      </div>
    </div>
  );
}
