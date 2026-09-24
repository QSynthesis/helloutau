#include <map>
#include <memory>
#include <sstream>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <substate/Codec.h>

#include "VoiceBankTree_p.h"

using namespace hello::kit;

namespace fs = std::filesystem;

// 葛平 in GBK.
static const QByteArray kGbkGePing = QByteArray("\xb8\xf0\xc6\xbd", 4);

namespace {

    /// Selects an encoding per directory, and declines a directory that has none.
    class DirectorySelector : public VoiceBankCharsetSelector {
    public:
        explicit DirectorySelector(std::map<fs::path, QString> charsets)
            : m_charsets(std::move(charsets)) {
        }

        std::optional<QString> selectCharset(const VoiceBankDirectorySource &directory,
                                             DiagnosticList &) override {
            const auto it = m_charsets.find(directory.path);
            return it == m_charsets.end() ? std::nullopt : std::optional<QString>(it->second);
        }

    private:
        std::map<fs::path, QString> m_charsets;
    };

}

class test_VoiceBankTree : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    fs::path root() const {
        return fs::path(m_dir->path().toStdU16String());
    }

    void write(const QString &relative, const QByteArray &bytes) {
        const QString path = m_dir->path() + QLatin1Char('/') + relative;
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    // A voice bank with every kind of content that the tree holds: the files of the root, an
    // oto.ini that declares its encoding, spellings of numbers including empty ones, an entry
    // whose audio file is missing, audio files without an entry, and directories that were left
    // out or did not decode.
    std::optional<VoiceBankDiskState::Opened> openRichBank() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing +
                                             ",41.0,87.688,97.316,8.938,4.457\r\n"
                                             "a.wav=- " +
                                             kGbkGePing +
                                             ",41,87.6880,-143.414,8.938,04.457\r\n"
                                             "b.wav=,,,,,\r\n"
                                             "missing.wav=m,1,2,3,4,5\r\n");
        write(QStringLiteral("character.txt"), "name=" + kGbkGePing +
                                                   "\r\nimage=icon.bmp\r\nauthor=a\r\n"
                                                   "web=w\r\nsample=s.wav\r\nVersion:1.0\r\n");
        write(QStringLiteral("prefix.map"), "C4\tp\ts\r\nD4\t\t" + kGbkGePing + "\r\n");
        write(QStringLiteral("readme.txt"), kGbkGePing);
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");
        write(QStringLiteral("c.wav"), "RIFF");
        write(QStringLiteral("sub/oto.ini"), "#Charset:UTF-8\r\nx.wav=\xe8\x91\x9b,1,2,3,4,5\r\n");
        write(QStringLiteral("sub/x.wav"), "RIFF");
        write(QStringLiteral("sub/deep/oto.ini"), "y.wav=\xff,1,2,3,4,5\r\n");
        write(QStringLiteral("sub/deep/y.wav"), "RIFF");
        write(QStringLiteral("left/oto.ini"), "z.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("left/z.wav"), "RIFF");

        DirectorySelector selector({
            {fs::path(),               QStringLiteral("GBK")  },
            {fs::path("sub") / "deep", QStringLiteral("UTF-8")},
        });
        DiagnosticList diagnostics;
        return VoiceBankDiskState::open(root(), &selector, diagnostics);
    }

    static void verifyCharacter(const std::optional<VoiceCharacter> &actual,
                                const std::optional<VoiceCharacter> &expected) {
        QCOMPARE(actual.has_value(), expected.has_value());
        if (!expected) {
            return;
        }
        QCOMPARE(actual->name, expected->name);
        QCOMPARE(actual->image, expected->image);
        QCOMPARE(actual->sample, expected->sample);
        QCOMPARE(actual->author, expected->author);
        QCOMPARE(actual->web, expected->web);
        QCOMPARE(actual->extraLines, expected->extraLines);
    }

    // Compares every field of two voice banks, including the order of the samples, which
    // determines the precedence between duplicate aliases.
    static void verifyEqual(const VoiceBank &actual, const VoiceBank &expected) {
        QCOMPARE(actual.root(), expected.root());
        QCOMPARE(actual.directories().size(), expected.directories().size());
        for (int i = 0; i < expected.directories().size(); ++i) {
            const auto &a = actual.directories().at(i);
            const auto &e = expected.directories().at(i);
            QCOMPARE(a.path, e.path);
            QCOMPARE(a.path.native(), e.path.native());
            QCOMPARE(a.charset, e.charset);
            QCOMPARE(a.otoCharset, e.otoCharset);
            QCOMPARE(a.leftOut, e.leftOut);
            QCOMPARE(a.lossy, e.lossy);
            verifyCharacter(a.character, e.character);
            QCOMPARE(a.prefixMap, e.prefixMap);
            QCOMPARE(a.readme, e.readme);
        }
        QCOMPARE(actual.samples().size(), expected.samples().size());
        for (int i = 0; i < expected.samples().size(); ++i) {
            const auto &a = actual.samples().at(i);
            const auto &e = expected.samples().at(i);
            QCOMPARE(a.path, e.path);
            QCOMPARE(a.path.native(), e.path.native());
            QCOMPARE(a.directory, e.directory);
            QCOMPARE(a.fileName, e.fileName);
            QCOMPARE(a.alias, e.alias);
            QCOMPARE(a.offset, e.offset);
            QCOMPARE(a.consonant, e.consonant);
            QCOMPARE(a.cutoff, e.cutoff);
            QCOMPARE(a.preUtterance, e.preUtterance);
            QCOMPARE(a.voiceOverlap, e.voiceOverlap);
            QCOMPARE(a.hasEntry, e.hasEntry);
            QVERIFY(a.spellings == e.spellings);
        }
        QCOMPARE(actual.character().name, expected.character().name);
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

    // The voice bank that a session assembles from its tree for each save equals the one read.
    void a_voice_bank_survives_its_tree() {
        auto opened = openRichBank();
        QVERIFY(opened.has_value());
        const auto &bank = opened->bank;
        QCOMPARE(bank.directories().size(), 4);
        QVERIFY(bank.directories().at(0).character);
        QVERIFY(bank.directories().at(0).prefixMap);
        const auto sub = bank.indexOf("sub");
        const auto left = bank.indexOf("left");
        const auto deep = bank.indexOf(fs::path("sub") / "deep");
        QVERIFY(sub > 0 && left > 0 && deep > 0);
        QCOMPARE(bank.directories().at(sub).otoCharset, QStringLiteral("UTF-8"));
        QVERIFY(bank.directories().at(left).leftOut);
        QVERIFY(bank.directories().at(deep).lossy);

        const auto tree = treeOf(bank);
        verifyEqual(voiceBankOf(tree.get(), opened->disk), bank);
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
        const auto deep = opened->bank.indexOf(fs::path("sub") / "deep");
        QVERIFY(deep > 0);
        const auto &nested = static_cast<const VoiceDirectoryNode &>(*directories->at(deep));
        QCOMPARE(nested.variant(VoiceDirectorySlots::Path.index).toString(),
                 QStringLiteral("sub/deep"));

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

        const auto back = voiceBankOf(treeOf(bank).get(), opened->disk);
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
        const auto decodedBank = voiceBankOf(back.get(), opened->disk);
        verifyEqual(decodedBank, voiceBankOf(treeOf(bank).get(), opened->disk));

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
