interface SettingsSectionProps {
  soundEnabled: boolean;
  autoStart: boolean;
  onToggleSound: () => void;
  onToggleAutoStart: () => void;
}

export function SettingsSection({ soundEnabled, autoStart, onToggleSound, onToggleAutoStart }: SettingsSectionProps) {
  return (
    <section className="settings-section">
      <div className="setting-row">
        <span className="setting-label">Sound effects</span>
        <button
          className={`toggle ${soundEnabled ? 'active' : ''}`}
          onClick={onToggleSound}
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
          onClick={onToggleAutoStart}
          title={autoStart ? 'Disable auto-start' : 'Enable auto-start'}
          role="switch"
          aria-checked={autoStart}
          aria-label="Start with Windows"
        >
          <span className="toggle-knob" />
        </button>
      </div>
    </section>
  );
}
