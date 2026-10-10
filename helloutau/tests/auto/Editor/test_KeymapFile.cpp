#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <QAKCore/actionregistry.h>

#include <helloutau/Editor/KeymapFile.h>

using namespace hello::daw;

namespace {

    // A scheme of the test: Move without modifiers and Copy with Ctrl start a drag, Snap Off
    // with Alt toggles.
    enum Role { Move, Copy, SnapOff };

    const ModifierScheme &scheme() {
        static const ModifierScheme scheme("drag", "test_KeymapFile", "Drag",
                                           {
                                               {Move,    "move",    "Move",     Qt::NoModifier     },
                                               {Copy,    "copy",    "Copy",     Qt::ControlModifier},
                                               {SnapOff, "snapOff", "Snap Off", Qt::AltModifier    },
        },
                                           {{0, {{Move, {SnapOff}}, {Copy, {SnapOff}}}}});
        return scheme;
    }

    QAK::ActionFamily::ShortcutsOverride keys(const char *sequence) {
        return QList<QKeySequence>{QKeySequence(QString::fromLatin1(sequence))};
    }

    void writeText(const QString &fileName, const QByteArray &text) {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(text);
    }

    QByteArray versioned(const QByteArray &section) {
        return "{\"version\": " + QByteArray::number(KeymapFile::version) +
               ", \"projectWindow\": " + section + "}";
    }

}

class test_KeymapFile : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QAK::ActionRegistry> m_registry;
    KeymapFile::Sections m_sections;

    QString fileName() const {
        return m_dir->filePath(QStringLiteral("keymap.json"));
    }

    // Reads the file into fresh sections, which start with the defaults.
    KeymapFile::Sections readBack(QAK::ActionRegistry &registry) const {
        KeymapFile::Sections sections{
            {QStringLiteral("projectWindow"), &registry, {ModifierBindings(scheme())}}
        };
        KeymapFile::read(sections, fileName());
        return sections;
    }

    // Verifies that reading the file leaves the defaults, with a warning matching \a pattern.
    void verifyIgnored(const char *pattern) {
        QAK::ActionRegistry registry;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QString::fromLatin1(pattern)));
        const auto sections = readBack(registry);
        QVERIFY(registry.shortcutsFamily().isEmpty());
        QVERIFY(sections[0].modifiers[0] == ModifierBindings(scheme()));
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        m_registry = std::make_unique<QAK::ActionRegistry>();
        m_sections = {
            {QStringLiteral("projectWindow"), m_registry.get(), {ModifierBindings(scheme())}}
        };
    }

    void the_assigned_shortcuts_and_modifiers_survive_a_round_trip() {
        m_registry->setShortcuts(QStringLiteral("helloutau.edit.undo"), keys("Ctrl+Alt+Z"));
        // An empty list leaves the command without a shortcut.
        m_registry->setShortcuts(QStringLiteral("helloutau.edit.redo"), QList<QKeySequence>());
        m_sections[0].modifiers[0].setModifiers(Copy, Qt::MetaModifier);
        QString error;
        QVERIFY2(KeymapFile::write(m_sections, fileName(), &error), qPrintable(error));

        QAK::ActionRegistry registry;
        const auto sections = readBack(registry);
        QCOMPARE(registry.shortcuts(QStringLiteral("helloutau.edit.undo")), keys("Ctrl+Alt+Z"));
        QCOMPARE(registry.shortcuts(QStringLiteral("helloutau.edit.redo")),
                 QAK::ActionFamily::ShortcutsOverride(QList<QKeySequence>()));
        QVERIFY(sections[0].modifiers[0] == m_sections[0].modifiers[0]);
    }

    // Only the shortcuts and the modifiers that the user has assigned are written, with the
    // version of the format.
    void only_the_assignments_are_written() {
        m_registry->setShortcuts(QStringLiteral("helloutau.edit.undo"), keys("Ctrl+Alt+Z"));
        m_registry->setShortcuts(QStringLiteral("helloutau.edit.cut"), std::nullopt);
        QVERIFY(KeymapFile::write(m_sections, fileName(), nullptr));
        QFile file(fileName());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(root.value(QStringLiteral("version")).toInt(), KeymapFile::version);
        const auto section = root.value(QStringLiteral("projectWindow")).toObject();
        QCOMPARE(section.value(QStringLiteral("shortcuts")).toArray().size(), 1);
        QVERIFY(!section.contains(QStringLiteral("modifiers")));
    }

    void a_missing_file_leaves_the_defaults() {
        QAK::ActionRegistry registry;
        const auto sections = readBack(registry);
        QVERIFY(registry.shortcutsFamily().isEmpty());
        QVERIFY(sections[0].modifiers[0] == ModifierBindings(scheme()));
    }

    // A file that is not a keymap, or a section that is not one, is ignored with a warning.
    void a_malformed_file_is_ignored() {
        writeText(fileName(), "not json");
        verifyIgnored("is not a keymap");
        writeText(fileName(), "[1]");
        verifyIgnored("is not a keymap");
        writeText(fileName(), versioned("[]"));
        verifyIgnored("is not a keymap");
        writeText(fileName(), versioned("{\"modifiers\": {}}"));
        verifyIgnored("is not a keymap");
    }

    // A file of another version, or without a version, is not read.
    void a_file_of_another_version_is_ignored() {
        const QByteArray section =
            "{\"shortcuts\": [{\"id\": \"helloutau.edit.undo\", \"keys\": [\"Ctrl+Alt+Z\"]}], "
            "\"modifiers\": {\"drag\": {\"copy\": [\"Meta\"]}}}";
        for (const QByteArray &version : {QByteArray(), QByteArray::number(KeymapFile::version - 1),
                                          QByteArray::number(KeymapFile::version + 1)}) {
            writeText(fileName(), version.isEmpty() ? "{\"projectWindow\": " + section + "}"
                                                    : "{\"version\": " + version +
                                                          ", \"projectWindow\": " + section + "}");
            verifyIgnored("instead of");
        }

        writeText(fileName(), versioned(section));
        QAK::ActionRegistry registry;
        const auto sections = readBack(registry);
        QCOMPARE(registry.shortcuts(QStringLiteral("helloutau.edit.undo")), keys("Ctrl+Alt+Z"));
        QCOMPARE(sections[0].modifiers[0].modifiers(Copy),
                 std::optional<Qt::KeyboardModifiers>(Qt::MetaModifier));
    }

    // Unknown roles are ignored with a warning, and bindings that conflict give way to the
    // defaults of their scheme with a warning.
    void modifiers_that_do_not_apply_are_ignored() {
        writeText(fileName(), versioned("{\"shortcuts\": [], \"modifiers\": {\"drag\": "
                                        "{\"copy\": [\"Meta\"], \"unknown\": [\"Ctrl\"]}}}"));
        QAK::ActionRegistry registry;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unknown")));
        auto sections = readBack(registry);
        QCOMPARE(sections[0].modifiers[0].modifiers(Copy),
                 std::optional<Qt::KeyboardModifiers>(Qt::MetaModifier));

        // Copy without modifiers starts like Move.
        writeText(fileName(),
                  versioned("{\"shortcuts\": [], \"modifiers\": {\"drag\": {\"copy\": []}}}"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("conflict")));
        sections = readBack(registry);
        QVERIFY(sections[0].modifiers[0] == ModifierBindings(scheme()));
    }
};

QTEST_GUILESS_MAIN(test_KeymapFile)

#include "test_KeymapFile.moc"
