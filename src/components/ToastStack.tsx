import type { ToastData } from '../types';

/** Per-type icon so the glyph carries meaning, not just the color */
function ToastIcon({ type }: { type: ToastData['type'] }) {
  switch (type) {
    case 'pin':
      return (
        <svg aria-hidden="true" className="toast-icon" width="10" height="10" viewBox="0 0 24 24" fill="currentColor">
          <path d="M16 12V4h1V2H7v2h1v8l-2 2v2h5.2v6h1.6v-6H18v-2l-2-2z"/>
        </svg>
      );
    case 'unpin':
      return (
        <svg aria-hidden="true" className="toast-icon" width="10" height="10" viewBox="0 0 24 24" fill="currentColor">
          <path d="M16 12V4h1V2H7v2h1v8l-2 2v2h5.2v6h1.6v-6H18v-2l-2-2z"/>
          <line x1="3" y1="3" x2="21" y2="21" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round"/>
        </svg>
      );
    case 'error':
      return (
        <svg aria-hidden="true" className="toast-icon" width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
          <path d="M12 3L2 20h20L12 3z"/>
          <line x1="12" y1="10" x2="12" y2="14"/>
          <circle cx="12" cy="17" r="0.5" fill="currentColor"/>
        </svg>
      );
  }
}

export function ToastStack({ toasts }: { toasts: ToastData[] }) {
  return (
    <div className="toast-container">
      {toasts.map((toast) => (
        <div key={toast.id} className={`toast toast-${toast.type}`}>
          <ToastIcon type={toast.type} />
          {toast.message}
        </div>
      ))}
    </div>
  );
}
