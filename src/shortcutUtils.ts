/**
 * Utilities for converting between browser KeyboardEvents and
 * tauri-plugin-global-shortcut shortcut strings.
 */

/** Convert a browser KeyboardEvent into a plugin-format shortcut string.
 *  Returns null if no valid key (only modifiers pressed). */
export function keyEventToShortcutString(e: KeyboardEvent): string | null {
  // Ignore bare modifier keys
  if (['Control', 'Shift', 'Alt', 'Meta'].includes(e.key)) {
    return null;
  }

  const parts: string[] = [];
  if (e.metaKey) parts.push('super');
  if (e.ctrlKey) parts.push('ctrl');
  if (e.altKey) parts.push('alt');
  if (e.shiftKey) parts.push('shift');

  // Require at least one modifier
  if (parts.length === 0) {
    return null;
  }

  // e.code gives values like "KeyT", "Equal", "Minus", "Digit1" etc.
  // which match the plugin's Code enum names
  parts.push(e.code);
  return parts.join('+');
}

/** Map of code values to display-friendly key names */
const CODE_DISPLAY: Record<string, string> = {
  Equal: '=',
  Minus: '-',
  BracketLeft: '[',
  BracketRight: ']',
  Backslash: '\\',
  Semicolon: ';',
  Quote: "'",
  Comma: ',',
  Period: '.',
  Slash: '/',
  Backquote: '`',
  Space: 'Space',
  Enter: 'Enter',
  Backspace: 'Bksp',
  Tab: 'Tab',
  Escape: 'Esc',
  Delete: 'Del',
  Insert: 'Ins',
  Home: 'Home',
  End: 'End',
  PageUp: 'PgUp',
  PageDown: 'PgDn',
  ArrowUp: 'Up',
  ArrowDown: 'Down',
  ArrowLeft: 'Left',
  ArrowRight: 'Right',
};

/** Modifier display names */
const MOD_DISPLAY: Record<string, string> = {
  super: 'Win',
  ctrl: 'Ctrl',
  alt: 'Alt',
  shift: 'Shift',
};

/** Convert a shortcut string like "super+ctrl+KeyT" to display-friendly array ["Win", "Ctrl", "T"] */
export function shortcutToDisplay(str: string): string[] {
  const parts = str.split('+');
  return parts.map((part) => {
    // Check if it's a modifier
    if (MOD_DISPLAY[part]) return MOD_DISPLAY[part];

    // Check display map
    if (CODE_DISPLAY[part]) return CODE_DISPLAY[part];

    // KeyX -> X
    if (part.startsWith('Key')) return part.slice(3);

    // DigitX -> X
    if (part.startsWith('Digit')) return part.slice(5);

    // FX (function keys)
    if (/^F\d+$/.test(part)) return part;

    // NumpadX
    if (part.startsWith('Numpad')) return 'Num' + part.slice(6);

    return part;
  });
}
