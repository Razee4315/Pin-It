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

QTEST_MAIN(TestPinIt)
#include "test_pinit.moc"
