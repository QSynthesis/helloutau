#include <QtTest/QTest>

#include <hellokit/NativeSystem/NativeMessageBox.h>

using namespace hello::kit;

using Buttons = std::vector<NativeMessageBox::StandardButton>;

// The tests cover the arrangement of the buttons only, because a box that is shown waits for the
// user.
class test_NativeMessageBox : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_box_without_buttons_has_ok() {
        const auto layout =
            NativeMessageBox::layoutOf(NativeMessageBox::NoButton, NativeMessageBox::NoButton);
        QVERIFY(layout.buttons == Buttons{NativeMessageBox::Ok});
        QCOMPARE(layout.defaultButton, NativeMessageBox::Ok);
        QCOMPARE(layout.escapeButton, NativeMessageBox::Ok);
    }

    // The buttons are in the order of their values, whichever order the caller combines them in,
    // and the first is the default button unless another is given.
    void the_buttons_are_in_the_order_of_their_values() {
        auto layout = NativeMessageBox::layoutOf(
            NativeMessageBox::Quit | NativeMessageBox::Continue, NativeMessageBox::NoButton);
        QVERIFY(layout.buttons == (Buttons{NativeMessageBox::Continue, NativeMessageBox::Quit}));
        QCOMPARE(layout.defaultButton, NativeMessageBox::Continue);
        QCOMPARE(layout.escapeButton, NativeMessageBox::Quit);

        layout = NativeMessageBox::layoutOf(NativeMessageBox::Continue | NativeMessageBox::Quit,
                                            NativeMessageBox::Quit);
        QCOMPARE(layout.defaultButton, NativeMessageBox::Quit);
    }

    void a_default_button_not_in_the_box_is_the_first_button() {
        const auto layout = NativeMessageBox::layoutOf(NativeMessageBox::Yes | NativeMessageBox::No,
                                                       NativeMessageBox::Cancel);
        QCOMPARE(layout.defaultButton, NativeMessageBox::Yes);
    }

    // Cancel is the escape button before No, and the last button is the escape button if
    // there is no Cancel, No or Quit.
    void the_escape_button_is_cancel_no_quit_or_the_last_button() {
        auto layout = NativeMessageBox::layoutOf(NativeMessageBox::Yes | NativeMessageBox::No |
                                                     NativeMessageBox::Cancel,
                                                 NativeMessageBox::NoButton);
        QCOMPARE(layout.escapeButton, NativeMessageBox::Cancel);

        layout = NativeMessageBox::layoutOf(NativeMessageBox::Yes | NativeMessageBox::No,
                                            NativeMessageBox::NoButton);
        QCOMPARE(layout.escapeButton, NativeMessageBox::No);

        layout = NativeMessageBox::layoutOf(NativeMessageBox::Ok | NativeMessageBox::Continue,
                                            NativeMessageBox::NoButton);
        QCOMPARE(layout.escapeButton, NativeMessageBox::Continue);
    }

    void a_box_has_at_most_three_buttons() {
        const auto layout = NativeMessageBox::layoutOf(0x3f, NativeMessageBox::Quit);
        QVERIFY(layout.buttons ==
                (Buttons{NativeMessageBox::Ok, NativeMessageBox::Cancel, NativeMessageBox::Yes}));
        QCOMPARE(layout.defaultButton, NativeMessageBox::Ok);
        QCOMPARE(layout.escapeButton, NativeMessageBox::Cancel);
    }

    void every_button_has_an_english_name() {
        for (const auto button :
             {NativeMessageBox::Ok, NativeMessageBox::Cancel, NativeMessageBox::Yes,
              NativeMessageBox::No, NativeMessageBox::Continue, NativeMessageBox::Quit}) {
            QVERIFY(!NativeMessageBox::nameOf(button).empty());
        }
        QVERIFY(NativeMessageBox::nameOf(NativeMessageBox::NoButton).empty());
    }
};

QTEST_APPLESS_MAIN(test_NativeMessageBox)

#include "test_NativeMessageBox.moc"
