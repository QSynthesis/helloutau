#include <memory>

#include <QtCore/QJsonArray>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/Edit/VoiceBankSession.h>

#include <hellokit/EditBase/private/NodeCommands_p.h>

#include "VoiceBankFields_p.h"
#include "VoiceBankSamples.h"

using namespace hello::kit;

namespace fs = std::filesystem;

class test_VoiceBankSession : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::optional<VoiceBank> m_bank;
    std::unique_ptr<VoiceBankSession> m_session;

    template <class Edit>
    QList<QJsonObject> logOf(Edit edit) {
        QList<QJsonObject> entries;
        const auto connection =
            QObject::connect(m_session.get(), &edit::EditSession::changed, m_session.get(),
                             [&](const edit::ChangePtr &change) {
                                 if (const auto entry = m_session->logEntry(*change)) {
                                     entries.push_back(*entry);
                                 }
                             });
        auto transaction = m_session->transaction(QStringLiteral("Edit"));
        edit();
        transaction.commit();
        QObject::disconnect(connection);
        return entries;
    }

    // Applies edit in one transaction and returns whether it was committed.
    template <class Edit>
    bool commit(Edit edit, DiagnosticList &diagnostics) {
        auto transaction = m_session->transaction(QStringLiteral("Edit"));
        edit();
        return transaction.commit(diagnostics);
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        auto opened = openRichBank(m_dir->path());
        QVERIFY(opened.has_value());
        m_bank = opened->bank;
        m_session = std::make_unique<VoiceBankSession>(std::move(*opened));
    }

    void cleanup() {
        m_session.reset();
        m_bank.reset();
        m_dir.reset();
    }

    // The snapshot is what saving and synthesis receive, including the samples without an entry,
    // which the tree does not hold.
    void the_snapshot_is_the_voice_bank_read() {
        QCOMPARE(m_session->rootPath(), m_bank->root());
        verifyEqual(m_session->snapshot(), *m_bank);
        verifyEqual(VoiceBankRef(m_session.get()).toVoiceBank(), *m_bank);
    }

    // A value is logged with the name of its field in VoiceBankSchema.h and its values.
    void a_value_is_logged_with_its_field_name_and_values() {
        const auto entry = VoiceBankRef(m_session.get()).directories().at(0).otoEntries().at(3);
        const auto entries = logOf([&] { entry.setAlias(QStringLiteral("n")); });
        QCOMPARE(entries, QList<QJsonObject>({
                              QJsonObject{{QStringLiteral("node"), qint64(entry.id())},
                                          {QStringLiteral("shape"), QStringLiteral("set")},
                                          {QStringLiteral("slot"), QStringLiteral("alias")},
                                          {QStringLiteral("before"), QStringLiteral("m")},
                                          {QStringLiteral("after"), QStringLiteral("n")}},
        }));
    }

    // A prefix is logged under its note number as the JSON a command writes.
    void a_prefix_is_logged_with_its_key_and_json() {
        const auto prefixMap = VoiceBankRef(m_session.get()).prefixMap();
        const auto entries = logOf(
            [&] { prefixMap.setValue(60, VoicePrefix{QStringLiteral("q"), QStringLiteral("s")}); });
        QCOMPARE(entries,
                 QList<QJsonObject>({
                     QJsonObject{{QStringLiteral("node"), qint64(prefixMap.id())},
                                 {QStringLiteral("shape"), QStringLiteral("entry")},
                                 {QStringLiteral("key"), QStringLiteral("60")},
                                 {QStringLiteral("before"),
                                  QJsonObject{{QStringLiteral("prefix"), QStringLiteral("p")},
                                              {QStringLiteral("suffix"), QStringLiteral("s")}}},
                                 {QStringLiteral("after"),
                                  QJsonObject{{QStringLiteral("prefix"), QStringLiteral("q")},
                                              {QStringLiteral("suffix"), QStringLiteral("s")}}}},
        }));
    }

    // Every entry names its audio file.
    void an_entry_without_a_file_name_is_refused() {
        const auto entry = VoiceBankRef(m_session.get()).directories().at(0).otoEntries().at(3);
        DiagnosticList diagnostics;
        QVERIFY(!commit([&] { entry.setFileName(QString()); }, diagnostics));
        QCOMPARE(diagnostics.first().message,
                 QStringLiteral("The oto entry with the alias \"m\" has no file name."));
        QCOMPARE(entry.fileName(), QStringLiteral("missing.wav"));
    }

    // The entries of one audio file have distinct aliases, and an empty alias counts as the stem
    // of the file name. Other audio files may use the same alias.
    void an_alias_repeated_for_one_audio_file_is_refused() {
        const auto entries = VoiceBankRef(m_session.get()).directories().at(0).otoEntries();
        const auto first = entries.at(0).alias();
        DiagnosticList diagnostics;
        QVERIFY(!commit([&] { entries.at(1).setAlias(first); }, diagnostics));
        QCOMPARE(
            diagnostics.first().message,
            QStringLiteral("The alias \"%1\" occurs more than once for \"a.wav\".").arg(first));

        VoiceOtoEntry stem;
        stem.fileName = QStringLiteral("b.wav");
        stem.alias = QStringLiteral("b");
        diagnostics.clear();
        QVERIFY(!commit([&] { entries.insert(0, {stem, stem}); }, diagnostics));
        QCOMPARE(diagnostics.size(), 1);

        QVERIFY(commit([&] { entries.at(3).setAlias(first); }, diagnostics));
    }

    // A key of prefix.map is a note number from C1 to B7.
    void a_prefix_outside_the_keys_is_refused() {
        const auto prefixMap = VoiceBankRef(m_session.get()).prefixMap();
        DiagnosticList diagnostics;
        QVERIFY(!commit([&] { prefixMap.setValue(23, VoicePrefix()); }, diagnostics));
        QCOMPARE(diagnostics.first().message,
                 QStringLiteral("The prefix map has the key \"23\", which is not a note number "
                                "from 24 to 107."));
        QVERIFY(!commit([&] { prefixMap.setValue(108, VoicePrefix()); }, diagnostics));
        QVERIFY(commit(
            [&] {
                prefixMap.setValue(24, VoicePrefix());
                prefixMap.setValue(107, VoicePrefix());
            },
            diagnostics));
    }

    // A command writes a key as text, and another spelling of a note number would be a second
    // key for the same note.
    void a_prefix_key_in_another_spelling_is_refused() {
        DiagnosticList diagnostics;
        for (const auto &key :
             {QStringLiteral("\"060\""), QStringLiteral("\"+60\""), QStringLiteral("\"60 \""),
              QStringLiteral("x"), QStringLiteral("\"\"")}) {
            const auto line = QStringLiteral("set /prefixMap %1 {}").arg(key);
            const auto arguments = edit::CommandSyntax::split(line, diagnostics);
            QVERIFY(arguments && arguments->size() == 4);
            QVERIFY2(!commit(
                         [&] {
                             QVERIFY(edit::NodeCommands::execute(*m_session, voiceBankRecord(),
                                                                 u"set", arguments->mid(1),
                                                                 diagnostics));
                         },
                         diagnostics),
                     qPrintable(line));
        }
        QVERIFY(VoiceBankRef(m_session.get()).prefixMap().keys() == QList<int>({60, 62}));
    }

    // A voice bank read from disk may violate its constraints, which does not prevent editing
    // the violating directory otherwise.
    void a_violation_read_from_disk_does_not_prevent_editing() {
        QTemporaryDir dir;
        QVERIFY(writeSampleFile(dir.path(), QStringLiteral("oto.ini"),
                                "a.wav=x,1,2,3,4,5\r\na.wav=x,1,2,3,4,5\r\n"));
        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        auto opened =
            VoiceBankDiskState::open(fs::path(dir.path().toStdU16String()), &selector, diagnostics);
        QVERIFY(opened);
        m_session = std::make_unique<VoiceBankSession>(std::move(*opened));
        const auto entries = VoiceBankRef(m_session.get()).directories().at(0).otoEntries();
        QVERIFY(commit([&] { entries.at(1).setOffset(10); }, diagnostics));
        QVERIFY(!commit([&] { entries.at(1).setFileName(QString()); }, diagnostics));
        m_session.reset();
    }

    // A directory that was not read, or whose text did not decode, cannot be saved and is not
    // edited. The other directories are.
    void a_directory_that_cannot_be_saved_is_not_edited() {
        const auto root = VoiceBankRef(m_session.get());
        const auto left = root.directories().at(m_bank->indexOf("left"));
        const auto deep = root.directories().at(m_bank->indexOf(fs::path("sub") / "deep"));
        VoiceOtoEntry entry;
        entry.fileName = QStringLiteral("z.wav");

        DiagnosticList diagnostics;
        QVERIFY(!commit([&] { left.otoEntries().insert(0, {entry}); }, diagnostics));
        QVERIFY(diagnostics.first().message.contains(QStringLiteral("\"left\" was not read")));

        diagnostics.clear();
        QVERIFY(
            !commit([&] { deep.otoEntries().at(0).setAlias(QStringLiteral("y")); }, diagnostics));
        QVERIFY(diagnostics.first().message.contains(QStringLiteral("\"sub/deep\"")));
        QVERIFY(diagnostics.first().message.contains(QStringLiteral("not valid")));

        QVERIFY(commit(
            [&] { root.directories().at(m_bank->indexOf("sub")).otoEntries().at(0).setOffset(7); },
            diagnostics));
    }

    // A replaced character is logged as its JSON after the change.
    void a_replaced_character_is_logged_as_its_json() {
        const auto root = VoiceBankRef(m_session.get());
        VoiceCharacter character;
        character.name = QStringLiteral("n");
        const auto entries = logOf([&] { root.setCharacter(character); });
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().value(QStringLiteral("slot")),
                 QJsonValue(QStringLiteral("character")));
        QCOMPARE(
            entries.first().value(QStringLiteral("after")).toObject().value(QStringLiteral("name")),
            QJsonValue(QStringLiteral("n")));
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankSession)

#include "test_VoiceBankSession.moc"
