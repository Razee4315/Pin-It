//! shortcuts — parse the Tauri-style shortcut strings stored in pinned.json
//! ("super+ctrl+KeyT") into RegisterHotKey modifiers and a virtual-key code,
//! and back into what the user sees ("Win", "Ctrl", "T").

pub const MOD_ALT: u32 = 0x0001;
pub const MOD_CONTROL: u32 = 0x0002;
pub const MOD_SHIFT: u32 = 0x0004;
pub const MOD_WIN: u32 = 0x0008;

/// Keys with a name rather than a letter or digit: the token stored in
/// pinned.json, the label shown to the user, and the virtual-key code.
const NAMED_KEYS: &[(&str, &str, u32)] = &[
    ("Equal", "=", 0xBB),
    ("Minus", "-", 0xBD),
    ("Comma", ",", 0xBC),
    ("Period", ".", 0xBE),
    ("Slash", "/", 0xBF),
    ("Semicolon", ";", 0xBA),
    ("Quote", "'", 0xDE),
    ("BracketLeft", "[", 0xDB),
    ("BracketRight", "]", 0xDD),
    ("Backslash", "\\", 0xDC),
    ("Backquote", "`", 0xC0),
    ("Space", "Space", 0x20),
    ("ArrowUp", "Up", 0x26),
    ("ArrowDown", "Down", 0x28),
    ("ArrowLeft", "Left", 0x25),
    ("ArrowRight", "Right", 0x27),
    ("Home", "Home", 0x24),
    ("End", "End", 0x23),
    ("PageUp", "Page Up", 0x21),
    ("PageDown", "Page Down", 0x22),
    ("Insert", "Insert", 0x2D),
    ("Delete", "Delete", 0x2E),
];

const VK_F1: u32 = 0x70;
const FUNCTION_KEYS: u32 = 24;

/// "F1".."F24" -> 1..24.
fn function_key(token: &str) -> Option<u32> {
    let rest = token.strip_prefix(['F', 'f'])?;
    let n: u32 = rest.parse().ok()?;
    (1..=FUNCTION_KEYS).contains(&n).then_some(n)
}

fn single_char(s: &str) -> Option<char> {
    let mut chars = s.chars();
    let c = chars.next()?;
    chars.next().is_none().then_some(c)
}

fn key_from_token(token: &str) -> Option<u32> {
    if let Some(c) = token.strip_prefix("Key").and_then(single_char) {
        if c.is_ascii_alphabetic() {
            return Some(c.to_ascii_uppercase() as u32);
        }
    }
    if let Some(c) = token.strip_prefix("Digit").and_then(single_char) {
        if c.is_ascii_digit() {
            return Some(c as u32);
        }
    }
    if let Some(n) = function_key(token) {
        return Some(VK_F1 + n - 1);
    }
    if let Some(&(_, _, vk)) = NAMED_KEYS.iter().find(|k| k.0.eq_ignore_ascii_case(token)) {
        return Some(vk);
    }
    // Hand-written configs: a bare character such as "T", "5", "=" or "-".
    let c = single_char(token)?;
    if let Some(&(_, _, vk)) = NAMED_KEYS.iter().find(|k| k.1 == token) {
        return Some(vk);
    }
    c.is_ascii_alphanumeric().then(|| c.to_ascii_uppercase() as u32)
}

fn modifier(token: &str) -> Option<u32> {
    match token.to_ascii_lowercase().as_str() {
        "super" | "meta" | "win" | "cmd" => Some(MOD_WIN),
        "ctrl" | "control" => Some(MOD_CONTROL),
        "alt" => Some(MOD_ALT),
        "shift" => Some(MOD_SHIFT),
        _ => None,
    }
}

/// Parse "super+ctrl+KeyT" into (modifiers, virtual key).
pub fn parse(s: &str) -> Option<(u32, u32)> {
    let mut mods = 0;
    let mut vk = None;
    for token in s.split('+').map(str::trim).filter(|t| !t.is_empty()) {
        match modifier(token) {
            Some(m) => mods |= m,
            None => vk = Some(key_from_token(token)?),
        }
    }
    Some((mods, vk?))
}

/// Shift alone would steal ordinary typing, so every shortcut needs Win, Ctrl or Alt.
pub fn has_safe_modifier(mods: u32) -> bool {
    mods & (MOD_WIN | MOD_CONTROL | MOD_ALT) != 0
}

fn label_for_token(token: &str) -> String {
    if let Some(rest) = token.strip_prefix("Key").filter(|r| r.len() == 1) {
        return rest.to_uppercase();
    }
    if let Some(rest) = token.strip_prefix("Digit").filter(|r| r.len() == 1) {
        return rest.to_string();
    }
    if function_key(token).is_some() {
        return token.to_uppercase();
    }
    if let Some(k) = NAMED_KEYS.iter().find(|k| k.0.eq_ignore_ascii_case(token)) {
        return k.1.to_string();
    }
    token.to_string()
}

fn token_for_label(label: &str) -> String {
    if let Some(c) = single_char(label) {
        if c.is_ascii_alphabetic() {
            return format!("Key{}", c.to_ascii_uppercase());
        }
        if c.is_ascii_digit() {
            return format!("Digit{c}");
        }
    }
    if function_key(label).is_some() {
        return label.to_uppercase();
    }
    if let Some(k) = NAMED_KEYS.iter().find(|k| k.1.eq_ignore_ascii_case(label)) {
        return k.0.to_string();
    }
    label.to_string()
}

/// What the user sees: ["Win", "Ctrl", "T"].
pub fn display_tokens(s: &str) -> Vec<String> {
    s.split('+')
        .map(str::trim)
        .filter(|t| !t.is_empty())
        .map(|t| match modifier(t) {
            Some(MOD_WIN) => "Win".into(),
            Some(MOD_CONTROL) => "Ctrl".into(),
            Some(MOD_ALT) => "Alt".into(),
            Some(MOD_SHIFT) => "Shift".into(),
            _ => label_for_token(t),
        })
        .collect()
}

/// Every key the shortcut editor offers, as labels.
pub fn key_labels() -> Vec<String> {
    let mut keys: Vec<String> = ('A'..='Z').chain('0'..='9').map(String::from).collect();
    keys.extend((1..=FUNCTION_KEYS).map(|n| format!("F{n}")));
    keys.extend(NAMED_KEYS.iter().map(|k| k.1.to_string()));
    keys
}

/// The parts of a stored shortcut, for the editor.
pub struct Parts {
    pub win: bool,
    pub ctrl: bool,
    pub alt: bool,
    pub shift: bool,
    pub key: String,
}

pub fn split(s: &str) -> Parts {
    let mut p = Parts { win: false, ctrl: false, alt: false, shift: false, key: String::new() };
    for t in s.split('+').map(str::trim).filter(|t| !t.is_empty()) {
        match modifier(t) {
            Some(MOD_WIN) => p.win = true,
            Some(MOD_CONTROL) => p.ctrl = true,
            Some(MOD_ALT) => p.alt = true,
            Some(MOD_SHIFT) => p.shift = true,
            _ => p.key = label_for_token(t),
        }
    }
    p
}

pub fn build(p: &Parts) -> String {
    let mut parts = Vec::new();
    if p.win {
        parts.push("super".to_string());
    }
    if p.ctrl {
        parts.push("ctrl".to_string());
    }
    if p.alt {
        parts.push("alt".to_string());
    }
    if p.shift {
        parts.push("shift".to_string());
    }
    parts.push(token_for_label(&p.key));
    parts.join("+")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_defaults() {
        assert_eq!(parse("super+ctrl+KeyT"), Some((MOD_WIN | MOD_CONTROL, 'T' as u32)));
        assert_eq!(parse("super+ctrl+Equal"), Some((MOD_WIN | MOD_CONTROL, 0xBB)));
        assert_eq!(parse("ctrl+alt+shift+F13"), Some((MOD_CONTROL | MOD_ALT | MOD_SHIFT, 0x7C)));
        assert_eq!(parse("ctrl+Digit5"), Some((MOD_CONTROL, '5' as u32)));
        assert_eq!(parse("Ctrl + t"), Some((MOD_CONTROL, 'T' as u32)));
    }

    #[test]
    fn rejects_bad_input() {
        assert_eq!(parse("super+ctrl"), None);
        assert_eq!(parse("super+Nonsense"), None);
        assert_eq!(parse("F25"), None);
        assert!(!has_safe_modifier(MOD_SHIFT));
        assert!(has_safe_modifier(MOD_ALT));
    }

    #[test]
    fn displays_and_round_trips() {
        assert_eq!(display_tokens("super+ctrl+Equal"), ["Win", "Ctrl", "="]);
        for s in ["super+ctrl+KeyT", "ctrl+alt+F9", "alt+shift+PageDown", "ctrl+Digit0", "super+Backslash"] {
            assert_eq!(build(&split(s)), s);
        }
    }
}
