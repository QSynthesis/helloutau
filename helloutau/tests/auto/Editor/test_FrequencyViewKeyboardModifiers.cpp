#include <QtTest/QTest>

#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/FrequencyViewKeyboardModifiers.h>
#include <helloutau/Editor/NoteViewKeyboardModifiers.h>

using namespace hello::daw;

class test_FrequencyViewKeyboardModifiers : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The defaults have no conflict, and the dialog that edits frequency maps uses the scheme.
    void the_defaults_are_valid() {
        const KeyboardModifierBindings bindings(FrequencyViewKeyboardModifiers::scheme());
        QVERIFY(bindings.isValid());
        QVERIFY(bindings.conflicts().isEmpty());
        QCOMPARE(Editor::keyboardModifierSchemes(Editor::FrequencyMapEditorKind),
                 QList<const KeyboardModifierScheme *>{&FrequencyViewKeyboardModifiers::scheme()});
    }

    // The wheel defaults to the wheel of the note area.
    void the_wheel_is_that_of_the_note_area() {
        const KeyboardModifierBindings frequency(FrequencyViewKeyboardModifiers::scheme());
        const KeyboardModifierBindings note(NoteViewKeyboardModifiers::scheme());
        const std::pair<int, int> roles[] = {
            {FrequencyViewKeyboardModifiers::VerticalScroll,   NoteViewKeyboardModifiers::VerticalScroll  },
            {FrequencyViewKeyboardModifiers::HorizontalScroll, NoteViewKeyboardModifiers::HorizontalScroll},
            {FrequencyViewKeyboardModifiers::TimeZoom,         NoteViewKeyboardModifiers::TimeZoom        },
            {FrequencyViewKeyboardModifiers::KeyZoom,          NoteViewKeyboardModifiers::KeyZoom         },
        };
        for (const auto &[ours, theirs] : roles) {
            QVERIFY(frequency.modifiers(ours).has_value());
            QCOMPARE(frequency.modifiers(ours), note.modifiers(theirs));
        }
    }
};

QTEST_MAIN(test_FrequencyViewKeyboardModifiers)

#include "test_FrequencyViewKeyboardModifiers.moc"
