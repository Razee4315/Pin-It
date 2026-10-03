//
// Unit tests for PinIt's pure logic (no GUI / no live windows needed):
//  - opacity percent <-> alpha conversion is lossless (regression guard:
//    the Rust port had a bug where opacity drifted ~1% on every restart)
//  - the Tauri-style shortcut parser maps keys/modifiers correctly
//  - which saved pin belongs to which window, and which windows may be pinned
//  - pinned.json round-trips, reads older files and survives a corrupt one
//  - autostart follows the registry (using a scratch key)
//
#include <QtTest>

#include <windows.h>          // MOD_*/VK_* constants for assertions

#include "winpin.h"
#include "shortcuts.h"
#include "pinmatch.h"
#include "autostart.h"

#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

class TestPinIt : public QObject
{
    Q_OBJECT
private slots:
    void opacityRoundTripIsLossless();
    void opacityBounds();
    void shortcutParsesDefault();
    void shortcutMapsEqualAndMinus();
    void shortcutRejectsGarbage();
    void shortcutBuildRoundTrips();
    void shortcutBuildDisplayTokens();
    void shortcutNeedsWinCtrlOrAlt();
    void shortcutSupportsNamedAndFunctionKeys();
    void shortcutEveryOfferedKeyRoundTrips();
    void shortcutUnknownKeyIsPreserved();
    void savedPinMatchesOnlySameAppAndTitle();
    void shellWindowClassesAreNotPinnable();
    void autostartFollowsTheRegistry();
    void persistenceRoundTrips();
    void persistenceReadsOlderFiles();
    void persistenceBacksUpACorruptFile();
};

void TestPinIt::opacityRoundTripIsLossless()
{
    for (int p = winpin::kMinOpacity; p <= winpin::kMaxOpacity; ++p)
        QCOMPARE(winpin::alphaToPercent(winpin::percentToAlpha(p)), p);
}

void TestPinIt::opacityBounds()
{
    QCOMPARE(winpin::percentToAlpha(100), 255);
    QCOMPARE(winpin::alphaToPercent(255), 100);
    QCOMPARE(winpin::percentToAlpha(0), 0);
    QCOMPARE(winpin::alphaToPercent(0), 0);
}

void TestPinIt::shortcutParsesDefault()
{
    unsigned mods = 0, vk = 0;
    QVERIFY(shortcuts::parse(QStringLiteral("super+ctrl+KeyT"), mods, vk));
    QVERIFY(mods & MOD_WIN);
    QVERIFY(mods & MOD_CONTROL);
    QCOMPARE(vk, unsigned('T'));
}

void TestPinIt::shortcutMapsEqualAndMinus()
{
    unsigned mods = 0, vk = 0;
    QVERIFY(shortcuts::parse(QStringLiteral("super+ctrl+Equal"), mods, vk));
    QCOMPARE(vk, unsigned(VK_OEM_PLUS));
    QVERIFY(shortcuts::parse(QStringLiteral("super+ctrl+Minus"), mods, vk));
    QCOMPARE(vk, unsigned(VK_OEM_MINUS));
}

void TestPinIt::shortcutRejectsGarbage()
{
    unsigned mods = 0, vk = 0;
    QVERIFY(!shortcuts::parse(QStringLiteral("not a shortcut"), mods, vk));
    QVERIFY(!shortcuts::parse(QString(), mods, vk));
    QVERIFY(!shortcuts::parse(QStringLiteral("ctrl+shift"), mods, vk));  // no key
}

// What the editor dialog produces must parse back to the same modifiers + key.
// This is the path that, if broken, would silently corrupt saved shortcuts.
void TestPinIt::shortcutBuildRoundTrips()
{
    struct Case { bool win, ctrl, alt, shift; QString key;
                  unsigned mods; unsigned vk; };
    const Case cases[] = {
        { true,  true,  false, false, "T", MOD_WIN | MOD_CONTROL, unsigned('T') },
        { true,  true,  false, false, "=", MOD_WIN | MOD_CONTROL, unsigned(VK_OEM_PLUS) },
        { true,  true,  false, false, "-", MOD_WIN | MOD_CONTROL, unsigned(VK_OEM_MINUS) },
        { false, true,  true,  true,  "5", MOD_CONTROL | MOD_ALT | MOD_SHIFT, unsigned('5') },
        { true,  false, false, false, "P", MOD_WIN, unsigned('P') },
    };

    for (const Case &c : cases) {
        const QString s = shortcuts::build(c.win, c.ctrl, c.alt, c.shift, c.key);
        unsigned mods = 0, vk = 0;
        QVERIFY2(shortcuts::parse(s, mods, vk), qUtf8Printable(s));
        QCOMPARE(mods, c.mods);
        QCOMPARE(vk, c.vk);
    }
}

// build() then displayTokens() should yield human-readable chips that match
// the modifiers and key that went in.
void TestPinIt::shortcutBuildDisplayTokens()
{
    const QString s = shortcuts::build(true, true, false, false, "T");
    QCOMPARE(shortcuts::displayTokens(s),
             (QStringList{QStringLiteral("Win"), QStringLiteral("Ctrl"), QStringLiteral("T")}));

    const QString eq = shortcuts::build(true, true, false, false, "=");
    QCOMPARE(shortcuts::displayTokens(eq).last(), QStringLiteral("="));
}

// Shift+letter (or a bare key) as a global hotkey would eat normal typing.
void TestPinIt::shortcutNeedsWinCtrlOrAlt()
{
    unsigned mods = 0, vk = 0;
    QVERIFY(shortcuts::parse(QStringLiteral("shift+KeyA"), mods, vk));
    QVERIFY(!shortcuts::hasSafeModifier(mods));
    QVERIFY(shortcuts::parse(QStringLiteral("KeyA"), mods, vk));
    QVERIFY(!shortcuts::hasSafeModifier(mods));
    QVERIFY(shortcuts::parse(QStringLiteral("alt+shift+KeyA"), mods, vk));
    QVERIFY(shortcuts::hasSafeModifier(mods));
    QVERIFY(shortcuts::parse(QStringLiteral("super+KeyA"), mods, vk));
    QVERIFY(shortcuts::hasSafeModifier(mods));
}

void TestPinIt::shortcutSupportsNamedAndFunctionKeys()
{
    unsigned mods = 0, vk = 0;
    QVERIFY(shortcuts::parse(QStringLiteral("super+ctrl+F9"), mods, vk));
    QCOMPARE(vk, unsigned(VK_F9));
    QVERIFY(shortcuts::parse(QStringLiteral("ctrl+alt+F24"), mods, vk));
    QCOMPARE(vk, unsigned(VK_F24));
    QVERIFY(!shortcuts::parse(QStringLiteral("ctrl+F25"), mods, vk));
    QVERIFY(shortcuts::parse(QStringLiteral("ctrl+alt+ArrowUp"), mods, vk));
    QCOMPARE(vk, unsigned(VK_UP));
    QVERIFY(shortcuts::parse(QStringLiteral("ctrl+alt+PageDown"), mods, vk));
    QCOMPARE(vk, unsigned(VK_NEXT));
    QVERIFY(shortcuts::parse(QStringLiteral("ctrl+alt+Space"), mods, vk));
    QCOMPARE(vk, unsigned(VK_SPACE));

    QCOMPARE(shortcuts::displayTokens(QStringLiteral("super+ctrl+F9")).last(),
             QStringLiteral("F9"));
    QCOMPARE(shortcuts::displayTokens(QStringLiteral("ctrl+alt+PageDown")).last(),
             QStringLiteral("Page Down"));
}

// Whatever the editor lets the user pick must build into something that
// parses, and must show the same label again when the dialog is reopened.
void TestPinIt::shortcutEveryOfferedKeyRoundTrips()
{
    const QStringList labels = shortcuts::keyLabels();
    QVERIFY(labels.size() > 60);
    for (const QString &label : labels) {
        const QString s = shortcuts::build(true, true, false, false, label);
        unsigned mods = 0, vk = 0;
        QVERIFY2(shortcuts::parse(s, mods, vk), qUtf8Printable(s));
        QCOMPARE(mods, unsigned(MOD_WIN | MOD_CONTROL));
        QCOMPARE(shortcuts::displayTokens(s).last(), label);
    }
}

// A key token the editor doesn't know must survive display -> build unchanged,
// not be rewritten into something else when the user presses OK.
void TestPinIt::shortcutUnknownKeyIsPreserved()
{
    const QString original = QStringLiteral("super+ctrl+NumpadAdd");
    const QStringList tokens = shortcuts::displayTokens(original);
    QCOMPARE(tokens.last(), QStringLiteral("NumpadAdd"));
    QCOMPARE(shortcuts::build(true, true, false, false, tokens.last()), original);
}

// Restoring must never fall back to "any window of the same app".
void TestPinIt::savedPinMatchesOnlySameAppAndTitle()
{
    persistence::SavedPin saved;
    saved.processName = QStringLiteral("notepad.exe");
    saved.title = QStringLiteral("notes.txt - Notepad");

    QVERIFY(pinmatch::matches(saved, QStringLiteral("notepad.exe"), saved.title));
    QVERIFY(pinmatch::matches(saved, QStringLiteral("Notepad.EXE"), saved.title));
    QVERIFY(!pinmatch::matches(saved, QStringLiteral("notepad.exe"),
                               QStringLiteral("other.txt - Notepad")));
    QVERIFY(!pinmatch::matches(saved, QStringLiteral("explorer.exe"), saved.title));

    saved.title.clear();   // legacy entries without a title match nothing
    QVERIFY(!pinmatch::matches(saved, QStringLiteral("notepad.exe"), QString()));
}

void TestPinIt::shellWindowClassesAreNotPinnable()
{
    QVERIFY(winpin::isShellClass(QStringLiteral("Progman")));
    QVERIFY(winpin::isShellClass(QStringLiteral("Shell_TrayWnd")));
    QVERIFY(winpin::isShellClass(QStringLiteral("Windows.UI.Core.CoreWindow")));
    QVERIFY(!winpin::isShellClass(QStringLiteral("Notepad")));
    QVERIFY(!winpin::isShellClass(QStringLiteral("ApplicationFrameWindow")));   // UWP apps
    QVERIFY(!winpin::isShellClass(QString()));

    // The live desktop and taskbar, and a window of this very process.
    QVERIFY(!winpin::isPinnable(reinterpret_cast<winpin::WindowId>(GetShellWindow())));
    if (HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr))
        QVERIFY(!winpin::isPinnable(reinterpret_cast<winpin::WindowId>(taskbar)));
    QVERIFY(!winpin::isPinnable(0));
}

// Uses a scratch key, never the real Run key.
void TestPinIt::autostartFollowsTheRegistry()
{
    const QString key = QStringLiteral("HKEY_CURRENT_USER\\Software\\PinIt-Tests\\Run-%1")
                            .arg(QCoreApplication::applicationPid());
    autostart::setRegistryKeyForTesting(key);

    QVERIFY(!autostart::isEnabled());
    QVERIFY(!autostart::repairPath());            // nothing to repair when off

    autostart::setEnabled(true);
    QVERIFY(autostart::isEnabled());
    QVERIFY(autostart::command().endsWith(QStringLiteral("\" --minimized")));
    QCOMPARE(QSettings(key, QSettings::NativeFormat).value(QStringLiteral("PinIt")).toString(),
             autostart::command());
    QVERIFY(!autostart::repairPath());            // already points here

    // Another copy that still exists (an installed PinIt) owns the entry:
    // autostart counts as on, and its path is not taken over.
    const QString other = QStringLiteral("\"%1\" --minimized").arg(qEnvironmentVariable("ComSpec"));
    QSettings(key, QSettings::NativeFormat).setValue(QStringLiteral("PinIt"), other);
    QVERIFY(autostart::isEnabled());              // the registry is the truth
    QVERIFY(!autostart::repairPath());
    QCOMPARE(QSettings(key, QSettings::NativeFormat).value(QStringLiteral("PinIt")).toString(),
             other);

    // The registered copy is gone (a portable folder that was moved): repair.
    QSettings(key, QSettings::NativeFormat)
        .setValue(QStringLiteral("PinIt"), QStringLiteral("\"C:\\No Such Folder\\PinIt.exe\" --minimized"));
    QVERIFY(autostart::repairPath());
    QCOMPARE(QSettings(key, QSettings::NativeFormat).value(QStringLiteral("PinIt")).toString(),
             autostart::command());

    autostart::setEnabled(false);
    QVERIFY(!autostart::isEnabled());

    QSettings(QStringLiteral("HKEY_CURRENT_USER\\Software"), QSettings::NativeFormat)
        .remove(QStringLiteral("PinIt-Tests"));
}

namespace {

// Point persistence at an empty scratch folder instead of the real
// %LOCALAPPDATA%\\PinIt for the lifetime of a test.
struct ScratchDataDir {
    ScratchDataDir()
    {
        qputenv("LOCALAPPDATA", dir.path().toLocal8Bit());
        persistence::dropCache();
    }
    ~ScratchDataDir()
    {
        qputenv("LOCALAPPDATA", previous);
        persistence::dropCache();
    }

    QString file() const { return dir.filePath(QStringLiteral("PinIt/pinned.json")); }
    void write(const QByteArray &content) const
    {
        QDir().mkpath(dir.filePath(QStringLiteral("PinIt")));
        QFile f(file());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
        f.close();
        persistence::dropCache();   // the file changed behind PinIt's back
    }

    QTemporaryDir dir;
    QByteArray previous = qgetenv("LOCALAPPDATA");
};

} // namespace

void TestPinIt::persistenceRoundTrips()
{
    const ScratchDataDir scratch;
    QVERIFY(scratch.dir.isValid());
    QCOMPARE(QDir::cleanPath(persistence::dataDir()),
             QDir::cleanPath(scratch.dir.filePath(QStringLiteral("PinIt"))));

    // No file yet: defaults.
    persistence::SavedState state = persistence::load();
    QVERIFY(state.pins.isEmpty());
    QVERIFY(state.settings.enableSound);
    QVERIFY(!state.settings.showNotifications);
    QCOMPARE(state.settings.shortcuts.togglePin, QStringLiteral("super+ctrl+KeyT"));

    persistence::SavedPin a;
    a.processName = QStringLiteral("notepad.exe");
    a.title = QStringLiteral("notes.txt - Notepad");
    a.opacity = 153;
    a.clickThrough = true;
    a.wasLayered = false;
    a.wasTopmost = false;
    a.wasClickThrough = false;
    persistence::SavedPin b;
    b.processName = QStringLiteral("chrome.exe");
    b.title = QStringLiteral("R&D \u2014 \u00fcber");   // non-ASCII survives
    persistence::savePins({a, b});

    // Saving settings must not lose the pins, and the other way round.
    persistence::UserSettings settings;
    settings.enableSound = false;
    settings.showNotifications = true;
    settings.startWithWindows = true;
    settings.hasSeenTrayNotice = true;
    settings.shortcuts.togglePin = QStringLiteral("ctrl+alt+F9");
    persistence::saveSettings(settings);
    persistence::savePins({a, b});

    persistence::dropCache();   // compare against what is in the file
    state = persistence::load();
    QCOMPARE(state.pins.size(), 2);
    // The file keys pins by "<process>:<index>", so they come back sorted by key.
    const persistence::SavedPin &chrome = state.pins[0];
    const persistence::SavedPin &notepad = state.pins[1];
    QCOMPARE(chrome.processName, b.processName);
    QCOMPARE(chrome.title, b.title);
    QCOMPARE(chrome.opacity, 255);
    QVERIFY(!chrome.clickThrough);
    QVERIFY(chrome.wasLayered && chrome.wasTopmost && chrome.wasClickThrough);
    QCOMPARE(notepad.processName, a.processName);
    QCOMPARE(notepad.title, a.title);
    QCOMPARE(notepad.opacity, 153);
    QVERIFY(notepad.clickThrough);
    QVERIFY(!notepad.wasLayered && !notepad.wasTopmost && !notepad.wasClickThrough);

    QVERIFY(!state.settings.enableSound);
    QVERIFY(state.settings.showNotifications);
    QVERIFY(state.settings.startWithWindows);
    QVERIFY(state.settings.hasSeenTrayNotice);
    QCOMPARE(state.settings.shortcuts.togglePin, QStringLiteral("ctrl+alt+F9"));
    QCOMPARE(state.settings.shortcuts.opacityUp, QStringLiteral("super+ctrl+Equal"));

    persistence::savePins({});
    persistence::dropCache();
    QVERIFY(persistence::load().pins.isEmpty());
    QVERIFY(!persistence::load().settings.enableSound);   // settings untouched
}

// A file written by PinIt 2.1 / the Tauri versions has none of the newer keys.
void TestPinIt::persistenceReadsOlderFiles()
{
    const ScratchDataDir scratch;
    scratch.write(R"({
        "pins": { "notepad.exe:0": { "process_name": "notepad.exe", "title": "a.txt - Notepad", "opacity": 204 },
                  ":1": { "process_name": "", "title": "no process", "opacity": 255 } },
        "settings": { "enable_sound": false,
                      "shortcuts": { "toggle_pin": "super+shift+KeyP" } } })");

    const persistence::SavedState state = persistence::load();
    QCOMPARE(state.pins.size(), 1);                 // the entry without a process is dropped
    QCOMPARE(state.pins[0].title, QStringLiteral("a.txt - Notepad"));
    QCOMPARE(state.pins[0].opacity, 204);
    QVERIFY(!state.pins[0].clickThrough);
    QVERIFY(state.pins[0].wasLayered && state.pins[0].wasTopmost);   // "unknown" defaults
    QVERIFY(!state.settings.enableSound);
    QVERIFY(!state.settings.showNotifications);     // new setting: off unless asked for
    QCOMPARE(state.settings.shortcuts.togglePin, QStringLiteral("super+shift+KeyP"));
    QCOMPARE(state.settings.shortcuts.toggleWindow, QStringLiteral("super+ctrl+KeyP"));
}

void TestPinIt::persistenceBacksUpACorruptFile()
{
    const ScratchDataDir scratch;
    scratch.write("{ \"pins\": { oops");

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("pinned.json is corrupt")));
    const persistence::SavedState state = persistence::load();
    QVERIFY(state.pins.isEmpty());
    QVERIFY(state.settings.enableSound);

    QFile backup(scratch.file() + QStringLiteral(".corrupt"));
    QVERIFY(backup.open(QIODevice::ReadOnly));
    QCOMPARE(backup.readAll(), QByteArray("{ \"pins\": { oops"));

    // The next save replaces the broken file with a valid one.
    persistence::saveSettings(state.settings);
    persistence::dropCache();
    QVERIFY(persistence::load().settings.enableSound);
    QFile f(scratch.file());
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.readAll().contains("\"settings\""));
}

QTEST_MAIN(TestPinIt)
#include "test_pinit.moc"
