#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Support/TextCodec.h>
#include <hellokit/VoiceBank/VoiceBank.h>
#include <hellokit/VoiceBank/VoiceBankDiskState.h>

using namespace hello::kit;

// あ in Shift_JIS, and 葛平 in GBK. Neither is valid UTF-8, and neither decodes validly in the
// other encoding, so decoding a directory with the wrong one fails visibly rather than
// producing plausible text.
static const QByteArray kShiftJisA = QByteArray("\x82\xa0", 2);
static const QByteArray kGbkGePing = QByteArray("\xb8\xf0\xc6\xbd", 4);

namespace {

    /// Selects the encoding registered for each directory, and declines all others.
    ///
    /// The design allows two directories of one voice bank to use different encodings, which
    /// only a per-directory selector can express.
    class PerDirectorySelector : public VoiceBankCharsetSelector {
    public:
        void set(const std::filesystem::path &relative, const QString &charset) {
            m_charsets.insert(QString::fromStdU16String(relative.u16string()), charset);
        }

        int asked = 0;

        std::optional<QString> selectCharset(const VoiceBankDirectorySource &directory,
                                             DiagnosticList &) override {
            ++asked;
            const auto key = QString::fromStdU16String(directory.path.u16string());
            const auto it = m_charsets.find(key);
            if (it == m_charsets.end()) {
                return std::nullopt;
            }
            return *it;
        }

    private:
        QHash<QString, QString> m_charsets;
    };

}

class test_VoiceBank : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    std::filesystem::path root() const {
        return std::filesystem::path(m_dir->path().toStdU16String());
    }

    void write(const QString &relative, const QByteArray &bytes) {
        const QString path = m_dir->path() + QLatin1Char('/') + relative;
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    QByteArray read(const QString &relative) const {
        QFile file(m_dir->path() + QLatin1Char('/') + relative);
        if (!file.open(QIODevice::ReadOnly)) {
            return "<missing>";
        }
        return file.readAll();
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    void character_txt_is_decoded() {
        write(QStringLiteral("character.txt"), "name=" + kGbkGePing + "\nauthor=" + kGbkGePing +
                                                   "\nweb=http://example.com/\nVersion:1.0\n");
        write(QStringLiteral("a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("GBK"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        QCOMPARE(bank.character().name, QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3"));
        QCOMPARE(bank.character().author, QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3"));
        QCOMPARE(bank.character().web, QStringLiteral("http://example.com/"));

        // UTAU displays a line containing a colon as part of the character profile, so it is
        // content and must be decoded rather than dropped.
        QCOMPARE(bank.character().extraLines, QStringList{QStringLiteral("Version:1.0")});
    }

    void a_bank_without_character_txt_is_called_after_its_folder() {
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        QCOMPARE(bank.character().name, QString::fromStdU16String(root().filename().u16string()));
    }

    void it_finds_a_sample_by_its_alias() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",1,2,3,4,5\n");
        write(QStringLiteral("a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        const auto *sample = bank.find(60, QString::fromUtf8("\xe3\x81\x82"));
        QVERIFY(sample);
        QCOMPARE(sample->path, root() / "a.wav");
        QVERIFY(sample->hasEntry);
        QCOMPARE(sample->offset, 1.0);
        QCOMPARE(sample->consonant, 2.0);
        QCOMPARE(sample->cutoff, 3.0);
        QCOMPARE(sample->preUtterance, 4.0);
        QCOMPARE(sample->voiceOverlap, 5.0);
    }

    // The prefix map selects different samples for one lyric at different keys, which is why
    // find() takes a note number.
    void the_prefix_map_selects_the_sample_for_each_key() {
        write(QStringLiteral("oto.ini"), "a.wav=a,0,0,0,0,0\n"
                                         "a_high.wav=a\x81\x99,0,0,0,0,0\n");
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("a_high.wav"), "RIFF");

        // C5 is 72. In UTAU tone names, C1 is 24.
        write(QStringLiteral("prefix.map"), "C5\t\t\x81\x99\n");

        FixedCharsetSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        const auto *plain = bank.find(60, QStringLiteral("a"));
        QVERIFY(plain);
        QCOMPARE(plain->path, root() / "a.wav");

        const auto *high = bank.find(72, QStringLiteral("a"));
        QVERIFY(high);
        QCOMPARE(high->path, root() / "a_high.wav");
    }

    // A voice bank may be distributed without an oto.ini, and UTAU then sings the file whose
    // name matches the lyric. Such a sample has no timing, which differs from zero timing.
    void a_bank_without_an_oto_is_reached_by_file_name() {
        write(QStringLiteral("ka.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        const auto *sample = bank.find(60, QStringLiteral("ka"));
        QVERIFY(sample);
        QCOMPARE(sample->path, root() / "ka.wav");
        QVERIFY(!sample->hasEntry);
    }

    // UTAU also treats the file name of a sample as an alias, which is why voice bank authors
    // prefix a name with _ to exclude it. The entry supplies the timing, so the file name must
    // resolve to the entry rather than to a sample without timing.
    void a_sample_is_also_reached_by_its_file_name() {
        write(QStringLiteral("oto.ini"), "ka.wav=" + kShiftJisA + ",1,2,3,4,5\n");
        write(QStringLiteral("ka.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        const auto *sample = bank.find(60, QStringLiteral("ka"));
        QVERIFY(sample);
        QCOMPARE(sample->path, root() / "ka.wav");
        QVERIFY(sample->hasEntry);
        QCOMPARE(sample->offset, 1.0);
    }

    // An alias takes precedence over a file name, because the author specified it explicitly.
    void an_alias_is_preferred_to_a_file_name() {
        write(QStringLiteral("oto.ini"), "one.wav=ka,1,0,0,0,0\n"
                                         "ka.wav=other,2,0,0,0,0\n");
        write(QStringLiteral("one.wav"), "RIFF");
        write(QStringLiteral("ka.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        const auto *sample = bank.find(60, QStringLiteral("ka"));
        QVERIFY(sample);
        QCOMPARE(sample->path, root() / "one.wav");
    }

    void a_lyric_the_bank_cannot_sing_gives_nothing() {
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        QVERIFY(!bank.find(60, QStringLiteral("nothing here")));
    }

    // The reason hello-config.json is stored per directory rather than once per voice bank.
    void two_directories_may_be_in_different_encodings() {
        write(QStringLiteral("jp/oto.ini"), "a.wav=" + kShiftJisA + ",0,0,0,0,0\n");
        write(QStringLiteral("jp/a.wav"), "RIFF");
        write(QStringLiteral("cn/oto.ini"), "b.wav=" + kGbkGePing + ",0,0,0,0,0\n");
        write(QStringLiteral("cn/b.wav"), "RIFF");

        PerDirectorySelector selector;
        selector.set("jp", QStringLiteral("Shift_JIS"));
        selector.set("cn", QStringLiteral("GBK"));

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        QVERIFY(bank.find(60, QString::fromUtf8("\xe3\x81\x82")));
        QVERIFY(bank.find(60, QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3")));
    }

    // Taken from the configuration without querying the user, which is the purpose of
    // recording it.
    void a_recorded_encoding_is_not_asked_about_again() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",0,0,0,0,0\n");
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("hello-config.json"),
              R"({"$format":"hello-voicebank","charset":"Shift_JIS"})");

        PerDirectorySelector selector;
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        QCOMPARE(selector.asked, 0);
        QVERIFY(bank.find(60, QString::fromUtf8("\xe3\x81\x82")));
    }

    // The encoding rules forbid guessing, so a directory without a known encoding is left out
    // and reported, not decoded in an arbitrary encoding.
    void a_directory_nobody_names_an_encoding_for_is_left_out() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",0,0,0,0,0\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        QVERIFY(!bank.find(60, QString::fromUtf8("\xe3\x81\x82")));
        QVERIFY(!diagnostics.isEmpty());

        // The sample remains reachable by name, because a file name requires no encoding.
        const auto *sample = bank.find(60, QStringLiteral("a"));
        QVERIFY(sample);
        QVERIFY(!sample->hasEntry);
    }

    void an_encoding_that_does_not_exist_leaves_the_directory_out() {
        write(QStringLiteral("oto.ini"), "a.wav=a,0,0,0,0,0\n");
        write(QStringLiteral("a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("Klingon-1"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        QVERIFY(!diagnostics.isEmpty());

        const auto *sample = bank.find(60, QStringLiteral("a"));
        QVERIFY(sample);
        QVERIFY(!sample->hasEntry);
    }

    // The character.txt, prefix.map and readme.txt of a subdirectory belong to the voice bank the
    // subdirectory forms if selected by itself. They are neither read nor written, so they neither
    // rename the voice bank that was opened nor require an encoding.
    void the_files_of_a_subdirectory_other_than_oto_are_ignored() {
        write(QStringLiteral("character.txt"), "name=outer\n");
        write(QStringLiteral("a.wav"), "RIFF");
        const QByteArray character = "name=" + kGbkGePing + "\n";
        write(QStringLiteral("inner/oto.ini"), "b.wav=b,1,2,3,4,5\r\n");
        write(QStringLiteral("inner/character.txt"), character);
        write(QStringLiteral("inner/prefix.map"), "C4\t\t_B\n");
        write(QStringLiteral("inner/readme.txt"), kGbkGePing);
        write(QStringLiteral("inner/b.wav"), "RIFF");
        write(QStringLiteral("text/character.txt"), character);

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QCOMPARE(bank.character().name, QStringLiteral("outer"));
        const auto *text = directoryAt(bank, "text");
        QVERIFY(text);
        QVERIFY(text->charset.isEmpty());
        QVERIFY(!text->leftOut);
        QVERIFY(!text->character.has_value());

        const auto *inner = directoryAt(bank, "inner");
        QVERIFY(inner);
        QVERIFY(!inner->character.has_value());
        QVERIFY(!inner->prefixMap.has_value());
        QVERIFY(inner->readme.isEmpty());

        auto samples = bank.samples();
        for (auto &sample : samples) {
            if (sample.alias == QStringLiteral("b")) {
                sample.offset = 7;
            }
        }
        bank.setSamples(samples);
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("inner/character.txt")), character);
        QCOMPARE(read(QStringLiteral("inner/prefix.map")), QByteArray("C4\t\t_B\n"));
        QCOMPARE(read(QStringLiteral("inner/readme.txt")), kGbkGePing);
    }

    // The encoding in which a directory is saved. Without it a save would have to guess, and a
    // wrong guess corrupts every alias in the file.
    void each_directory_keeps_its_encoding() {
        write(QStringLiteral("jp/oto.ini"), "a.wav=" + kShiftJisA + ",0,0,0,0,0\n");
        write(QStringLiteral("jp/a.wav"), "RIFF");
        write(QStringLiteral("cn/oto.ini"), "b.wav=" + kGbkGePing + ",0,0,0,0,0\n");
        write(QStringLiteral("cn/b.wav"), "RIFF");

        PerDirectorySelector selector;
        selector.set("jp", QStringLiteral("Shift_JIS"));
        selector.set("cn", QStringLiteral("GBK"));

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        const auto *jp = directoryAt(bank, "jp");
        const auto *cn = directoryAt(bank, "cn");
        QVERIFY(jp && cn);
        QCOMPARE(jp->charset, TextCodec(QStringLiteral("Shift_JIS")).name());
        QCOMPARE(cn->charset, TextCodec(QStringLiteral("GBK")).name());
        QVERIFY(!jp->leftOut && !cn->leftOut);
    }

    // A directory that could not be read must be marked as such, because saving it would
    // overwrite unread files with empty content.
    void a_directory_left_out_is_marked() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",0,0,0,0,0\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        const auto *top = directoryAt(bank, "");
        QVERIFY(top);
        QVERIFY(top->leftOut);
        QVERIFY(top->charset.isEmpty());
    }

    // All data from which an entry is saved: its directory, the file name as written in the
    // oto.ini, and the original text of its numbers.
    void a_sample_records_its_origin() {
        write(QStringLiteral("sub/oto.ini"), "ka.wav=" + kShiftJisA + ",41.0,2,3,4,5\n");
        write(QStringLiteral("sub/ka.wav"), "RIFF");
        write(QStringLiteral("sub/ki.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        const auto *entry = bank.find(60, QString::fromUtf8("\xe3\x81\x82"));
        QVERIFY(entry);
        QCOMPARE(bank.directories().at(entry->directory).path, std::filesystem::path("sub"));
        QCOMPARE(entry->fileName, QStringLiteral("ka.wav"));
        QCOMPARE(entry->spellings.at(0), std::optional<std::string>("41.0"));

        const auto *bare = bank.find(60, QStringLiteral("ki"));
        QVERIFY(bare);
        QVERIFY(!bare->hasEntry);
        QCOMPARE(bank.directories().at(bare->directory).path, std::filesystem::path("sub"));
        QCOMPARE(bare->fileName, QStringLiteral("ki.wav"));
    }

    // UTAU treats an empty alias as the file name, and the sample is found that way. Filling it
    // in would write the file name where the author left the field empty.
    void an_empty_alias_stays_empty() {
        write(QStringLiteral("oto.ini"), "ka.wav=,1,2,3,4,5\n");
        write(QStringLiteral("ka.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        const auto *sample = bank.find(60, QStringLiteral("ka"));
        QVERIFY(sample);
        QVERIFY(sample->hasEntry);
        QVERIFY(sample->alias.isEmpty());
    }

    // A name that no code page can represent. On Windows the narrow form of a path uses the
    // system code page, and requesting it throws for such a name, which previously caused the
    // entire voice bank to fail to open.
    void a_file_name_the_code_page_cannot_spell_is_read() {
        write(QString::fromUtf8("\xf0\x9f\x98\x80.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        QVERIFY(bank.find(60, QString::fromUtf8("\xf0\x9f\x98\x80")));
    }

    // A file name in an oto.ini is in the encoding of the voice bank, not of the system.
    // Interpreted in the system encoding, it refers to a different file, which fails on every
    // Windows system whose code page differs from the voice bank encoding. Here the encoding is
    // UTF-8, which is not the default code page of any Windows system.
    void an_entry_names_its_file_in_the_bank_encoding() {
        write(QStringLiteral("oto.ini"), "\xe3\x81\x82.wav=a,1,2,3,4,5\n");
        write(QString::fromUtf8("\xe3\x81\x82.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;

        const auto *sample = bank.find(60, QStringLiteral("a"));
        QVERIFY(sample);
        QCOMPARE(sample->path, root() / std::filesystem::path(u"\u3042.wav"));

        // One file yields one entry. The file belongs to the entry and is not added again as a
        // bare sample.
        QCOMPARE(bank.samples().size(), 1);
    }

private:
    static const VoiceBankDirectory *directoryAt(const VoiceBank &bank, const char *relative) {
        for (const auto &directory : bank.directories()) {
            if (directory.path == std::filesystem::path(relative)) {
                return &directory;
            }
        }
        return nullptr;
    }
};

QTEST_APPLESS_MAIN(test_VoiceBank)

#include "test_VoiceBank.moc"
