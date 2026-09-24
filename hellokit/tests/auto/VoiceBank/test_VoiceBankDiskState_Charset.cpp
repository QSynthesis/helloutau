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

// 葛平 in GBK and in UTF-8.
static const QByteArray kGbkGePing = QByteArray("\xb8\xf0\xc6\xbd", 4);
static const QByteArray kUtf8GePing = QByteArray("\xe8\x91\x9b\xe5\xb9\xb3");
static const QString kGePing = QString::fromUtf8(kUtf8GePing);

// 们 and 这 in GBK. Both are simplified Chinese characters absent from Shift_JIS.
static const QByteArray kGbkMen = QByteArray("\xc3\xc7", 2);
static const QByteArray kGbkZhe = QByteArray("\xd5\xe2", 2);

// あ in Shift_JIS. The same two bytes are also valid GBK and decode to a different character
// there, which is how a voice bank decoded in the wrong one of the two appears.
static const QByteArray kShiftJisA = QByteArray("\x82\xa0", 2);
static const QString kA = QString::fromUtf8("\xe3\x81\x82");

class test_VoiceBankDiskState_Charset : public QObject {
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

    static std::optional<VoiceBankDiskState::Opened> open(const std::filesystem::path &root,
                                                          const QString &charset) {
        FixedCharsetSelector selector(charset);
        DiagnosticList diagnostics;
        return VoiceBankDiskState::open(root, &selector, diagnostics);
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

    // Conversion: the text is retained, the bytes change, and the configuration records the new
    // encoding, which the next open uses.
    void converting_keeps_the_text_and_writes_every_file_anew() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("character.txt"), "name=" + kGbkGePing + "\r\n");
        write(QStringLiteral("readme.txt"), kGbkGePing);
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        recode(bank, 0, "UTF-8");

        DiagnosticList diagnostics;
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 "#Charset:UTF-8\r\na.wav=" + kUtf8GePing + ",1,2,3,4,5\r\n");
        QCOMPARE(read(QStringLiteral("character.txt")), "name=" + kUtf8GePing + "\r\n");
        QCOMPARE(read(QStringLiteral("readme.txt")), kUtf8GePing);
        QCOMPARE(recorded(), name("UTF-8"));

        // Reopened without querying the user.
        DiagnosticList again;
        const auto reopened = VoiceBank::open(root(), nullptr, again);
        QVERIFY(reopened.has_value());
        QVERIFY(reopened->find(60, kGePing));
        QCOMPARE(reopened->character().name, kGePing);
    }

    // Every unrepresentable text is reported, not only the first, so that a single attempt
    // informs the user of every required change.
    void converting_to_an_encoding_that_cannot_hold_the_text_names_all_of_it() {
        const QByteArray oto = "a.wav=" + kGbkMen +
                               ",1,2,3,4,5\r\n"
                               "b.wav=" +
                               kGbkZhe + ",1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        recode(bank, 0, "Shift_JIS");

        DiagnosticList diagnostics;
        QVERIFY(!disk.save(bank, diagnostics));
        QCOMPARE(diagnostics.size(), 2);
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QVERIFY(!exists(QStringLiteral("hello-config.json")));
    }

    // Plain ASCII is identical in both encodings, so no file needs rewriting. Only the
    // configuration changes, which is necessary because the next open would otherwise decode
    // the directory in the previous encoding. UTF-8 is not the target here, because converting to
    // it adds a declaration to the oto.ini.
    void converting_a_file_that_reads_the_same_rewrites_only_the_record() {
        const QByteArray oto = "a.wav=a,1,2,3,4,5\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QVERIFY(!disk.isModified(bank, std::filesystem::path()));
        recode(bank, 0, "Shift_JIS");
        // Modified although no file changes, because the encoding is to be recorded.
        QVERIFY(disk.isModified(bank, std::filesystem::path()));

        DiagnosticList diagnostics;
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(recorded(), name("Shift_JIS"));
        QVERIFY(!disk.isModified(bank, std::filesystem::path()));
    }

    // An oto.ini that declares its encoding is read in it without querying the user, and the
    // directory takes that encoding.
    void a_declared_oto_needs_no_selected_encoding() {
        write(QStringLiteral("oto.ini"),
              "#Charset:UTF-8\r\na.wav=" + kUtf8GePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        QVERIFY(!bank.directories().at(0).leftOut);
        QCOMPARE(bank.directories().at(0).otoCharset, QStringLiteral("UTF-8"));
        QCOMPARE(bank.directories().at(0).charset, name("UTF-8"));
        QVERIFY(bank.find(60, kGePing));
        QVERIFY(diagnostics.isEmpty());
    }

    // The declaration takes precedence for the oto.ini, the other files keep the encoding of the
    // directory, and the disagreement is reported.
    void the_declaration_takes_precedence_for_the_oto_only() {
        write(QStringLiteral("oto.ini"),
              "#Charset:UTF-8\r\na.wav=" + kUtf8GePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("character.txt"), "name=" + kGbkGePing + "\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("GBK"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QVERIFY(bank.find(60, kGePing));
        QCOMPARE(bank.character().name, kGePing);
        QCOMPARE(bank.directories().at(0).charset, name("GBK"));
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Warning);

        // A modified entry is written in the declared encoding, and the declaration is kept.
        auto samples = bank.samples();
        samples[0].offset = 7;
        bank.setSamples(samples);
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 "#Charset:UTF-8\r\na.wav=" + kUtf8GePing + ",7,2,3,4,5\r\n");
        QCOMPARE(read(QStringLiteral("character.txt")), "name=" + kGbkGePing + "\r\n");
    }

    // The declaration is written in one form, but an unmodified file is not rewritten.
    void an_unmodified_declaration_is_not_rewritten() {
        const QByteArray oto = "#charset:utf-8\r\na.wav=a,1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);

        auto samples = bank.samples();
        samples[0].offset = 7;
        bank.setSamples(samples);
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), "#Charset:utf-8\r\na.wav=a,7,2,3,4,5\r\n");
    }

    // A declaration of an unavailable encoding is reported and kept as written, and the file is
    // read and written in the encoding of the directory.
    void an_unavailable_declaration_falls_back_to_the_directory() {
        write(QStringLiteral("oto.ini"),
              "#Charset:Klingon-1\r\na.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("GBK"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QVERIFY(bank.find(60, kGePing));
        QCOMPARE(bank.directories().at(0).otoCharset, QStringLiteral("Klingon-1"));
        QCOMPARE(diagnostics.size(), 1);

        auto samples = bank.samples();
        samples[0].offset = 7;
        bank.setSamples(samples);
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 "#Charset:Klingon-1\r\na.wav=" + kGbkGePing + ",7,2,3,4,5\r\n");
    }

    // An oto.ini written in UTF-8 declares it, but an unmodified file is not rewritten for the
    // declaration alone.
    void a_utf8_oto_receives_the_declaration_when_written() {
        const QByteArray oto = "a.wav=" + kUtf8GePing + ",1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QVERIFY(bank.directories().at(0).otoCharset.isEmpty());

        DiagnosticList diagnostics;
        disk.rememberCharset(bank.directories().at(0).path);
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);

        auto samples = bank.samples();
        samples[0].offset = 7;
        bank.setSamples(samples);
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 "#Charset:UTF-8\r\na.wav=" + kUtf8GePing + ",7,2,3,4,5\r\n");
    }

    // A declaration would contradict the encoding in which the file is written after conversion.
    void converting_away_from_utf8_removes_the_declaration() {
        write(QStringLiteral("oto.ini"), "#Charset:UTF-8\r\na.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        recode(bank, 0, "Shift_JIS");
        QVERIFY(bank.directories().at(0).otoCharset.isEmpty());

        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), "a.wav=a,1,2,3,4,5\r\n");
        QCOMPARE(recorded(), name("Shift_JIS"));
    }

    // The alternative way of setting an encoding: the files are unchanged and decoded
    // differently.
    void rereading_in_the_right_encoding_mends_mojibake() {
        const QByteArray oto = "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QVERIFY(!bank.directories().at(0).lossy);
        QVERIFY(!bank.find(60, kA));

        DiagnosticList diagnostics;
        QVERIFY(disk.reread(bank, bank.directories().at(0).path, QStringLiteral("Shift_JIS"),
                            diagnostics));
        QVERIFY(bank.find(60, kA));
        QCOMPARE(bank.directories().at(0).charset, name("Shift_JIS"));

        // Only the decoding encoding changed, so the files are unchanged and the encoding is
        // recorded.
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(recorded(), name("Shift_JIS"));
    }

    void rereading_brings_in_a_directory_that_was_left_out() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QVERIFY(bank.directories().at(0).leftOut);

        QVERIFY(disk.reread(bank, bank.directories().at(0).path, QStringLiteral("Shift_JIS"),
                            diagnostics));
        QVERIFY(!bank.directories().at(0).leftOut);
        const auto *sample = bank.find(60, kA);
        QVERIFY(sample);
        QVERIFY(sample->hasEntry);
        QCOMPARE(bank.samples().size(), 1);
    }

    void rereading_drops_what_was_not_saved() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        auto samples = bank.samples();
        samples[0].offset = 100;
        bank.setSamples(samples);

        DiagnosticList diagnostics;
        QVERIFY(
            disk.reread(bank, bank.directories().at(0).path, QStringLiteral("UTF-8"), diagnostics));
        QCOMPARE(bank.samples().at(0).offset, 1.0);
    }

    // An encoding in which the files are invalid is incorrect, and recording it would apply it
    // every time the voice bank is opened.
    void an_encoding_that_does_not_read_is_not_remembered() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        DiagnosticList diagnostics;
        QVERIFY(
            disk.reread(bank, bank.directories().at(0).path, QStringLiteral("UTF-8"), diagnostics));
        QVERIFY(bank.directories().at(0).lossy);
        QVERIFY(!disk.hasUnrecordedCharsets());
        QVERIFY(disk.save(bank, diagnostics));
        QVERIFY(!exists(QStringLiteral("hello-config.json")));
    }

    // The encoding the user selected when the voice bank was opened is recorded, so that the
    // question is not repeated, and no other file is modified.
    void remembering_writes_the_record_and_nothing_else() {
        const QByteArray oto = "a.wav=" + kGbkGePing + ",1,2,3,4,5\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QVERIFY(!disk.hasUnrecordedCharsets());
        disk.rememberCharset(bank.directories().at(0).path);
        QVERIFY(disk.isModified(bank, std::filesystem::path()));
        QVERIFY(disk.hasUnrecordedCharsets());

        DiagnosticList diagnostics;
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(recorded(), name("GBK"));
        QVERIFY(!disk.isModified(bank, std::filesystem::path()));
        QVERIFY(!disk.hasUnrecordedCharsets());

        // The configuration written by the save becomes the baseline, so a second save detects
        // no external change.
        auto samples = bank.samples();
        samples[0].offset = 7;
        bank.setSamples(samples);
        QVERIFY(disk.save(bank, diagnostics));
    }

    // The precedence between duplicate aliases follows the sample order, and rereading one
    // directory must not move its samples behind those of the others.
    void rereading_keeps_the_directory_in_its_place() {
        write(QStringLiteral("a/oto.ini"), "one.wav=same,1,0,0,0,0\r\n");
        write(QStringLiteral("a/one.wav"), "RIFF");
        write(QStringLiteral("b/oto.ini"), "two.wav=same,2,0,0,0,0\r\n");
        write(QStringLiteral("b/two.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        const auto *before = bank.find(60, QStringLiteral("same"));
        QVERIFY(before);
        const auto winner = before->fileName;
        int index = -1;
        for (int i = 0; i < bank.directories().size(); ++i) {
            if (bank.directories().at(i).path == before->path.parent_path().filename()) {
                index = i;
            }
        }
        QVERIFY(index >= 0);

        DiagnosticList diagnostics;
        QVERIFY(disk.reread(bank, bank.directories().at(index).path, QStringLiteral("UTF-8"),
                            diagnostics));
        const auto *after = bank.find(60, QStringLiteral("same"));
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

QTEST_APPLESS_MAIN(test_VoiceBankDiskState_Charset)

#include "test_VoiceBankDiskState_Charset.moc"
