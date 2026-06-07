import type { ToastData } from '../types';

export function ToastStack({ toasts }: { toasts: ToastData[] }) {
  return (
    <div className="toast-container">
      {toasts.map((toast) => (
        <div key={toast.id} className={`toast toast-${toast.type}`}>
          <svg aria-hidden="true" className="toast-icon" width="10" height="10" viewBox="0 0 24 24" fill="currentColor">
            <path d="M16 12V4h1V2H7v2h1v8l-2 2v2h5.2v6h1.6v-6H18v-2l-2-2z"/>
          </svg>
          {toast.message}
        </div>
      ))}
    </div>
  );
}
