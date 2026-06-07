export function TrayNotice({ onDismiss }: { onDismiss: () => void }) {
  return (
    <div className="tray-notice-overlay">
      <div className="tray-notice">
        <p>PinIt will keep running in the system tray.</p>
        <p className="tray-notice-hint">Right-click the tray icon to quit.</p>
        <button className="tray-notice-btn" onClick={onDismiss}>Got it</button>
      </div>
    </div>
  );
}
