#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/VoiceBank.h>

using namespace hello::kit;

// あ in Shift_JIS, and 葛平 in GBK. Neither is valid UTF-8, and neither reads as the other, so a
// directory decoded with the wrong one says so rather than looking plausible.
static const QByteArray kShiftJisA = QByteArray("\x82\xa0", 2);
static const QByteArray kGbkGePing = QByteArray("\xb8\xf0\xc6\xbd", 4);

namespace {

    /// Answers each directory with whatever was registered for it, and refuses the rest.
    ///
    /// The point of the design is that two directories of one bank may be in different
    /// encodings, and only a selector asked per directory can say so.
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

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    void it_decodes_what_character_txt_says() {
        write(QStringLiteral("character.txt"),
              "name=" + kGbkGePing + "\nauthor=" + kGbkGePing +
                  "\nweb=http://example.com/\nVersion:1.0\n");
        write(QStringLiteral("a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("GBK"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), &selector, diagnostics);
        QVERIFY(bank.has_value());

        QCOMPARE(bank->character().name, QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3"));
        QCOMPARE(bank->character().author, QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3"));
        QCOMPARE(bank->character().web, QStringLiteral("http://example.com/"));

        // UTAU shows a line holding a colon as part of the character's profile, so it is content
        // and has to arrive decoded rather than be dropped.
        QCOMPARE(bank->character().extraLines, QStringList{QStringLiteral("Version:1.0")});
    }

    void a_bank_without_character_txt_is_called_after_its_folder() {
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), nullptr, diagnostics);
        QVERIFY(bank.has_value());
        QCOMPARE(bank->character().name,
                 QString::fromStdU16String(root().filename().u16string()));
    }

    void it_finds_a_sample_by_its_alias() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",1,2,3,4,5\n");
        write(QStringLiteral("a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), &selector, diagnostics);
        QVERIFY(bank.has_value());

        const auto *sample = bank->find(60, QString::fromUtf8("\xe3\x81\x82"));
        QVERIFY(sample);
        QCOMPARE(sample->path, root() / "a.wav");
        QVERIFY(sample->hasEntry);
        QCOMPARE(sample->offset, 1.0);
        QCOMPARE(sample->consonant, 2.0);
        QCOMPARE(sample->cutoff, 3.0);
        QCOMPARE(sample->preUtterance, 4.0);
        QCOMPARE(sample->voiceOverlap, 5.0);
    }

    // What makes one lyric sing differently at different keys, and the reason find() takes a
    // note number at all.
    void the_prefix_map_decides_which_sample_a_key_uses() {
        write(QStringLiteral("oto.ini"), "a.wav=a,0,0,0,0,0\n"
                                         "a_high.wav=a\x81\x99,0,0,0,0,0\n");
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("a_high.wav"), "RIFF");

        // C5 is 72. UTAU's tone names count C1 as 24.
        write(QStringLiteral("prefix.map"), "C5\t\t\x81\x99\n");

        FixedCharsetSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), &selector, diagnostics);
        QVERIFY(bank.has_value());

        const auto *plain = bank->find(60, QStringLiteral("a"));
        QVERIFY(plain);
        QCOMPARE(plain->path, root() / "a.wav");

        const auto *high = bank->find(72, QStringLiteral("a"));
        QVERIFY(high);
        QCOMPARE(high->path, root() / "a_high.wav");
    }

    // A bank may ship without an oto.ini at all, and UTAU then sings the file whose name is the
    // lyric. Such a sample has no timing, which is not timing that is zero by choice.
    void a_bank_without_an_oto_is_reached_by_file_name() {
        write(QStringLiteral("ka.wav"), "RIFF");

        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), nullptr, diagnostics);
        QVERIFY(bank.has_value());

        const auto *sample = bank->find(60, QStringLiteral("ka"));
        QVERIFY(sample);
        QCOMPARE(sample->path, root() / "ka.wav");
        QVERIFY(!sample->hasEntry);
    }

    // UTAU reads a sample's file name as an alias as well, which is why bank authors put a _ in
    // front of a name they do not want sung by it. The entry is what carries the timing, so the
    // file name has to lead to it rather than to a sample with none.
    void a_sample_is_also_reached_by_its_file_name() {
        write(QStringLiteral("oto.ini"), "ka.wav=" + kShiftJisA + ",1,2,3,4,5\n");
        write(QStringLiteral("ka.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), &selector, diagnostics);
        QVERIFY(bank.has_value());

        const auto *sample = bank->find(60, QStringLiteral("ka"));
        QVERIFY(sample);
        QCOMPARE(sample->path, root() / "ka.wav");
        QVERIFY(sample->hasEntry);
        QCOMPARE(sample->offset, 1.0);
    }

    // An alias wins over a file name, since it is what the bank's author wrote down.
    void an_alias_is_preferred_to_a_file_name() {
        write(QStringLiteral("oto.ini"), "one.wav=ka,1,0,0,0,0\n"
                                         "ka.wav=other,2,0,0,0,0\n");
        write(QStringLiteral("one.wav"), "RIFF");
        write(QStringLiteral("ka.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), &selector, diagnostics);
        QVERIFY(bank.has_value());

        const auto *sample = bank->find(60, QStringLiteral("ka"));
        QVERIFY(sample);
        QCOMPARE(sample->path, root() / "one.wav");
    }

    void a_lyric_the_bank_cannot_sing_gives_nothing() {
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), nullptr, diagnostics);
        QVERIFY(bank.has_value());
        QVERIFY(!bank->find(60, QStringLiteral("nothing here")));
    }

    // The reason hello-config.json sits in each directory rather than once per bank.
    void two_directories_may_be_in_different_encodings() {
        write(QStringLiteral("jp/oto.ini"), "a.wav=" + kShiftJisA + ",0,0,0,0,0\n");
        write(QStringLiteral("jp/a.wav"), "RIFF");
        write(QStringLiteral("cn/oto.ini"), "b.wav=" + kGbkGePing + ",0,0,0,0,0\n");
        write(QStringLiteral("cn/b.wav"), "RIFF");

        PerDirectorySelector selector;
        selector.set("jp", QStringLiteral("Shift_JIS"));
        selector.set("cn", QStringLiteral("GBK"));

        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), &selector, diagnostics);
        QVERIFY(bank.has_value());

        QVERIFY(bank->find(60, QString::fromUtf8("\xe3\x81\x82")));
        QVERIFY(bank->find(60, QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3")));
    }

    // Read from the record rather than asked, which is what writing it down was for.
    void a_recorded_encoding_is_not_asked_about_again() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",0,0,0,0,0\n");
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("hello-config.json"),
              R"({"$format":"hello-voicebank","charset":"Shift_JIS"})");

        PerDirectorySelector selector;
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), &selector, diagnostics);
        QVERIFY(bank.has_value());
        QCOMPARE(selector.asked, 0);
        QVERIFY(bank->find(60, QString::fromUtf8("\xe3\x81\x82")));
    }

    // Guessing is the one thing the encoding rules forbid, so a directory nobody can speak for
    // is left out and said so, not read in whatever happens to be handy.
    void a_directory_nobody_names_an_encoding_for_is_left_out() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",0,0,0,0,0\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), nullptr, diagnostics);
        QVERIFY(bank.has_value());
        QVERIFY(!bank->find(60, QString::fromUtf8("\xe3\x81\x82")));
        QVERIFY(!diagnostics.isEmpty());

        // The sample is still there to be reached by name, since a file name needed no encoding.
        const auto *sample = bank->find(60, QStringLiteral("a"));
        QVERIFY(sample);
        QVERIFY(!sample->hasEntry);
    }

    void an_encoding_that_does_not_exist_leaves_the_directory_out() {
        write(QStringLiteral("oto.ini"), "a.wav=a,0,0,0,0,0\n");
        write(QStringLiteral("a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("Klingon-1"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), &selector, diagnostics);
        QVERIFY(bank.has_value());
        QVERIFY(!diagnostics.isEmpty());

        const auto *sample = bank->find(60, QStringLiteral("a"));
        QVERIFY(sample);
        QVERIFY(!sample->hasEntry);
    }

    // A subdirectory with a character.txt of its own is a bank in its own right. Reading it here
    // would let it rename the one that was actually opened.
    void a_subdirectory_does_not_rename_the_bank() {
        write(QStringLiteral("character.txt"), "name=outer\n");
        write(QStringLiteral("inner/character.txt"), "name=inner\n");
        write(QStringLiteral("a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("UTF-8"));
        DiagnosticList diagnostics;
        const auto bank = VoiceBank::open(root(), &selector, diagnostics);
        QVERIFY(bank.has_value());
        QCOMPARE(bank->character().name, QStringLiteral("outer"));
    }
};

QTEST_APPLESS_MAIN(test_VoiceBank)

#include "test_VoiceBank.moc"
