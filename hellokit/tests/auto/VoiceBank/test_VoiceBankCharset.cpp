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

// 葛平 in GBK and in UTF-8.
static const QByteArray kGbkGePing = QByteArray("\xb8\xf0\xc6\xbd", 4);
static const QByteArray kUtf8GePing = QByteArray("\xe8\x91\x9b\xe5\xb9\xb3");
static const QString kGePing = QString::fromUtf8(kUtf8GePing);

// 们 and 这 in GBK. Simplified Chinese has them and Shift_JIS has neither.
static const QByteArray kGbkMen = QByteArray("\xc3\xc7", 2);
static const QByteArray kGbkZhe = QByteArray("\xd5\xe2", 2);

// あ in Shift_JIS. The same two bytes are valid GBK as well and read as something else there,
// which is what a bank read in the wrong one of the two looks like.
static const QByteArray kShiftJisA = QByteArray("\x82\xa0", 2);
static const QString kA = QString::fromUtf8("\xe3\x81\x82");

class test_VoiceBankCharset : public QObject {
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

    std::optional<QString> recorded(const QString &relative = QString()) const {
        DiagnosticList ignored;
        const auto config = VoiceBankConfig::open(
            root() / relative.toStdU16String() / "hello-config.json", ignored);
        if (!config) {
            return std::nullopt;
        }
        return config->charset;
    }

    static std::optional<VoiceBank> open(const std::filesystem::path &root,
                                         const QString &charset) {
        FixedCharsetSelector selector(charset);
        DiagnosticList diagnostics;
        return VoiceBank::open(root, &selector, diagnostics);
    }

    static QString name(const char *charset) {
        return TextCodec(QString::fromLatin1(charset)).name();
    }

    static void recode(VoiceBank &bank, int index, const char *charset) {
        auto directory = bank.directories().at(index);
        directory.charset = QString::fromLatin1(charset);
        bank.setDirectory(index, directory);
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    // Converting: the text stays, the bytes change, and the record says so, which is what the
    // next open goes by.
    void converting_keeps_the_text_and_writes_every_file_anew() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("character.txt"), "name=" + kGbkGePing + "\r\n");
        write(QStringLiteral("readme.txt"), kGbkGePing);
        write(QStringLiteral("a.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("GBK"));
        QVERIFY(bank.has_value());
        recode(*bank, 0, "UTF-8");

        DiagnosticList diagnostics;
        QVERIFY(bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), "a.wav=" + kUtf8GePing + ",1,2,3,4,5\r\n");
        QCOMPARE(read(QStringLiteral("character.txt")), "name=" + kUtf8GePing + "\r\n");
        QCOMPARE(read(QStringLiteral("readme.txt")), kUtf8GePing);
        QCOMPARE(recorded(), name("UTF-8"));

        // Opened again, and asked nothing.
        DiagnosticList again;
        const auto reopened = VoiceBank::open(root(), nullptr, again);
        QVERIFY(reopened.has_value());
        QVERIFY(reopened->find(60, kGePing));
        QCOMPARE(reopened->character().name, kGePing);
    }

    // Every piece of text that does not fit is named, not only the first, so that one attempt
    // tells the user all there is to change.
    void converting_to_an_encoding_that_cannot_hold_the_text_names_all_of_it() {
        const QByteArray oto = "a.wav=" + kGbkMen +
                               ",1,2,3,4,5\r\n"
                               "b.wav=" +
                               kGbkZhe + ",1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("GBK"));
        QVERIFY(bank.has_value());
        recode(*bank, 0, "Shift_JIS");

        DiagnosticList diagnostics;
        QVERIFY(!bank->save(diagnostics));
        QCOMPARE(diagnostics.size(), 2);
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QVERIFY(!exists(QStringLiteral("hello-config.json")));
    }

    // Plain ASCII reads the same in either, so there is nothing to rewrite. Only the record
    // changes, and it has to, or the next open would read the directory in the old one.
    void converting_a_file_that_reads_the_same_rewrites_only_the_record() {
        const QByteArray oto = "a.wav=a,1,2,3,4,5\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("GBK"));
        QVERIFY(bank.has_value());
        recode(*bank, 0, "UTF-8");

        DiagnosticList diagnostics;
        QVERIFY(bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(recorded(), name("UTF-8"));
    }

    // The other way to set an encoding: the files stay, and are read differently.
    void rereading_in_the_right_encoding_mends_mojibake() {
        const QByteArray oto = "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("GBK"));
        QVERIFY(bank.has_value());
        QVERIFY(!bank->directories().at(0).lossy);
        QVERIFY(!bank->find(60, kA));

        DiagnosticList diagnostics;
        QVERIFY(bank->reread(0, QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(bank->find(60, kA));
        QCOMPARE(bank->directories().at(0).charset, name("Shift_JIS"));

        // Nothing was changed but the encoding it is read in, so the files stay, and the
        // encoding is written down.
        QVERIFY(bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(recorded(), name("Shift_JIS"));
    }

    void rereading_brings_in_a_directory_that_was_left_out() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto bank = VoiceBank::open(root(), nullptr, diagnostics);
        QVERIFY(bank.has_value());
        QVERIFY(bank->directories().at(0).leftOut);

        QVERIFY(bank->reread(0, QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(!bank->directories().at(0).leftOut);
        const auto *sample = bank->find(60, kA);
        QVERIFY(sample);
        QVERIFY(sample->hasEntry);
        QCOMPARE(bank->samples().size(), 1);
    }

    void rereading_drops_what_was_not_saved() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(bank.has_value());
        auto samples = bank->samples();
        samples[0].offset = 100;
        bank->setSamples(samples);

        DiagnosticList diagnostics;
        QVERIFY(bank->reread(0, QStringLiteral("UTF-8"), diagnostics));
        QCOMPARE(bank->samples().at(0).offset, 1.0);
    }

    // An encoding that does not read the files is a wrong guess, and writing it down would
    // make it the answer every time the bank is opened.
    void an_encoding_that_does_not_read_is_not_remembered() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("GBK"));
        QVERIFY(bank.has_value());

        DiagnosticList diagnostics;
        QVERIFY(bank->reread(0, QStringLiteral("UTF-8"), diagnostics));
        QVERIFY(bank->directories().at(0).lossy);
        QVERIFY(bank->save(diagnostics));
        QVERIFY(!exists(QStringLiteral("hello-config.json")));
    }

    // What a user answered when the bank was opened, written down so the question does not
    // come back, and nothing else touched.
    void remembering_writes_the_record_and_nothing_else() {
        const QByteArray oto = "a.wav=" + kGbkGePing + ",1,2,3,4,5\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("GBK"));
        QVERIFY(bank.has_value());
        bank->rememberCharset(0);

        DiagnosticList diagnostics;
        QVERIFY(bank->save(diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(recorded(), name("GBK"));

        // And the record it wrote is its own, so a second save finds nothing changed under it.
        auto samples = bank->samples();
        samples[0].offset = 7;
        bank->setSamples(samples);
        QVERIFY(bank->save(diagnostics));
    }

    // Which of two equal aliases wins goes by the order the samples are in, and rereading one
    // directory must not move it behind the others.
    void rereading_keeps_the_directory_in_its_place() {
        write(QStringLiteral("a/oto.ini"), "one.wav=same,1,0,0,0,0\r\n");
        write(QStringLiteral("a/one.wav"), "RIFF");
        write(QStringLiteral("b/oto.ini"), "two.wav=same,2,0,0,0,0\r\n");
        write(QStringLiteral("b/two.wav"), "RIFF");

        auto bank = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(bank.has_value());
        const auto *before = bank->find(60, QStringLiteral("same"));
        QVERIFY(before);
        const auto winner = before->fileName;
        int index = -1;
        for (int i = 0; i < bank->directories().size(); ++i) {
            if (bank->directories().at(i).path == before->path.parent_path().filename()) {
                index = i;
            }
        }
        QVERIFY(index >= 0);

        DiagnosticList diagnostics;
        QVERIFY(bank->reread(index, QStringLiteral("UTF-8"), diagnostics));
        const auto *after = bank->find(60, QStringLiteral("same"));
        QVERIFY(after);
        QCOMPARE(after->fileName, winner);
    }

    void whether_utau_reads_an_encoding_here() {
#ifdef Q_OS_WIN
        QVERIFY(VoiceBank::isCharsetReadableByUtau(TextCodec::systemName()));
        QCOMPARE(VoiceBank::isCharsetReadableByUtau(QStringLiteral("UTF-8")),
                 TextCodec(TextCodec::systemName()).isUtf8());
#else
        QVERIFY(!VoiceBank::isCharsetReadableByUtau(TextCodec::systemName()));
#endif
        QVERIFY(!VoiceBank::isCharsetReadableByUtau(QStringLiteral("Klingon-1")));
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankCharset)

#include "test_VoiceBankCharset.moc"
