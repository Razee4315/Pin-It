import type { ShortcutConfig } from '../types';
import { SHORTCUT_LABELS } from '../types';
import { shortcutToDisplay } from '../shortcutUtils';
import { Keys } from './Keys';

interface ShortcutsPopoverProps {
  shortcuts: ShortcutConfig;
  editingKey: keyof ShortcutConfig | null;
  captureValue: string | null;
  onEdit: (key: keyof ShortcutConfig) => void;
  onCancelEdit: () => void;
  onCaptureKeyDown: (e: React.KeyboardEvent) => void;
  onSave: () => void;
  onReset: () => void;
}

export function ShortcutsPopover({
  shortcuts,
  editingKey,
  captureValue,
  onEdit,
  onCancelEdit,
  onCaptureKeyDown,
  onSave,
  onReset,
}: ShortcutsPopoverProps) {
  return (
    <div className="shortcuts-popover" id="shortcuts-panel" role="region" aria-label="Edit shortcuts">
      <div className="shortcuts-popover-title">Edit Shortcuts</div>
      {(Object.keys(SHORTCUT_LABELS) as (keyof ShortcutConfig)[]).map((key) => {
        const isEditing = editingKey === key;
        const displayParts = shortcutToDisplay(
          isEditing && captureValue ? captureValue : shortcuts[key]
        );
        return (
          <div
            key={key}
            className={`shortcut-item${isEditing ? ' shortcut-editing' : ''}`}
          >
            <span className="desc">{SHORTCUT_LABELS[key]}</span>
            {isEditing ? (
              <div
                className="shortcut-capture"
                tabIndex={0}
                onKeyDown={onCaptureKeyDown}
                onBlur={onCancelEdit}
                ref={(el) => el?.focus()}
              >
                {captureValue ? (
                  <Keys parts={displayParts} />
                ) : (
                  <div className="keys">
                    <span className="capture-hint">Press keys...</span>
                  </div>
                )}
                {captureValue && (
                  <button
                    className="shortcut-save-btn"
                    onMouseDown={(e) => { e.preventDefault(); onSave(); }}
                    title="Save shortcut"
                  >
                    <svg aria-hidden="true" width="10" height="10" viewBox="0 0 12 12" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                      <path d="M2 6l3 3 5-5" />
                    </svg>
                  </button>
                )}
              </div>
            ) : (
              <div className="shortcut-display">
                <Keys parts={displayParts} />
                <button
                  className="shortcut-edit-btn"
                  onClick={() => onEdit(key)}
                  title="Edit shortcut"
                >
                  <svg aria-hidden="true" width="9" height="9" viewBox="0 0 12 12" fill="currentColor">
                    <path d="M9.5.5a1.4 1.4 0 012 2L4 10l-3 1 1-3L9.5.5z"/>
                  </svg>
                </button>
              </div>
            )}
          </div>
        );
      })}
      <button className="shortcut-reset-btn" onClick={onReset}>
        Reset to defaults
      </button>
    </div>
  );
}
