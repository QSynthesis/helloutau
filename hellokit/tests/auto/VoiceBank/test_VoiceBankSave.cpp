#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Support/TextCodec.h>
#include <hellokit/VoiceBank/VoiceBank.h>

using namespace hello::kit;

// 葛平 in GBK, and あ in Shift_JIS.
static const QByteArray kGbkGePing = QByteArray("\xb8\xf0\xc6\xbd", 4);
static const QByteArray kShiftJisA = QByteArray("\x82\xa0", 2);

// 们, which simplified Chinese has and Shift_JIS does not.
static const QString kNotInShiftJis = QString::fromUtf8("\xe4\xbb\xac");

class test_VoiceBankSave : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    std::filesystem::path root() const {
        return std::filesystem::path(m_dir->path().toStdU16String());
    }

    QString pathOf(const QString &relative) const {
        return m_dir->path() + QLatin1Char('/') + relative;
    }

    void write(const QString &relative, const QByteArray &bytes) {
        const QString path = pathOf(relative);
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    QByteArray read(const QString &relative) const {
        QFile file(pathOf(relative));
        if (!file.open(QIODevice::ReadOnly)) {
            return "<missing>";
        }
        return file.readAll();
    }

    bool exists(const QString &relative) const {
        return QFileInfo::exists(pathOf(relative));
    }

    static std::optional<VoiceBank> open(const std::filesystem::path &root,
                                         const QString &charset) {
        FixedCharsetSelector selector(charset);
        DiagnosticList diagnostics;
        return VoiceBank::open(root, &selector, diagnostics);
    }

    /// The bank with the entry aliased \a alias changed by \a change .
    template <class F>
    static void edit(VoiceBank &bank, const QString &alias, F change) {
        auto samples = bank.samples();
        bool found = false;
        for (auto &sample : samples) {
            if (sample.hasEntry && sample.alias == alias) {
                change(sample);
                found = true;
            }
        }
        QVERIFY2(found, qPrintable(alias));
        bank.setSamples(samples);
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    // Opening and saving a bank nobody changed is not an edit, and must not look like one to
    // the author's version control or to UTAU.
    void a_bank_nobody_changed_is_left_as_it_was() {
        const QByteArray oto = "a.wav=" + kGbkGePing +
                               ",41.0,87.688,97.316,8.938,4.457\r\n"
                               "a.wav=- " +
                               kGbkGePing + ",41,87.6880,-143.414,8.938,04.457\r\n";
        const QByteArray character = "name=" + kGbkGePing + "\r\nVersion:1.0\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("character.txt"), character);
        write(QStringLiteral("readme.txt"), kGbkGePing);
        write(QStringLiteral("a.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("GBK"));
        QVERIFY(bank.has_value());

        DiagnosticList diagnostics;
        QVERIFY(bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(read(QStringLiteral("character.txt")), character);
        QCOMPARE(read(QStringLiteral("readme.txt")), kGbkGePing);

        // Nothing was written, so there was nothing to record an encoding for.
        QVERIFY(!exists(QStringLiteral("hello-config.json")));
    }

    // Line ends and order are how this file happens to be written, not something the user
    // changed, so an edit elsewhere in the directory must not rewrite it.
    void a_file_spelled_differently_is_not_rewritten_by_an_edit_elsewhere() {
        const QByteArray oto = "b.wav=b,1,2,3,4,5\n"
                               "a.wav=a,1,2,3,4,5\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("character.txt"), "name=old\n");
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(bank.has_value());

        auto directory = bank->directories().at(0);
        directory.character->name = QStringLiteral("new");
        bank->setDirectory(0, directory);

        DiagnosticList diagnostics;
        QVERIFY(bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(read(QStringLiteral("character.txt")), QByteArray("name=new\r\n"));
        QCOMPARE(bank->character().name, QStringLiteral("new"));
    }

    // The whole point of keeping the encoding: a GBK bank stays GBK, and every entry nobody
    // touched comes out as it went in.
    void a_changed_entry_is_written_in_the_encoding_it_was_read_in() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing +
                                             ",41.0,2,3,4,5\r\n"
                                             "b.wav=b,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("GBK"));
        QVERIFY(bank.has_value());
        edit(*bank, QStringLiteral("b"), [](VoiceSample &sample) {
            sample.alias = QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3");
            sample.offset = 12345.678;
        });

        DiagnosticList diagnostics;
        QVERIFY(bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), "a.wav=" + kGbkGePing +
                                                      ",41.0,2,3,4,5\r\n"
                                                      "b.wav=" +
                                                      kGbkGePing + ",12345.678,2,3,4,5\r\n");

        // Written down, so that the next open reads it in GBK without asking.
        DiagnosticList ignored;
        const auto config = VoiceBankConfig::open(root() / "hello-config.json", ignored);
        QVERIFY(config.has_value());
        QCOMPARE(config->charset, TextCodec(QStringLiteral("GBK")).name());
    }

    void text_the_encoding_cannot_hold_is_refused_and_nothing_is_written() {
        const QByteArray oto = "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("Shift_JIS"));
        QVERIFY(bank.has_value());
        edit(*bank, QString::fromUtf8("\xe3\x81\x82"),
             [](VoiceSample &sample) { sample.alias = kNotInShiftJis; });

        DiagnosticList diagnostics;
        QVERIFY(!bank->save(diagnostics));
        QVERIFY(!diagnostics.isEmpty());
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QVERIFY(!exists(QStringLiteral("hello-config.json")));
    }

    // Somebody else's work, UTAU's setParam for one. Replacing it would lose it without a word.
    void a_file_changed_on_disk_since_it_was_read_is_not_replaced() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(bank.has_value());
        edit(*bank, QStringLiteral("a"), [](VoiceSample &sample) { sample.offset = 100; });

        const QByteArray theirs = "a.wav=a,9,9,9,9,9\r\n";
        write(QStringLiteral("oto.ini"), theirs);

        DiagnosticList diagnostics;
        QVERIFY(!bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), theirs);
    }

    // The same, for a file that was not there when the bank was read.
    void a_file_that_appeared_since_is_not_replaced() {
        write(QStringLiteral("a.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(bank.has_value());
        auto samples = bank->samples();
        samples[0].hasEntry = true;
        samples[0].alias = QStringLiteral("a");
        bank->setSamples(samples);
        auto directory = bank->directories().at(0);
        directory.charset = QStringLiteral("UTF-8");
        bank->setDirectory(0, directory);

        const QByteArray theirs = "a.wav=theirs,1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), theirs);

        DiagnosticList diagnostics;
        QVERIFY(!bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), theirs);
    }

    // A directory that was never read has nothing to write its files from.
    void a_directory_never_read_is_never_written() {
        const QByteArray oto = "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto bank = VoiceBank::open(root(), nullptr, diagnostics);
        QVERIFY(bank.has_value());
        QVERIFY(bank->directories().at(0).leftOut);

        // Untouched, it saves, and writes nothing.
        QVERIFY(bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);

        auto samples = bank->samples();
        samples[0].hasEntry = true;
        bank->setSamples(samples);
        diagnostics.clear();
        QVERIFY(!bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
    }

    // What did not decode reads as empty, and writing the file would put the empty text where
    // the original was.
    void text_that_did_not_decode_is_not_written_back() {
        const QByteArray oto = "a.wav=" + kGbkGePing +
                               ",1,2,3,4,5\r\n"
                               "b.wav=b,1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(bank.has_value());
        QVERIFY(bank->directories().at(0).lossy);

        DiagnosticList diagnostics;
        QVERIFY(bank->save(diagnostics));

        edit(*bank, QStringLiteral("b"), [](VoiceSample &sample) { sample.offset = 100; });
        QVERIFY(!bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
    }

    // What a save wrote is what the next one compares against, or the second save would find
    // its own work and call it somebody else's.
    void a_bank_can_be_saved_again() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(bank.has_value());

        DiagnosticList diagnostics;
        edit(*bank, QStringLiteral("a"), [](VoiceSample &sample) { sample.offset = 10; });
        QVERIFY(bank->save(diagnostics));
        edit(*bank, QStringLiteral("a"), [](VoiceSample &sample) { sample.offset = 20; });
        QVERIFY(bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), QByteArray("a.wav=a,20,2,3,4,5\r\n"));
    }

    // Where case matters, oto.ini would be a second file beside the one UTAU reads.
    void a_file_is_written_back_under_the_name_it_was_found_by() {
        write(QStringLiteral("OTO.INI"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(bank.has_value());
        edit(*bank, QStringLiteral("a"), [](VoiceSample &sample) { sample.offset = 10; });

        DiagnosticList diagnostics;
        QVERIFY(bank->save(diagnostics));

        const auto names = QDir(m_dir->path()).entryList({QStringLiteral("*.ini")}, QDir::Files);
        QCOMPARE(names, QStringList{QStringLiteral("OTO.INI")});
        QCOMPARE(read(QStringLiteral("OTO.INI")), QByteArray("a.wav=a,10,2,3,4,5\r\n"));
    }

    // A directory of bare files has no encoding, and a first oto.ini there needs one. It is
    // asked for rather than guessed, the same as when reading.
    void a_first_oto_ini_needs_an_encoding() {
        write(QStringLiteral("ka.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(bank.has_value());
        QVERIFY(bank->directories().at(0).charset.isEmpty());

        auto samples = bank->samples();
        samples[0].hasEntry = true;
        samples[0].alias = QStringLiteral("ka");
        samples[0].offset = 5;
        bank->setSamples(samples);

        DiagnosticList diagnostics;
        QVERIFY(!bank->save(diagnostics));
        QVERIFY(!exists(QStringLiteral("oto.ini")));

        auto directory = bank->directories().at(0);
        directory.charset = QStringLiteral("UTF-8");
        bank->setDirectory(0, directory);
        QVERIFY(bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), QByteArray("ka.wav=ka,5,0,0,0,0\r\n"));
        QVERIFY(exists(QStringLiteral("hello-config.json")));
    }

    // One directory edited is one directory written. The others are not touched, and get no
    // record either.
    void only_the_directory_that_changed_is_written() {
        const QByteArray jp = "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n";
        write(QStringLiteral("jp/oto.ini"), jp);
        write(QStringLiteral("jp/a.wav"), "RIFF");
        write(QStringLiteral("en/oto.ini"), "b.wav=b,1,2,3,4,5\r\n");
        write(QStringLiteral("en/b.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        auto bank = VoiceBank::open(root(), &selector, diagnostics);
        QVERIFY(bank.has_value());
        edit(*bank, QStringLiteral("b"), [](VoiceSample &sample) { sample.offset = 9; });

        QVERIFY(bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("jp/oto.ini")), jp);
        QVERIFY(!exists(QStringLiteral("jp/hello-config.json")));
        QCOMPARE(read(QStringLiteral("en/oto.ini")), QByteArray("b.wav=b,9,2,3,4,5\r\n"));
        QVERIFY(exists(QStringLiteral("en/hello-config.json")));
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankSave)

#include "test_VoiceBankSave.moc"
