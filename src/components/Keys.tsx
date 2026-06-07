/** Renders a key combo like ["Win", "Ctrl", "T"] as kbd chips. */
export function Keys({ parts }: { parts: string[] }) {
  return (
    <div className="keys">
      {parts.map((k, i) => (
        <span key={i}>
          {i > 0 && <span>+</span>}
          <kbd>{k}</kbd>
        </span>
      ))}
    </div>
  );
}
