#include <QtTest/QTest>

#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/FrequencyViewModifiers.h>
#include <helloutau/Editor/NoteViewModifiers.h>

using namespace hello::daw;

class test_FrequencyViewModifiers : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The defaults have no conflict, and the dialog that edits frequency maps uses the scheme.
    void the_defaults_are_valid() {
        const ModifierBindings bindings(FrequencyViewModifiers::scheme());
        QVERIFY(bindings.isValid());
        QVERIFY(bindings.conflicts().isEmpty());
        QCOMPARE(Editor::modifierSchemes(Editor::FrequencyMapEditorKind),
                 QList<const ModifierScheme *>{&FrequencyViewModifiers::scheme()});
    }

    // The wheel defaults to the wheel of the note area.
    void the_wheel_is_that_of_the_note_area() {
        const ModifierBindings frequency(FrequencyViewModifiers::scheme());
        const ModifierBindings note(NoteViewModifiers::scheme());
        const std::pair<int, int> roles[] = {
            {FrequencyViewModifiers::VerticalScroll,   NoteViewModifiers::VerticalScroll  },
            {FrequencyViewModifiers::HorizontalScroll, NoteViewModifiers::HorizontalScroll},
            {FrequencyViewModifiers::TimeZoom,         NoteViewModifiers::TimeZoom        },
            {FrequencyViewModifiers::KeyZoom,          NoteViewModifiers::KeyZoom         },
        };
        for (const auto &[ours, theirs] : roles) {
            QVERIFY(frequency.modifiers(ours).has_value());
            QCOMPARE(frequency.modifiers(ours), note.modifiers(theirs));
        }
    }
};

QTEST_MAIN(test_FrequencyViewModifiers)

#include "test_FrequencyViewModifiers.moc"
