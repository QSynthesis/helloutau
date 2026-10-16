#include <QtTest/QTest>

#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/NoteViewKeyboardModifiers.h>
#include <helloutau/Editor/OtoWaveformKeyboardModifiers.h>

using namespace hello::daw;

class test_OtoWaveformKeyboardModifiers : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The defaults have no conflict, and the voice bank window uses the scheme.
    void the_defaults_are_valid() {
        const KeyboardModifierBindings bindings(OtoWaveformKeyboardModifiers::scheme());
        QVERIFY(bindings.isValid());
        QVERIFY(bindings.conflicts().isEmpty());
        QCOMPARE(Editor::keyboardModifierSchemes(Editor::VoiceBankWindowKind),
                 QList<const KeyboardModifierScheme *>{&OtoWaveformKeyboardModifiers::scheme()});
    }

    // The wheel scrolls without modifiers, and the zoom is that of the time in the note area.
    void the_wheel_scrolls_and_zooms_the_time() {
        const KeyboardModifierBindings waveform(OtoWaveformKeyboardModifiers::scheme());
        const KeyboardModifierBindings note(NoteViewKeyboardModifiers::scheme());
        QCOMPARE(waveform.modifiers(OtoWaveformKeyboardModifiers::HorizontalScroll),
                 std::optional<Qt::KeyboardModifiers>(Qt::NoModifier));
        QVERIFY(waveform.modifiers(OtoWaveformKeyboardModifiers::TimeZoom).has_value());
        QCOMPARE(waveform.modifiers(OtoWaveformKeyboardModifiers::TimeZoom),
                 note.modifiers(NoteViewKeyboardModifiers::TimeZoom));
    }
};

QTEST_MAIN(test_OtoWaveformKeyboardModifiers)

#include "test_OtoWaveformKeyboardModifiers.moc"
