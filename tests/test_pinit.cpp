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

QTEST_MAIN(TestPinIt)
#include "test_pinit.moc"
