//
// Unit tests for PinIt's pure logic (no GUI / no live windows needed):
//  - opacity percent <-> alpha conversion is lossless (regression guard:
//    the Rust port had a bug where opacity drifted ~1% on every restart)
//  - the Tauri-style shortcut parser maps keys/modifiers correctly
//
#include <QtTest>

#include <windows.h>          // MOD_*/VK_* constants for assertions

#include "winpin.h"
#include "shortcuts.h"
#include "pinmatch.h"

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
    QVERIFY(!winpin::isPinnable(GetShellWindow()));
    if (HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr))
        QVERIFY(!winpin::isPinnable(taskbar));
    QVERIFY(!winpin::isPinnable(nullptr));
}

QTEST_MAIN(TestPinIt)
#include "test_pinit.moc"
