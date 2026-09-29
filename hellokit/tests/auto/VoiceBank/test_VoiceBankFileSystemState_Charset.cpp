#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Support/TextCodec.h>
#include <hellokit/VoiceBank/VoiceBank.h>
#include <hellokit/VoiceBank/VoiceBankFileSystemState.h>

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

class test_VoiceBankFileSystemState_Charset : public QObject {
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

    static std::optional<VoiceBankFileSystemState::Opened> open(const std::filesystem::path &root,
                                                                const QString &charset) {
        FixedCharsetSelector selector(charset);
        DiagnosticList diagnostics;
        return VoiceBankFileSystemState::open(root, &selector, diagnostics);
    }

    static QString name(const char *charset) {
        return TextCodec(QString::fromLatin1(charset)).name();
    }

    static void recode(VoiceBank &bank, int index, const char *charset) {
        auto directory = bank.directories().at(index);
        directory.charset = QString::fromLatin1(charset);
        bank.setDirectory(index, directory);
    }

    static int count(const DiagnosticList &diagnostics, DiagnosticSeverity severity) {
        return int(
            std::count_if(diagnostics.begin(), diagnostics.end(),
                          [severity](const Diagnostic &d) { return d.severity == severity; }));
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
        auto &files = opened->files;
        recode(bank, 0, "UTF-8");

        DiagnosticList diagnostics;
        QVERIFY(files.save(bank, diagnostics));
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
        auto &files = opened->files;
        recode(bank, 0, "Shift_JIS");

        DiagnosticList diagnostics;
        QVERIFY(!files.save(bank, diagnostics));
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
        auto &files = opened->files;
        QVERIFY(!files.isModified(bank, std::filesystem::path()));
        recode(bank, 0, "Shift_JIS");
        // Modified although no file changes, because the encoding is to be recorded.
        QVERIFY(files.isModified(bank, std::filesystem::path()));

        DiagnosticList diagnostics;
        QVERIFY(files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(recorded(), name("Shift_JIS"));
        QVERIFY(!files.isModified(bank, std::filesystem::path()));
    }

    // An oto.ini that declares its encoding is read in it without querying the user, and the
    // directory takes that encoding.
    void a_declared_oto_needs_no_selected_encoding() {
        write(QStringLiteral("oto.ini"),
              "#Charset:UTF-8\r\na.wav=" + kUtf8GePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        QVERIFY(!bank.directories().at(0).leftOut);
        QCOMPARE(bank.directories().at(0).charset, name("UTF-8"));
        QVERIFY(bank.find(60, kGePing));
        QVERIFY(diagnostics.isEmpty());
    }

    // The declaration determines the encoding of the entire directory, over the one that the
    // configuration records, and the disagreement is reported. A file in another encoding reads
    // with U+FFFD in place of its invalid bytes.
    void the_declaration_determines_the_encoding_of_the_directory() {
        write(QStringLiteral("oto.ini"),
              "#Charset:UTF-8\r\na.wav=" + kUtf8GePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("character.txt"), "name=" + kUtf8GePing + "\r\n");
        write(QStringLiteral("readme.txt"), kGbkGePing);
        write(QStringLiteral("hello-config.json"),
              R"({"$format":"hello-voicebank","charset":"GBK"})");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &files = opened->files;
        QVERIFY(bank.find(60, kGePing));
        QCOMPARE(bank.character().name, kGePing);
        QVERIFY(bank.readme().contains(QChar::ReplacementCharacter));
        QCOMPARE(bank.directories().at(0).charset, name("UTF-8"));
        QCOMPARE(count(diagnostics, DiagnosticSeverity::Warning), 2);
        QCOMPARE(diagnostics.size(), 2);

        // A modified entry is written with the declaration, the readme is left as it is, and the
        // encoding in effect is recorded.
        auto samples = bank.samples();
        samples[0].offset = 7;
        bank.setSamples(samples);
        QVERIFY(files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 "#Charset:UTF-8\r\na.wav=" + kUtf8GePing + ",7,2,3,4,5\r\n");
        QCOMPARE(read(QStringLiteral("readme.txt")), kGbkGePing);
        QCOMPARE(recorded(), name("UTF-8"));
    }

    // The declaration is written in one form, but an unmodified file is not rewritten.
    void an_unmodified_declaration_is_not_rewritten() {
        const QByteArray oto = "#charset:utf-8\r\na.wav=a,1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &files = opened->files;
        QVERIFY(files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);

        auto samples = bank.samples();
        samples[0].offset = 7;
        bank.setSamples(samples);
        QVERIFY(files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), "#Charset:UTF-8\r\na.wav=a,7,2,3,4,5\r\n");
    }

    // UTF8 without the hyphen declares UTF-8 as well, and is written with it.
    void utf8_without_the_hyphen_is_a_declaration() {
        write(QStringLiteral("oto.ini"),
              "#Charset:Utf8\r\na.wav=" + kUtf8GePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        QVERIFY(bank.find(60, kGePing));
        QCOMPARE(bank.directories().at(0).charset, name("UTF-8"));

        auto samples = bank.samples();
        samples[0].offset = 7;
        bank.setSamples(samples);
        QVERIFY(opened->files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 "#Charset:UTF-8\r\na.wav=" + kUtf8GePing + ",7,2,3,4,5\r\n");
    }

    // A declaration can state UTF-8 only. Any other is treated as absent, without a warning: the
    // file is read in the encoding of the directory, which is therefore asked for, and written
    // without the line.
    void a_declaration_of_another_encoding_is_absent() {
        write(QStringLiteral("oto.ini"),
              "#Charset:Shift_JIS\r\na.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        const auto unselected = VoiceBankFileSystemState::open(root(), nullptr, diagnostics);
        QVERIFY(unselected.has_value());
        QVERIFY(unselected->bank.directories().at(0).leftOut);

        FixedCharsetSelector selector(QStringLiteral("GBK"));
        diagnostics.clear();
        auto opened = VoiceBankFileSystemState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &files = opened->files;
        QVERIFY(bank.find(60, kGePing));
        QCOMPARE(bank.directories().at(0).charset, name("GBK"));
        QVERIFY(diagnostics.isEmpty());

        auto samples = bank.samples();
        samples[0].offset = 7;
        bank.setSamples(samples);
        QVERIFY(files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), "a.wav=" + kGbkGePing + ",7,2,3,4,5\r\n");
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
        auto &files = opened->files;

        DiagnosticList diagnostics;
        files.rememberCharset(bank.directories().at(0).path);
        QVERIFY(files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);

        auto samples = bank.samples();
        samples[0].offset = 7;
        bank.setSamples(samples);
        QVERIFY(files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 "#Charset:UTF-8\r\na.wav=" + kUtf8GePing + ",7,2,3,4,5\r\n");
    }

    // A declaration would contradict the encoding in which the file is written after conversion.
    void converting_away_from_utf8_removes_the_declaration() {
        write(QStringLiteral("oto.ini"), "#Charset:UTF-8\r\na.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &files = opened->files;
        recode(bank, 0, "Shift_JIS");

        QVERIFY(files.save(bank, diagnostics));
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
        auto &files = opened->files;
        QVERIFY(!bank.find(60, kA));

        DiagnosticList diagnostics;
        QVERIFY(files.reread(bank, bank.directories().at(0).path, QStringLiteral("Shift_JIS"),
                             diagnostics));
        QVERIFY(bank.find(60, kA));
        QCOMPARE(bank.directories().at(0).charset, name("Shift_JIS"));

        // Only the decoding encoding changed, so the files are unchanged and the encoding is
        // recorded.
        QVERIFY(files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(recorded(), name("Shift_JIS"));
    }

    // The directories that nothing determines are asked about at once, and each takes its own
    // answer, or is left out without one.
    void the_directories_are_asked_about_at_once() {
        class Batch : public VoiceBankCharsetSelector {
        public:
            std::optional<QString> selectCharset(const VoiceBankDirectorySource &,
                                                 DiagnosticList &) override {
                ++single;
                return std::nullopt;
            }

            QList<std::optional<QString>>
                selectCharsets(const QList<const VoiceBankDirectorySource *> &directories,
                               DiagnosticList &) override {
                ++batches;
                for (const auto directory : directories) {
                    asked.push_back(directory->path);
                }
                return {QStringLiteral("Shift_JIS"), std::nullopt};
            }

            int single = 0;
            int batches = 0;
            QList<std::filesystem::path> asked;
        };

        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("sub/oto.ini"), "b.wav=" + kShiftJisA + ",1,2,3,4,5\r\n");
        write(QStringLiteral("sub/b.wav"), "RIFF");
        write(QStringLiteral("utf/oto.ini"), "#Charset:UTF-8\r\nc.wav=c,1,2,3,4,5\r\n");
        write(QStringLiteral("utf/c.wav"), "RIFF");

        Batch selector;
        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        QCOMPARE(selector.batches, 1);
        QCOMPARE(selector.single, 0);
        QCOMPARE(selector.asked, (QList<std::filesystem::path>{std::filesystem::path(), "sub"}));
        const auto &directories = opened->bank.directories();
        QCOMPARE(directories.size(), 3);
        QVERIFY(!directories.at(0).leftOut);
        QCOMPARE(directories.at(0).charset, name("Shift_JIS"));
        QVERIFY(directories.at(1).leftOut);
        QVERIFY(!directories.at(2).leftOut);
    }

    void rereading_brings_in_a_directory_that_was_left_out() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &files = opened->files;
        QVERIFY(bank.directories().at(0).leftOut);

        QVERIFY(files.reread(bank, bank.directories().at(0).path, QStringLiteral("Shift_JIS"),
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
        auto &files = opened->files;
        auto samples = bank.samples();
        samples[0].offset = 100;
        bank.setSamples(samples);

        DiagnosticList diagnostics;
        QVERIFY(files.reread(bank, bank.directories().at(0).path, QStringLiteral("UTF-8"),
                             diagnostics));
        QCOMPARE(bank.samples().at(0).offset, 1.0);
    }

    // The user chose the encoding, so it is recorded even if some bytes are invalid in it. The
    // file is not rewritten, because it did not change, and keeps its bytes.
    void an_encoding_with_invalid_bytes_is_remembered_and_the_file_kept() {
        const QByteArray oto = "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &files = opened->files;

        DiagnosticList diagnostics;
        QVERIFY(files.reread(bank, bank.directories().at(0).path, QStringLiteral("UTF-8"),
                             diagnostics));
        QCOMPARE(count(diagnostics, DiagnosticSeverity::Warning), 1);
        QVERIFY(bank.samples().at(0).alias.contains(QChar::ReplacementCharacter));
        QVERIFY(files.hasUnrecordedCharsets());
        QVERIFY(files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(recorded(), name("UTF-8"));
    }

    // Invalid bytes are read as U+FFFD and the rest of the file is usable, as a readme with a
    // mistyped character leaves the voice bank usable. Each file with invalid bytes is reported.
    void invalid_bytes_are_read_as_replacement_characters() {
        write(QStringLiteral("oto.ini"),
              "a.wav=" + kShiftJisA + "\x82" + ",1,2,3,4,5\r\nb.wav=b,1,2,3,4,5\r\n");
        write(QStringLiteral("readme.txt"), kShiftJisA + "\x82\x20" + kShiftJisA);
        write(QStringLiteral("a.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        QVERIFY(!bank.directories().at(0).leftOut);
        QCOMPARE(count(diagnostics, DiagnosticSeverity::Warning), 2);
        QVERIFY(bank.find(60, QStringLiteral("b")));
        QVERIFY(bank.find(60, kA + QChar(QChar::ReplacementCharacter)));
        QCOMPARE(bank.readme(), kA + QChar(QChar::ReplacementCharacter) + QLatin1Char(' ') + kA);
    }

    // Writing U+FFFD would replace the original bytes, so a changed file containing it is
    // refused, and every such text is named at once. An unchanged file is not written and not
    // refused, so the rest of the voice bank can still be saved.
    void a_changed_file_containing_replacement_characters_is_refused() {
        const QByteArray oto = "a.wav=" + kShiftJisA + "\x82" +
                               ",1,2,3,4,5\r\n"
                               "b.wav=b\x82,1,2,3,4,5\r\n"
                               "c.wav=c,1,2,3,4,5\r\n";
        const QByteArray readme = kShiftJisA + "\x82\x20";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("readme.txt"), readme);
        write(QStringLiteral("character.txt"), "name=a\r\n");

        auto opened = open(root(), QStringLiteral("Shift_JIS"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &files = opened->files;

        // The character.txt changes, and the files containing U+FFFD do not.
        auto directory = bank.directories().at(0);
        directory.character->name = QStringLiteral("b");
        bank.setDirectory(0, directory);
        DiagnosticList diagnostics;
        QVERIFY(files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(read(QStringLiteral("readme.txt")), readme);
        QCOMPARE(read(QStringLiteral("character.txt")), QByteArray("name=b\r\n"));

        // An entry without U+FFFD changes, which rewrites the oto.ini with the two that contain
        // it. The readme changes as well. Nothing is written.
        auto samples = bank.samples();
        for (auto &sample : samples) {
            if (sample.fileName == QStringLiteral("c.wav")) {
                sample.offset = 7;
            }
        }
        bank.setSamples(samples);
        directory = bank.directories().at(0);
        directory.readme += QStringLiteral("x");
        directory.character->name = QStringLiteral("c");
        bank.setDirectory(0, directory);
        diagnostics.clear();
        QVERIFY(!files.save(bank, diagnostics));
        QCOMPARE(count(diagnostics, DiagnosticSeverity::Error), 3);
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(read(QStringLiteral("readme.txt")), readme);
        QCOMPARE(read(QStringLiteral("character.txt")), QByteArray("name=b\r\n"));
    }

    // In UTF-8, U+FFFD is representable, and only the rule against it keeps the original bytes.
    void replacement_characters_are_refused_in_utf8_as_well() {
        const QByteArray readme = "a\xff";
        write(QStringLiteral("readme.txt"), readme);

        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto directory = bank.directories().at(0);
        directory.readme += QStringLiteral("b");
        bank.setDirectory(0, directory);
        DiagnosticList diagnostics;
        QVERIFY(!opened->files.save(bank, diagnostics));
        QCOMPARE(count(diagnostics, DiagnosticSeverity::Error), 1);
        QCOMPARE(read(QStringLiteral("readme.txt")), readme);
    }

    // Replacing U+FFFD with a question mark is a change and is written, although Shift_JIS writes
    // both as a question mark.
    void replacing_a_replacement_character_is_a_change() {
        write(QStringLiteral("readme.txt"), kShiftJisA + "\x82\x20");

        auto opened = open(root(), QStringLiteral("Shift_JIS"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto directory = bank.directories().at(0);
        directory.readme = kA + QStringLiteral("? ");
        bank.setDirectory(0, directory);
        QVERIFY(opened->files.isModified(bank, {}));
        DiagnosticList diagnostics;
        QVERIFY(opened->files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("readme.txt")), kShiftJisA + "? ");
        QCOMPARE(recorded(), name("Shift_JIS"));
    }

    // A declaration of UTF-8 that another program adds takes precedence over the encoding the
    // directory was read in when it is read again.
    void a_declaration_written_elsewhere_takes_precedence_on_reload() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &files = opened->files;

        write(QStringLiteral("oto.ini"),
              "#Charset:UTF-8\r\na.wav=" + kUtf8GePing + ",1,2,3,4,5\r\n");
        const auto changes = files.checkDisk();
        QCOMPARE(changes.changed.size(), 1);
        DiagnosticList diagnostics;
        files.reloadFromDisk(bank, changes, nullptr, diagnostics);
        QVERIFY(bank.find(60, kGePing));
        QCOMPARE(bank.directories().at(0).charset, name("UTF-8"));
    }

    // A text file that appears in a directory already read is read in its encoding, without a
    // selector.
    void a_text_file_that_appears_is_read_in_the_encoding_of_its_directory() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n");

        auto opened = open(root(), QStringLiteral("Shift_JIS"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &files = opened->files;

        write(QStringLiteral("readme.txt"), kShiftJisA);
        const auto changes = files.checkDisk();
        QCOMPARE(changes.changed.size(), 1);
        DiagnosticList diagnostics;
        files.reloadFromDisk(bank, changes, nullptr, diagnostics);
        QCOMPARE(bank.readme(), kA);
        QVERIFY(diagnostics.isEmpty());
    }

    // A directory left out has no known encoding, so when it is read again it takes the one that
    // its configuration records by then, without a selector.
    void a_directory_left_out_takes_a_recorded_encoding_when_read_again() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n");

        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &files = opened->files;
        QVERIFY(bank.directories().at(0).leftOut);

        write(QStringLiteral("hello-config.json"),
              R"({"$format":"hello-voicebank","charset":"Shift_JIS"})");
        write(QStringLiteral("oto.ini"), "a.wav=" + kShiftJisA + ",7,2,3,4,5\r\n");
        const auto changes = files.checkDisk();
        QCOMPARE(changes.changed.size(), 1);
        files.reloadFromDisk(bank, changes, nullptr, diagnostics);
        QVERIFY(!bank.directories().at(0).leftOut);
        QCOMPARE(bank.find(60, kA)->offset, 7.0);
    }

    // A directory whose oto.ini declares UTF-8 is in UTF-8, and reading it in another encoding is
    // refused rather than contradicting the declaration.
    void rereading_a_declared_directory_in_another_encoding_is_refused() {
        write(QStringLiteral("oto.ini"),
              "#Charset:UTF-8\r\na.wav=" + kUtf8GePing + ",1,2,3,4,5\r\n");

        DiagnosticList diagnostics;
        auto opened = VoiceBankFileSystemState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        QVERIFY(!opened->files.reread(bank, {}, QStringLiteral("GBK"), diagnostics));
        QVERIFY(hasError(diagnostics));
        QVERIFY(bank.find(60, kGePing));
        QVERIFY(!opened->files.hasUnrecordedCharsets());

        diagnostics.clear();
        QVERIFY(opened->files.reread(bank, {}, QStringLiteral("utf-8"), diagnostics));
        QVERIFY(bank.find(60, kGePing));
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
        auto &files = opened->files;
        QVERIFY(!files.hasUnrecordedCharsets());
        files.rememberCharset(bank.directories().at(0).path);
        QVERIFY(files.isModified(bank, std::filesystem::path()));
        QVERIFY(files.hasUnrecordedCharsets());

        DiagnosticList diagnostics;
        QVERIFY(files.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(recorded(), name("GBK"));
        QVERIFY(!files.isModified(bank, std::filesystem::path()));
        QVERIFY(!files.hasUnrecordedCharsets());

        // The configuration written by the save becomes the baseline, so a second save detects
        // no external change.
        auto samples = bank.samples();
        samples[0].offset = 7;
        bank.setSamples(samples);
        QVERIFY(files.save(bank, diagnostics));
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
        auto &files = opened->files;
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
        QVERIFY(files.reread(bank, bank.directories().at(index).path, QStringLiteral("UTF-8"),
                             diagnostics));
        const auto *after = bank.find(60, QStringLiteral("same"));
        QVERIFY(after);
        QCOMPARE(after->fileName, winner);
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankFileSystemState_Charset)

#include "test_VoiceBankFileSystemState_Charset.moc"
