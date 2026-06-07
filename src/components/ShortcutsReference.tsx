import type { ShortcutConfig } from '../types';
import { SHORTCUT_LABELS } from '../types';
import { shortcutToDisplay } from '../shortcutUtils';
import { Keys } from './Keys';

export function ShortcutsReference({ shortcuts }: { shortcuts: ShortcutConfig }) {
  return (
    <section className="shortcuts-reference">
      <h2 className="section-heading">
        <svg aria-hidden="true" width="11" height="11" viewBox="0 0 16 16" fill="currentColor" opacity="0.6">
          <path d="M0 3a2 2 0 012-2h12a2 2 0 012 2v7a2 2 0 01-2 2H2a2 2 0 01-2-2V3zm3 1a1 1 0 100 2h1a1 1 0 100-2H3zm4 0a1 1 0 100 2h1a1 1 0 100-2H7zm4 0a1 1 0 100 2h1a1 1 0 100-2h-1zM3 7a1 1 0 100 2h10a1 1 0 100-2H3z"/>
        </svg>
        Shortcuts
      </h2>
      <div className="shortcuts-grid">
        {(Object.keys(SHORTCUT_LABELS) as (keyof ShortcutConfig)[]).map((key) => (
          <div key={key} className="shortcut-ref-row">
            <span className="shortcut-ref-label">{SHORTCUT_LABELS[key]}</span>
            <Keys parts={shortcutToDisplay(shortcuts[key])} />
          </div>
        ))}
      </div>
    </section>
  );
}
