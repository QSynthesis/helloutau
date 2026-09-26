#include <memory>
#include <sstream>

#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <substate/Codec.h>

#include "VoiceBankSamples.h"
#include "VoiceBankTree_p.h"

using namespace hello::kit;

namespace fs = std::filesystem;

class test_VoiceBankTree : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    std::optional<VoiceBankFileSystemState::Opened> openRichBank() const {
        return hello::kit::openRichBank(m_dir->path());
    }


    static std::string encoded(const ss::Node *node) {
        std::stringstream buffer;
        ss::OBinaryStream out(buffer);
        ss::Encoder encoder(out);
        encoder.writeNode(node);
        return encoder.fail() ? std::string() : buffer.str();
    }

    static std::unique_ptr<ss::Node> decoded(const ss::QCodec &codec, const std::string &bytes) {
        std::stringstream buffer(bytes);
        ss::IBinaryStream in(buffer);
        ss::Decoder decoder(codec, in, nullptr);
        auto node = decoder.readNode();
        return decoder.fail() ? nullptr : std::move(node);
    }

private Q_SLOTS:
    // A process that reads a history without having written these values finds their types by
    // name only after registration. This case runs first, before any encoding in this process
    // registers the types as a side effect.
    void the_value_types_are_found_by_name_after_registration() {
        ss::QCodec codec;
        registerVoiceBankTypes(codec);
        QVERIFY(QMetaType::fromName("hello::kit::VoicePrefix").isValid());
    }

    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    // The voice bank that a session assembles from its tree for each save equals the one read,
    // without the directories that cannot be saved.
    void a_voice_bank_survives_its_tree() {
        auto opened = openRichBank();
        QVERIFY(opened.has_value());
        const auto &bank = opened->bank;
        QCOMPARE(bank.directories().size(), 5);
        QVERIFY(bank.directories().at(0).character);
        QVERIFY(bank.directories().at(0).prefixMap);
        const auto sub = bank.indexOf("sub");
        const auto left = bank.indexOf("left");
        const auto deep = bank.indexOf(fs::path("sub") / "deep");
        QVERIFY(sub > 0 && left > 0 && deep > 0);
        QCOMPARE(bank.directories().at(sub).charset, QStringLiteral("UTF-8"));
        QVERIFY(bank.directories().at(left).leftOut);
        QVERIFY(bank.directories().at(deep).leftOut);

        const auto tree = treeOf(bank);
        const auto back = voiceBankOf(tree.get(), opened->files);
        QCOMPARE(back.directories().size(), 3);
        QCOMPARE(back.indexOf("left"), -1);
        QCOMPARE(back.indexOf(fs::path("sub") / "deep"), -1);
        verifyEqual(back, editablePart(bank));
    }

    // The files of the root belong to the voice bank as a whole, and the directories hold the
    // entries only.
    void the_tree_holds_the_entries_and_the_files_of_the_root() {
        auto opened = openRichBank();
        QVERIFY(opened.has_value());
        const auto tree = treeOf(opened->bank);
        const auto &root = static_cast<const VoiceBankNode &>(*tree);

        QCOMPARE(root.variant(VoiceBankSlots::Readme.index).toString(),
                 QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3"));
        const auto map =
            static_cast<const ss::MappingNode *>(root.child(VoiceBankSlots::PrefixMap.index));
        QVERIFY(map);
        QCOMPARE(map->keys(), QStringList({QStringLiteral("60"), QStringLiteral("62")}));

        const auto directories =
            static_cast<const ss::VectorNode *>(root.child(VoiceBankSlots::Directories.index));
        const auto &first = static_cast<const VoiceDirectoryNode &>(*directories->at(0));
        QCOMPARE(first.variant(VoiceDirectorySlots::Path.index).toString(), QString());

        // Written with slashes on every system, as a command addresses it.
        const auto inner = editablePart(opened->bank).indexOf(fs::path("sub") / "inner");
        QVERIFY(inner > 0);
        const auto &nested = static_cast<const VoiceDirectoryNode &>(*directories->at(inner));
        QCOMPARE(nested.variant(VoiceDirectorySlots::Path.index).toString(),
                 QStringLiteral("sub/inner"));

        // Four entries in the root, and none for the audio file without one.
        const auto entries =
            static_cast<const ss::VectorNode *>(first.child(VoiceDirectorySlots::OtoEntries.index));
        QCOMPARE(entries->size(), 4);
        const auto empty = edit::fromTree<VoiceOtoEntry>(entries->at(2));
        QCOMPARE(empty.fileName, QStringLiteral("b.wav"));
        QVERIFY(empty.spellings[0].has_value());
        QCOMPARE(*empty.spellings[0], std::string());
    }

    // The samples without an entry are the audio files that no entry names, so inserting an
    // entry for a file removes its bare sample, and removing the last entry of a file restores
    // it.
    void the_samples_without_an_entry_follow_the_entries() {
        auto opened = openRichBank();
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        auto samples = bank.samples();
        for (auto &sample : samples) {
            if (sample.fileName == QStringLiteral("c.wav")) {
                sample.hasEntry = true;
                sample.alias = QStringLiteral("c");
            }
        }
        samples.erase(std::remove_if(samples.begin(), samples.end(),
                                     [](const VoiceSample &sample) {
                                         return sample.fileName == QStringLiteral("b.wav");
                                     }),
                      samples.end());
        bank.setSamples(samples);

        const auto back = voiceBankOf(treeOf(bank).get(), opened->files);
        const auto count = [&back](const QString &fileName, bool hasEntry) {
            int n = 0;
            for (const auto &sample : back.samples()) {
                n += sample.fileName == fileName && sample.hasEntry == hasEntry;
            }
            return n;
        };
        QCOMPARE(count(QStringLiteral("c.wav"), true), 1);
        QCOMPARE(count(QStringLiteral("c.wav"), false), 0);
        QCOMPARE(count(QStringLiteral("b.wav"), true), 0);
        QCOMPARE(count(QStringLiteral("b.wav"), false), 1);
        QCOMPARE(count(QStringLiteral("missing.wav"), true), 1);
    }

    // The edit history stores inserted subtrees in this encoding, including the prefixes, which
    // are stored as values of a type outside the fixed encoding of QVariant, and the spellings,
    // whose absent numbers are invalid values.
    void a_voice_bank_tree_survives_encoding() {
        auto opened = openRichBank();
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        // A new entry, whose spellings are absent, beside those read.
        auto samples = bank.samples();
        VoiceSample added;
        added.fileName = QStringLiteral("c.wav");
        added.hasEntry = true;
        samples.push_back(added);
        bank.setSamples(samples);

        ss::QCodec codec;
        registerVoiceBankTypes(codec);
        const auto bytes = encoded(treeOf(bank).get());
        QVERIFY(!bytes.empty());
        const auto back = decoded(codec, bytes);
        QVERIFY(back);
        const auto decodedBank = voiceBankOf(back.get(), opened->files);
        verifyEqual(decodedBank, voiceBankOf(treeOf(bank).get(), opened->files));

        // An absent spelling differs from an empty one, which reads as zero.
        int found = 0;
        for (const auto &sample : decodedBank.samples()) {
            if (sample.fileName == QStringLiteral("c.wav") && sample.hasEntry) {
                ++found;
                for (const auto &spelling : sample.spellings) {
                    QVERIFY(!spelling.has_value());
                }
            }
        }
        QCOMPARE(found, 1);
    }

    // The default types of the struct nodes do not determine their size, so decoding requires
    // the registered node types.
    void decoding_requires_the_registered_node_types() {
        auto opened = openRichBank();
        QVERIFY(opened.has_value());
        const auto bytes = encoded(treeOf(opened->bank).get());
        QVERIFY(!bytes.empty());
        QVERIFY(!decoded(ss::QCodec(), bytes));
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankTree)

#include "test_VoiceBankTree.moc"
