#include <memory>

#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/Edit/VoiceBankSession.h>

#include "VoiceBankSamples.h"

using namespace hello::kit;

namespace fs = std::filesystem;

class test_VoiceBankRefs : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::optional<VoiceBank> m_bank;
    std::unique_ptr<VoiceBankSession> m_session;

    // The number of samples of fileName in bank with or without an entry.
    static int countOf(const VoiceBank &bank, const QString &fileName, bool hasEntry) {
        int n = 0;
        for (const auto &sample : bank.samples()) {
            n += sample.fileName == fileName && sample.hasEntry == hasEntry;
        }
        return n;
    }

    VoiceBankRef root() const {
        return VoiceBankRef(m_session.get());
    }

    OtoEntryListRef rootEntries() const {
        return root().directories().at(0).otoEntries();
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

    void every_getter_reads_its_field() {
        const auto &bank = *m_bank;
        const auto &rootDirectory = bank.directories().at(0);

        QCOMPARE(root().readme(), rootDirectory.readme);
        const auto character = root().character();
        QVERIFY(character.isValid());
        QCOMPARE(character.name(), rootDirectory.character->name);
        QCOMPARE(character.image(), rootDirectory.character->image);
        QCOMPARE(character.sample(), rootDirectory.character->sample);
        QCOMPARE(character.author(), rootDirectory.character->author);
        QCOMPARE(character.web(), rootDirectory.character->web);
        QCOMPARE(character.extraLines(), rootDirectory.character->extraLines);
        QVERIFY(character.toVoiceCharacter() == *rootDirectory.character);

        const auto prefixMap = root().prefixMap();
        QCOMPARE(prefixMap.keys(), QList<int>({60, 62}));
        QVERIFY(prefixMap.contains(60));
        QVERIFY(!prefixMap.contains(61));
        QVERIFY(prefixMap.value(60) == rootDirectory.prefixMap->value(60));
        QCOMPARE(prefixMap.value(60).prefix, QStringLiteral("p"));

        const auto directories = root().directories();
        QCOMPARE(directories.size(), int(bank.directories().size()));
        for (int i = 0; i < directories.size(); ++i) {
            const auto directory = directories.at(i);
            const auto &expected = bank.directories().at(i);
            QCOMPARE(directory.path(), expected.path);
            QCOMPARE(directory.path().native(), expected.path.native());
            QCOMPARE(directory.charset(), expected.charset);
            QCOMPARE(directory.otoCharset(), expected.otoCharset);
        }

        const auto entries = rootEntries();
        QCOMPARE(entries.size(), 4);
        for (int i = 0; i < entries.size(); ++i) {
            const auto entry = entries.at(i);
            const auto &sample = bank.samples().at(i);
            QVERIFY(sample.hasEntry);
            QCOMPARE(entry.fileName(), sample.fileName);
            QCOMPARE(entry.alias(), sample.alias);
            QCOMPARE(entry.offset(), sample.offset);
            QCOMPARE(entry.consonant(), sample.consonant);
            QCOMPARE(entry.cutoff(), sample.cutoff);
            QCOMPARE(entry.preUtterance(), sample.preUtterance);
            QCOMPARE(entry.voiceOverlap(), sample.voiceOverlap);
            QVERIFY(entry.toVoiceOtoEntry().spellings == sample.spellings);
        }
    }

    // Writing the value a field already holds is no change, so no change is reported and no undo
    // step is created. This relies on the equality of every value type, including the prefix.
    void writing_the_current_values_changes_nothing() {
        QSignalSpy changed(m_session.get(), &edit::EditSession::changed);
        {
            auto transaction = m_session->transaction(QStringLiteral("Nothing"));
            root().setReadme(root().readme());
            const auto character = root().character();
            character.setName(character.name());
            character.setImage(character.image());
            character.setSample(character.sample());
            character.setAuthor(character.author());
            character.setWeb(character.web());
            character.setExtraLines(character.extraLines());
            root().setCharacter(character.toVoiceCharacter());

            const auto prefixMap = root().prefixMap();
            for (const auto key : prefixMap.keys()) {
                prefixMap.setValue(key, prefixMap.value(key));
            }

            const auto entry = rootEntries().at(0);
            entry.setFileName(entry.fileName());
            entry.setAlias(entry.alias());
            entry.setOffset(entry.offset());
            entry.setConsonant(entry.consonant());
            entry.setCutoff(entry.cutoff());
            entry.setPreUtterance(entry.preUtterance());
            entry.setVoiceOverlap(entry.voiceOverlap());
            QVERIFY(transaction.commit());
        }
        QCOMPARE(changed.count(), 0);
        QCOMPARE(m_session->currentStep(), 0);
    }

    // Every setter reaches the voice bank that is saved, and undo restores what was read.
    void every_setter_writes_its_field() {
        {
            auto transaction = m_session->transaction(QStringLiteral("Everything"));
            root().setReadme(QStringLiteral("readme"));
            const auto character = root().character();
            character.setName(QStringLiteral("name"));
            character.setImage(QStringLiteral("image.bmp"));
            character.setSample(QStringLiteral("sample.wav"));
            character.setAuthor(QStringLiteral("author"));
            character.setWeb(QStringLiteral("web"));
            character.setExtraLines({QStringLiteral("line")});
            root().prefixMap().setValue(61, VoicePrefix{QStringLiteral("x"), QStringLiteral("y")});
            root().prefixMap().remove(62);

            const auto entry = rootEntries().at(3);
            entry.setFileName(QStringLiteral("c.wav"));
            entry.setAlias(QStringLiteral("c"));
            entry.setOffset(11);
            entry.setConsonant(12);
            entry.setCutoff(-13);
            entry.setPreUtterance(14);
            entry.setVoiceOverlap(15);
            QVERIFY(transaction.commit());
        }

        const auto bank = m_session->snapshot();
        const auto &rootDirectory = bank.directories().at(0);
        QCOMPARE(rootDirectory.readme, QStringLiteral("readme"));
        QVERIFY(rootDirectory.character == (VoiceCharacter{QStringLiteral("name"),
                                                           QStringLiteral("image.bmp"),
                                                           QStringLiteral("sample.wav"),
                                                           QStringLiteral("author"),
                                                           QStringLiteral("web"),
                                                           {QStringLiteral("line")}}));
        QCOMPARE(rootDirectory.prefixMap->keys(), QList<int>({60, 61}));
        QCOMPARE(rootDirectory.prefixMap->value(61).suffix, QStringLiteral("y"));

        const auto *sample = bank.find(70, QStringLiteral("c"));
        QVERIFY(sample);
        QVERIFY(sample->hasEntry);
        QCOMPARE(sample->fileName, QStringLiteral("c.wav"));
        QCOMPARE(sample->offset, 11.0);
        QCOMPARE(sample->consonant, 12.0);
        QCOMPARE(sample->cutoff, -13.0);
        QCOMPARE(sample->preUtterance, 14.0);
        QCOMPARE(sample->voiceOverlap, 15.0);

        // The entry named missing.wav now names c.wav, so c.wav has no sample without an entry.
        QCOMPARE(countOf(bank, QStringLiteral("c.wav"), false), 0);
        QCOMPARE(countOf(bank, QStringLiteral("missing.wav"), true), 0);

        m_session->undo();
        verifyEqual(m_session->snapshot(), *m_bank);
    }

    // The character and the prefix map are files that may be absent, which is not the same as
    // empty.
    void the_files_of_the_root_are_added_and_removed_as_a_whole() {
        {
            auto transaction = m_session->transaction(QStringLiteral("Remove"));
            root().setCharacter(std::nullopt);
            root().setPrefixMap(std::nullopt);
            QVERIFY(transaction.commit());
        }
        QVERIFY(!root().character().isValid());
        QVERIFY(!root().prefixMap().isValid());
        auto bank = m_session->snapshot();
        QVERIFY(!bank.directories().at(0).character);
        QVERIFY(!bank.directories().at(0).prefixMap);

        {
            auto transaction = m_session->transaction(QStringLiteral("Add"));
            root().setCharacter(VoiceCharacter());
            root().setPrefixMap(QMap<int, VoicePrefix>());
            QVERIFY(transaction.commit());
        }
        QVERIFY(root().character().isValid());
        QVERIFY(root().prefixMap().isValid());
        QVERIFY(root().prefixMap().keys().isEmpty());
        bank = m_session->snapshot();
        QVERIFY(bank.directories().at(0).character == VoiceCharacter());
        QCOMPARE(bank.directories().at(0).prefixMap, std::optional(QMap<int, VoicePrefix>()));
    }

    // An inserted entry has no spellings, and takes the audio file it names out of the samples
    // without an entry. Removing it returns the file to them.
    void entries_are_inserted_removed_and_moved() {
        const auto entries = rootEntries();
        VoiceOtoEntry added;
        added.fileName = QStringLiteral("c.wav");
        added.alias = QStringLiteral("c");
        {
            auto transaction = m_session->transaction(QStringLiteral("Insert"));
            entries.insert(1, {added});
            QVERIFY(transaction.commit());
        }
        QCOMPARE(entries.size(), 5);
        QVERIFY(entries.at(1).toVoiceOtoEntry() == added);
        for (const auto &spelling : entries.at(1).toVoiceOtoEntry().spellings) {
            QVERIFY(!spelling.has_value());
        }
        auto bank = m_session->snapshot();
        QCOMPARE(countOf(bank, QStringLiteral("c.wav"), true), 1);
        QCOMPARE(countOf(bank, QStringLiteral("c.wav"), false), 0);

        {
            auto transaction = m_session->transaction(QStringLiteral("Move"));
            entries.move(1, 1, 4);
            QVERIFY(transaction.commit());
        }
        QCOMPARE(entries.at(4).fileName(), QStringLiteral("c.wav"));

        {
            auto transaction = m_session->transaction(QStringLiteral("Remove"));
            entries.remove(4, 1);
            QVERIFY(transaction.commit());
        }
        QCOMPARE(entries.size(), 4);
        bank = m_session->snapshot();
        QCOMPARE(countOf(bank, QStringLiteral("c.wav"), true), 0);
        QCOMPARE(countOf(bank, QStringLiteral("c.wav"), false), 1);
    }

    // The path of a directory reads with the separators of the system, as reading produces it.
    void a_directory_path_reads_as_read_from_disk() {
        const auto inner = m_bank->indexOf(fs::path("sub") / "inner");
        QVERIFY(inner > 0);
        const auto directory = root().directories().at(inner);
        QCOMPARE(directory.path().native(), (fs::path("sub") / "inner").native());
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankRefs)

#include "test_VoiceBankRefs.moc"
