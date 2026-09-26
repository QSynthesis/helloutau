#include <memory>

#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Edit/VoiceBankEdits.h>
#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/Edit/VoiceBankSession.h>

#include "VoiceBankSamples.h"

using namespace hello::kit;

namespace fs = std::filesystem;

class test_VoiceBankEdits : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::optional<VoiceBank> m_bank;
    std::unique_ptr<VoiceBankSession> m_session;

    VoiceBankRef bank() const {
        return VoiceBankRef(m_session.get());
    }

    VoiceDirectoryRef directory(const fs::path &path) const {
        const auto index = m_bank->indexOf(path);
        Q_ASSERT(index >= 0);
        return bank().directories().at(index);
    }

    static QStringList fileNames(const VoiceDirectoryRef &directory) {
        QStringList names;
        const auto entries = directory.otoEntries();
        for (int i = 0; i < entries.size(); ++i) {
            names.push_back(entries.at(i).fileName());
        }
        return names;
    }

    static VoiceOtoEntry entryOf(const QString &fileName, const QString &alias) {
        VoiceOtoEntry entry;
        entry.fileName = fileName;
        entry.alias = alias;
        return entry;
    }

    // Verifies that a refused function leaves the session unchanged.
    template <class Edit>
    void verifyRefused(Edit edit, const QString &reason) {
        const auto step = m_session->currentStep();
        DiagnosticList diagnostics;
        QVERIFY(!edit(diagnostics));
        QVERIFY(hasError(diagnostics));
        QVERIFY2(diagnostics.first().message.contains(reason),
                 qPrintable(diagnostics.first().message));
        QCOMPARE(m_session->currentStep(), step);
        verifyEqual(m_session->snapshot(), *m_bank);
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        auto opened = openRichBank(m_dir->path());
        QVERIFY(opened.has_value());
        m_bank = editablePart(opened->bank);
        DiagnosticList diagnostics;
        m_session = VoiceBankSession::create(std::move(*opened), diagnostics);
        QVERIFY(m_session);
    }

    void cleanup() {
        m_session.reset();
        m_bank.reset();
        m_dir.reset();
    }

    // Every public field changes in one step. A spelling that still reads as the number is
    // kept, so an unchanged number is saved as it was written.
    void an_entry_is_changed_as_a_whole() {
        const auto entry = directory({}).otoEntries().at(0);
        auto value = entry.toVoiceOtoEntry();
        value.alias = QStringLiteral("x");
        value.consonant = 1;
        DiagnosticList diagnostics;
        QVERIFY(VoiceBankEdits::setEntry(entry, value, diagnostics));
        QCOMPARE(m_session->currentStep(), 1);
        QCOMPARE(m_session->undoMessage(), QStringLiteral("Change Oto Entry"));

        const auto changed = entry.toVoiceOtoEntry();
        QCOMPARE(changed.alias, QStringLiteral("x"));
        QCOMPARE(changed.consonant, 1.0);
        QCOMPARE(changed.offset, 41.0);
        QCOMPARE(changed.spellings[0], std::optional<std::string>("41.0"));
    }

    // Entries are placed as saving sorts them, each after the entries of its audio file.
    void inserted_entries_take_their_place_by_file_name() {
        const auto root = directory({});
        DiagnosticList diagnostics;
        QVERIFY(
            VoiceBankEdits::insertEntries(root,
                                          {entryOf(QStringLiteral("b.wav"), QStringLiteral("b2")),
                                           entryOf(QStringLiteral("0.wav"), QStringLiteral("z")),
                                           entryOf(QStringLiteral("z.wav"), QString())},
                                          diagnostics));
        QCOMPARE(
            fileNames(root),
            QStringList({QStringLiteral("0.wav"), QStringLiteral("a.wav"), QStringLiteral("a.wav"),
                         QStringLiteral("b.wav"), QStringLiteral("b.wav"),
                         QStringLiteral("missing.wav"), QStringLiteral("z.wav")}));
        QCOMPARE(root.otoEntries().at(4).alias(), QStringLiteral("b2"));
        QCOMPARE(m_session->currentStep(), 1);
        m_session->undo();

        verifyRefused(
            [&](DiagnosticList &diagnostics) {
                return VoiceBankEdits::insertEntries(
                    root, {entryOf(QString(), QStringLiteral("e"))}, diagnostics);
            },
            QStringLiteral("has no file name"));
    }

    // An audio file without an entry is given one with an empty alias and zero numbers, which
    // takes it out of the samples without an entry.
    void an_audio_file_is_included() {
        const auto root = directory({});
        DiagnosticList diagnostics;
        QVERIFY(VoiceBankEdits::includeAudio(root, {QStringLiteral("c.wav")}, diagnostics));
        QCOMPARE(fileNames(root).at(3), QStringLiteral("c.wav"));
        QVERIFY(root.otoEntries().at(3).toVoiceOtoEntry() == entryOf(QStringLiteral("c.wav"), {}));

        int bare = 0;
        int entries = 0;
        const auto snapshot = m_session->snapshot();
        for (const auto &sample : snapshot.samples()) {
            if (sample.fileName == QStringLiteral("c.wav")) {
                (sample.hasEntry ? entries : bare) += 1;
            }
        }
        QCOMPARE(entries, 1);
        QCOMPARE(bare, 0);
        m_session->undo();

        verifyRefused(
            [&](DiagnosticList &diagnostics) {
                return VoiceBankEdits::includeAudio(root, {QStringLiteral("x.wav")}, diagnostics);
            },
            QStringLiteral("is not an audio file"));
        verifyRefused(
            [&](DiagnosticList &diagnostics) {
                return VoiceBankEdits::includeAudio(root, {QStringLiteral("a.wav")}, diagnostics);
            },
            QStringLiteral("already has an oto entry"));
        verifyRefused(
            [&](DiagnosticList &diagnostics) {
                return VoiceBankEdits::includeAudio(
                    root, {QStringLiteral("c.wav"), QStringLiteral("c.wav")}, diagnostics);
            },
            QStringLiteral("already has an oto entry"));
    }

    void entries_are_removed_by_index() {
        const auto root = directory({});
        DiagnosticList diagnostics;
        QVERIFY(VoiceBankEdits::removeEntries(root, {3, 0}, diagnostics));
        QCOMPARE(fileNames(root), QStringList({QStringLiteral("a.wav"), QStringLiteral("b.wav")}));
        QCOMPARE(m_session->currentStep(), 1);
        m_session->undo();

        for (const auto &indices : {
                 QList<int>{4},
                 QList<int>{-1},
                 QList<int>{1, 1}
        }) {
            verifyRefused(
                [&](DiagnosticList &diagnostics) {
                    return VoiceBankEdits::removeEntries(root, indices, diagnostics);
                },
                QStringLiteral("not an entry"));
        }
    }

    // A prefix is written into the existing prefix.map, or into a new one if there is none.
    void a_prefix_is_set_and_removed() {
        DiagnosticList diagnostics;
        QVERIFY(VoiceBankEdits::setPrefix(bank(), 61, VoicePrefix{QStringLiteral("x"), {}},
                                          diagnostics));
        QCOMPARE(bank().prefixMap().keys(), QList<int>({60, 61, 62}));
        QVERIFY(VoiceBankEdits::removePrefix(bank(), 60, diagnostics));
        QCOMPARE(bank().prefixMap().keys(), QList<int>({61, 62}));

        const auto step = m_session->currentStep();
        QVERIFY(!VoiceBankEdits::removePrefix(bank(), 60, diagnostics));
        QVERIFY(!VoiceBankEdits::setPrefix(bank(), 23, VoicePrefix(), diagnostics));
        QCOMPARE(m_session->currentStep(), step);

        {
            auto transaction = m_session->transaction(QStringLiteral("Remove"));
            bank().setPrefixMap(std::nullopt);
            QVERIFY(transaction.commit());
        }
        QVERIFY(!VoiceBankEdits::removePrefix(bank(), 61, diagnostics));
        QVERIFY(VoiceBankEdits::setPrefix(bank(), 70, VoicePrefix{QStringLiteral("p"), {}},
                                          diagnostics));
        QCOMPARE(bank().prefixMap().keys(), QList<int>({70}));
        QCOMPARE(bank().prefixMap().value(70).prefix, QStringLiteral("p"));
    }

    // Converting changes the encoding in which the files are saved. A directory whose oto.ini
    // declares UTF-8 is in UTF-8, and converting it writes the oto.ini without the declaration.
    void an_encoding_is_converted() {
        const auto sub = directory("sub");
        QCOMPARE(sub.charset(), QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        QVERIFY(VoiceBankEdits::convertCharset(sub, QStringLiteral("gbk"), diagnostics));
        QCOMPARE(sub.charset(), QStringLiteral("GBK"));
        QCOMPARE(m_session->undoMessage(), QStringLiteral("Convert Encoding"));
        m_session->undo();

        // The encoding in effect, in another spelling, is no change.
        const auto root = directory({});
        QVERIFY(VoiceBankEdits::convertCharset(root, QStringLiteral("gbk"), diagnostics));
        QVERIFY(VoiceBankEdits::convertCharset(sub, QStringLiteral("utf-8"), diagnostics));
        QCOMPARE(m_session->currentStep(), 0);

        verifyRefused(
            [&](DiagnosticList &diagnostics) {
                return VoiceBankEdits::convertCharset(root, QStringLiteral("no-such-encoding"),
                                                      diagnostics);
            },
            QStringLiteral("is not available"));
        verifyRefused(
            [&](DiagnosticList &diagnostics) {
                return VoiceBankEdits::convertCharset(root, QString(), diagnostics);
            },
            QStringLiteral("is not available"));
    }

    // A domain function called within a transaction joins it, so that composed functions form
    // one undo step.
    void domain_functions_compose_into_one_step() {
        DiagnosticList diagnostics;
        {
            auto transaction = m_session->transaction(QStringLiteral("Both"));
            QVERIFY(VoiceBankEdits::includeAudio(directory({}), {QStringLiteral("c.wav")},
                                                 diagnostics));
            QVERIFY(VoiceBankEdits::removePrefix(bank(), 62, diagnostics));
            QVERIFY(transaction.commit(diagnostics));
        }
        QCOMPARE(m_session->currentStep(), 1);
        QCOMPARE(m_session->undoMessage(), QStringLiteral("Both"));
        m_session->undo();
        verifyEqual(m_session->snapshot(), *m_bank);
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankEdits)

#include "test_VoiceBankEdits.moc"
