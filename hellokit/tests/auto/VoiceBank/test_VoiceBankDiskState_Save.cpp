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

namespace fs = std::filesystem;

// 葛平 in GBK, and あ in Shift_JIS.
static const QByteArray kGbkGePing = QByteArray("\xb8\xf0\xc6\xbd", 4);
static const QByteArray kShiftJisA = QByteArray("\x82\xa0", 2);

// 们, which simplified Chinese has and Shift_JIS does not.
static const QString kNotInShiftJis = QString::fromUtf8("\xe4\xbb\xac");

class test_VoiceBankDiskState_Save : public QObject {
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

    static std::optional<VoiceBankDiskState::Opened> open(const std::filesystem::path &root,
                                                          const QString &charset) {
        FixedCharsetSelector selector(charset);
        DiagnosticList diagnostics;
        return VoiceBankDiskState::open(root, &selector, diagnostics);
    }

    /// The voice bank with the entry of alias \a alias modified by \a change .
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

    // Opening and saving an unmodified voice bank is not an edit, and must not appear as one to
    // the version control of the author or to UTAU.
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

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        DiagnosticList diagnostics;
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(read(QStringLiteral("character.txt")), character);
        QCOMPARE(read(QStringLiteral("readme.txt")), kGbkGePing);

        // No file was written, so no encoding was recorded.
        QVERIFY(!exists(QStringLiteral("hello-config.json")));
    }

    // The line endings and the order are properties of the existing file, not user changes, so
    // an edit elsewhere in the directory must not rewrite it.
    void a_file_spelled_differently_is_not_rewritten_by_an_edit_elsewhere() {
        const QByteArray oto = "b.wav=b,1,2,3,4,5\n"
                               "a.wav=a,1,2,3,4,5\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("character.txt"), "name=old\n");
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        auto directory = bank.directories().at(0);
        directory.character->name = QStringLiteral("new");
        bank.setDirectory(0, directory);

        DiagnosticList diagnostics;
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QCOMPARE(read(QStringLiteral("character.txt")), QByteArray("name=new\r\n"));
        QCOMPARE(bank.character().name, QStringLiteral("new"));
    }

    // Entries whose numbers are all empty, as in the breath samples of a real voice bank. Editing
    // one of them must not write the others as zeros.
    void empty_numbers_of_other_entries_stay_empty() {
        write(QStringLiteral("oto.ini"), "01.wav=,,,,,\r\n"
                                         "02.wav=,,,,,\r\n");
        write(QStringLiteral("01.wav"), "RIFF");
        write(QStringLiteral("02.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        auto samples = bank.samples();
        for (auto &sample : samples) {
            if (sample.fileName == QStringLiteral("02.wav")) {
                sample.cutoff = -300;
            }
        }
        bank.setSamples(samples);

        DiagnosticList diagnostics;
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), QByteArray("01.wav=,,,,,\r\n"
                                                             "02.wav=,,,-300,,\r\n"));
    }

    // The purpose of retaining the encoding: a GBK voice bank remains GBK, and every unmodified
    // entry is saved unchanged.
    void a_changed_entry_is_written_in_the_encoding_it_was_read_in() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing +
                                             ",41.0,2,3,4,5\r\n"
                                             "b.wav=b,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        edit(bank, QStringLiteral("b"), [](VoiceSample &sample) {
            sample.alias = QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3");
            sample.offset = 12345.678;
        });

        DiagnosticList diagnostics;
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), "a.wav=" + kGbkGePing +
                                                      ",41.0,2,3,4,5\r\n"
                                                      "b.wav=" +
                                                      kGbkGePing + ",12345.678,2,3,4,5\r\n");

        // Recorded, so that the next open decodes it as GBK without querying the user.
        DiagnosticList ignored;
        const auto config = VoiceBankConfig::open(root() / "hello-config.json", ignored);
        QVERIFY(config.has_value());
        QCOMPARE(config->charset, TextCodec(QStringLiteral("GBK")).name());
    }

    void text_the_encoding_cannot_hold_is_refused_and_nothing_is_written() {
        const QByteArray oto = "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("Shift_JIS"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        edit(bank, QString::fromUtf8("\xe3\x81\x82"),
             [](VoiceSample &sample) { sample.alias = kNotInShiftJis; });

        DiagnosticList diagnostics;
        QVERIFY(!disk.save(bank, diagnostics));
        QVERIFY(!diagnostics.isEmpty());
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
        QVERIFY(!exists(QStringLiteral("hello-config.json")));
    }

    // Changes made by another program, such as the setParam tool of UTAU. Overwriting them
    // would lose them silently.
    void a_file_changed_on_disk_since_it_was_read_is_not_replaced() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        edit(bank, QStringLiteral("a"), [](VoiceSample &sample) { sample.offset = 100; });

        const QByteArray theirs = "a.wav=a,9,9,9,9,9\r\n";
        write(QStringLiteral("oto.ini"), theirs);

        DiagnosticList diagnostics;
        QVERIFY(!disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), theirs);
    }

    // The same for a file that did not exist when the voice bank was read.
    void a_file_that_appeared_since_is_not_replaced() {
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        auto samples = bank.samples();
        samples[0].hasEntry = true;
        samples[0].alias = QStringLiteral("a");
        bank.setSamples(samples);
        auto directory = bank.directories().at(0);
        directory.charset = QStringLiteral("UTF-8");
        bank.setDirectory(0, directory);

        const QByteArray theirs = "a.wav=theirs,1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), theirs);

        DiagnosticList diagnostics;
        QVERIFY(!disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), theirs);
    }

    // A directory that was never read has no data from which to write its files.
    void a_directory_never_read_is_never_written() {
        const QByteArray oto = "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QVERIFY(bank.directories().at(0).leftOut);

        // If unmodified, the save succeeds and writes nothing.
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);

        auto samples = bank.samples();
        samples[0].hasEntry = true;
        bank.setSamples(samples);
        diagnostics.clear();
        QVERIFY(!disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
    }

    // Invalid text was read as empty, and writing the file would replace the original with
    // empty text.
    void text_that_did_not_decode_is_not_written_back() {
        const QByteArray oto = "a.wav=" + kGbkGePing +
                               ",1,2,3,4,5\r\n"
                               "b.wav=b,1,2,3,4,5\r\n";
        write(QStringLiteral("oto.ini"), oto);
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QVERIFY(bank.directories().at(0).lossy);

        DiagnosticList diagnostics;
        QVERIFY(disk.save(bank, diagnostics));

        edit(bank, QStringLiteral("b"), [](VoiceSample &sample) { sample.offset = 100; });
        QVERIFY(!disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), oto);
    }

    // The configuration that recorded the encoding is read and kept, so a save neither mistakes
    // it for one that could not be read nor rewrites it when the encoding is unchanged.
    void a_recorded_encoding_is_kept_as_it_was() {
        VoiceBankConfig config;
        config.charset = TextCodec(QStringLiteral("GBK")).name();
        const QByteArray recorded = config.toJson();
        write(QStringLiteral("hello-config.json"), recorded);
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        edit(opened->bank, QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3"),
             [](VoiceSample &sample) { sample.offset = 7; });

        // Read-only, so that a save that wrote it would fail.
        QVERIFY(QFile::setPermissions(pathOf(QStringLiteral("hello-config.json")),
                                      QFileDevice::ReadOwner | QFileDevice::ReadUser));
        const bool saved = opened->disk.save(opened->bank, diagnostics);
        QVERIFY(QFile::setPermissions(pathOf(QStringLiteral("hello-config.json")),
                                      QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        QVERIFY(saved);
        QCOMPARE(read(QStringLiteral("oto.ini")), "a.wav=" + kGbkGePing + ",7,2,3,4,5\r\n");
        QCOMPARE(read(QStringLiteral("hello-config.json")), recorded);
    }

    // The configuration belongs to HelloUtau. One that another program modified or removed, or
    // that could not be read, is written again by the next save, although nothing else changed,
    // and without the refusal that protects the other files.
    void a_configuration_changed_elsewhere_is_written_again() {
        VoiceBankConfig config;
        config.charset = TextCodec(QStringLiteral("GBK")).name();
        config.unknownFields.insert(QStringLiteral("kept"), 1);
        const QByteArray recorded = config.toJson();
        write(QStringLiteral("hello-config.json"), recorded);
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), nullptr, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        write(QStringLiteral("hello-config.json"),
              R"({"$format":"hello-voicebank","charset":"Shift_JIS","kept":1})");
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("hello-config.json")), recorded);

        // A field added elsewhere is dropped as well.
        write(QStringLiteral("hello-config.json"),
              R"({"$format":"hello-voicebank","charset":"GBK","kept":1,"added":2})");
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("hello-config.json")), recorded);

        QVERIFY(QFile::remove(pathOf(QStringLiteral("hello-config.json"))));
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("hello-config.json")), recorded);
        QCOMPARE(read(QStringLiteral("oto.ini")), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
    }

    // A configuration that another program created after the voice bank was read would decode
    // the directory in its encoding at the next open, so a save replaces it as well.
    void a_configuration_created_elsewhere_is_replaced() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());

        write(QStringLiteral("Hello-Config.json"),
              R"({"$format":"hello-voicebank","charset":"Shift_JIS"})");
        DiagnosticList diagnostics;
        QVERIFY(opened->disk.save(opened->bank, diagnostics));
        DiagnosticList ignored;
        const auto config = VoiceBankConfig::open(root() / "Hello-Config.json", ignored);
        QVERIFY(config.has_value());
        QCOMPARE(config->charset, TextCodec(QStringLiteral("GBK")).name());
        QCOMPARE(read(QStringLiteral("oto.ini")), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
    }

    // A configuration that could not be read is taken as none, so the encoding is selected
    // again, and replaced by the next save.
    void a_configuration_that_did_not_read_is_replaced() {
        write(QStringLiteral("hello-config.json"), "not json");
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        DiagnosticList diagnostics;
        QVERIFY(opened->disk.save(opened->bank, diagnostics));
        DiagnosticList ignored;
        const auto config = VoiceBankConfig::open(root() / "hello-config.json", ignored);
        QVERIFY(config.has_value());
        QCOMPARE(config->charset, TextCodec(QStringLiteral("GBK")).name());
    }

    // Only a configuration that cannot be written at all fails the save, before anything is
    // written: a folder of its name, or a file that does not open for writing.
    void a_configuration_that_cannot_be_written_fails_the_save() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        QVERIFY(QDir().mkpath(pathOf(QStringLiteral("hello-config.json"))));

        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        edit(opened->bank, QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3"),
             [](VoiceSample &sample) { sample.offset = 7; });
        DiagnosticList diagnostics;
        QVERIFY(!opened->disk.save(opened->bank, diagnostics));
        QVERIFY(diagnostics.last().message.contains(QStringLiteral("hello-config.json")));
        QCOMPARE(read(QStringLiteral("oto.ini")), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");

        QVERIFY(QDir(pathOf(QStringLiteral("hello-config.json"))).removeRecursively());
        write(QStringLiteral("hello-config.json"), R"({"charset":"GBK"})");
        opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        edit(opened->bank, QString::fromUtf8("\xe8\x91\x9b\xe5\xb9\xb3"),
             [](VoiceSample &sample) { sample.offset = 7; });
        write(QStringLiteral("hello-config.json"), R"({"charset":"Shift_JIS"})");
        QVERIFY(QFile::setPermissions(pathOf(QStringLiteral("hello-config.json")),
                                      QFileDevice::ReadOwner | QFileDevice::ReadUser));
        diagnostics.clear();
        const bool saved = opened->disk.save(opened->bank, diagnostics);
        QVERIFY(QFile::setPermissions(pathOf(QStringLiteral("hello-config.json")),
                                      QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        QVERIFY(!saved);
        QCOMPARE(read(QStringLiteral("oto.ini")), "a.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
    }

    // The contents and the disk state are paired by directory path. A directory of the contents
    // that the disk state has not read is saved as a new one, but a file already in it has no
    // record of its state, so replacing it could overwrite changes made elsewhere, and nothing is
    // saved.
    void a_file_in_a_directory_the_disk_state_has_not_read_is_not_replaced() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());

        write(QStringLiteral("sub/oto.ini"), "b.wav=b,1,2,3,4,5\r\n");
        write(QStringLiteral("sub/b.wav"), "RIFF");
        auto later = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(later.has_value());
        edit(later->bank, QStringLiteral("a"), [](VoiceSample &sample) { sample.offset = 7; });

        DiagnosticList diagnostics;
        QVERIFY(!opened->disk.save(later->bank, diagnostics));
        QCOMPARE(diagnostics.size(), 1);
        QVERIFY(diagnostics.first().message.contains(QStringLiteral("sub")));
        QCOMPARE(read(QStringLiteral("oto.ini")), QByteArray("a.wav=a,1,2,3,4,5\r\n"));
        QVERIFY(!exists(QStringLiteral("sub/hello-config.json")));
    }

    // A directory that a reload removed and that the contents still hold, as after an undo in
    // an editing session, is created again with its files. Saving it again finds it read.
    void a_directory_the_disk_state_has_not_read_is_created() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("sub/oto.ini"), "b.wav=b,1,2,3,4,5\r\n");
        write(QStringLiteral("sub/b.wav"), "RIFF");
        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &disk = opened->disk;
        const auto before = opened->bank;

        QVERIFY(QDir(pathOf(QStringLiteral("sub"))).removeRecursively());
        DiagnosticList diagnostics;
        auto reloaded = opened->bank;
        disk.reloadFromDisk(reloaded, disk.checkDisk(), nullptr, diagnostics);
        QCOMPARE(reloaded.indexOf("sub"), -1);

        QVERIFY(disk.save(before, diagnostics));
        QCOMPARE(read(QStringLiteral("sub/oto.ini")),
                 QByteArray("#Charset:UTF-8\r\nb.wav=b,1,2,3,4,5\r\n"));
        QVERIFY(exists(QStringLiteral("sub/hello-config.json")));
        QCOMPARE(read(QStringLiteral("oto.ini")), QByteArray("a.wav=a,1,2,3,4,5\r\n"));
        QVERIFY(disk.checkDisk().isEmpty());
        QVERIFY(!disk.isModified(before, "sub"));
        QVERIFY(disk.save(before, diagnostics));
    }

    // Without its root, the voice bank on disk is gone, and saving writes every text file of the
    // contents again, as a new voice bank.
    void a_removed_root_is_written_again_as_a_whole() {
        write(QStringLiteral("oto.ini"), "a.wav=a,41.0,2,3,4,5\r\n");
        write(QStringLiteral("character.txt"), "name=n\r\n");
        write(QStringLiteral("sub/oto.ini"), "b.wav=b,1,2,3,4,5\r\n");
        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &disk = opened->disk;

        QVERIFY(QDir(m_dir->path()).removeRecursively());
        QVERIFY(disk.checkDisk().rootNotFound);
        DiagnosticList diagnostics;
        QVERIFY(disk.save(opened->bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")), QByteArray("a.wav=a,41.0,2,3,4,5\r\n"));
        QCOMPARE(read(QStringLiteral("character.txt")), QByteArray("name=n\r\n"));
        QCOMPARE(read(QStringLiteral("sub/oto.ini")), QByteArray("b.wav=b,1,2,3,4,5\r\n"));
        QVERIFY(exists(QStringLiteral("sub/hello-config.json")));
        QVERIFY(disk.checkDisk().isEmpty());
        QVERIFY(!disk.isModified(opened->bank, {}));
    }

    // The state of the files read before the root was removed describes nothing on disk, so a
    // directory that the saved contents do not hold is not reported as removed afterward.
    void the_state_before_the_root_was_removed_is_dropped() {
        write(QStringLiteral("oto.ini"), "a.wav=a,41.0,2,3,4,5\r\n");
        write(QStringLiteral("sub/oto.ini"), "b.wav=b,1,2,3,4,5\r\n");
        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        auto &disk = opened->disk;
        const auto &bank = opened->bank;
        const auto index = bank.indexOf({});
        QList<VoiceSample> samples;
        for (auto sample : bank.samples()) {
            if (sample.directory == index) {
                sample.directory = 0;
                samples.push_back(sample);
            }
        }
        const VoiceBank rootOnly(bank.root(), {bank.directories().at(index)}, samples);

        QVERIFY(QDir(m_dir->path()).removeRecursively());
        DiagnosticList diagnostics;
        QVERIFY(disk.save(rootOnly, diagnostics));
        QVERIFY(!exists(QStringLiteral("sub")));
        QVERIFY(disk.checkDisk().isEmpty());
    }

    // Saving as writes the text files of the contents into a new folder and copies every other
    // file, including the directories that the contents do not hold, as they are. The original
    // folder is unchanged.
    void saving_as_copies_the_other_files() {
        write(QStringLiteral("oto.ini"), "a.wav=a,41.0,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("Xia.bmp"), "BM");
        write(QStringLiteral("sub/oto.ini"), "b.wav=b,1,2,3,4,5\r\n");
        write(QStringLiteral("sub/b.wav"), "RIFF");
        write(QStringLiteral("left/oto.ini"), "z.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto bank = opened->bank;
        edit(bank, QStringLiteral("a"), [](VoiceSample &sample) { sample.offset = 7; });
        QVERIFY(bank.directories().at(bank.indexOf("left")).lossy);

        // The contents without the directory that did not decode, as a session holds them.
        DiagnosticList diagnostics;
        QList<VoiceBankDirectory> directories;
        QList<VoiceSample> samples;
        for (const auto &path : {fs::path(), fs::path("sub")}) {
            const auto index = bank.indexOf(path);
            for (auto sample : bank.samples()) {
                if (sample.directory == index) {
                    sample.directory = int(directories.size());
                    samples.push_back(sample);
                }
            }
            directories.push_back(bank.directories().at(index));
        }
        const VoiceBank contents(bank.root(), directories, samples);

        QTemporaryDir target;
        const auto folder = fs::path(target.path().toStdU16String()) / "copy";
        diagnostics.clear();
        const auto saved = VoiceBankDiskState::saveAs(contents, folder, true, diagnostics);
        QVERIFY(saved.has_value());
        const auto copy = [&folder](const char *relative) {
            QFile file(QString::fromStdU16String((folder / relative).u16string()));
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
        };
        QCOMPARE(copy("oto.ini"), QByteArray("#Charset:UTF-8\r\na.wav=a,7,2,3,4,5\r\n"));
        QCOMPARE(copy("a.wav"), QByteArray("RIFF"));
        QCOMPARE(copy("Xia.bmp"), QByteArray("BM"));
        QCOMPARE(copy("sub/b.wav"), QByteArray("RIFF"));
        QCOMPARE(copy("left/oto.ini"), "z.wav=" + kGbkGePing + ",1,2,3,4,5\r\n");
        QVERIFY(copy("hello-config.json").contains("UTF-8"));

        // Read from the new folder, which is unmodified, and without the directory left out.
        QCOMPARE(saved->disk.root(), folder);
        QVERIFY(!saved->disk.isModified(saved->bank, {}));
        QVERIFY(saved->bank.directories().at(saved->bank.indexOf("left")).leftOut);
        QVERIFY(saved->bank.find(60, QStringLiteral("a")));
        QCOMPARE(saved->bank.find(60, QStringLiteral("a"))->offset, 7.0);

        QCOMPARE(read(QStringLiteral("oto.ini")), QByteArray("a.wav=a,41.0,2,3,4,5\r\n"));
        QVERIFY(!exists(QStringLiteral("hello-config.json")));
    }

    // Saving only the text files leaves every other file behind.
    void saving_as_text_writes_the_text_files_only() {
        write(QStringLiteral("oto.ini"), "a.wav=a,41.0,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");
        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());

        QTemporaryDir target;
        const auto folder = fs::path(target.path().toStdU16String());
        DiagnosticList diagnostics;
        const auto saved = VoiceBankDiskState::saveAs(opened->bank, folder, false, diagnostics);
        QVERIFY(saved.has_value());
        QVERIFY(QFileInfo::exists(target.filePath(QStringLiteral("oto.ini"))));
        QVERIFY(!QFileInfo::exists(target.filePath(QStringLiteral("a.wav"))));
        QVERIFY(saved->bank.find(60, QStringLiteral("a"))->hasEntry);
    }

    // A voice bank is not saved into a folder that holds something, nor over a file.
    void saving_as_refuses_a_folder_that_is_not_empty() {
        write(QStringLiteral("oto.ini"), "a.wav=a,41.0,2,3,4,5\r\n");
        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());

        QTemporaryDir target;
        QFile other(target.filePath(QStringLiteral("other.txt")));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.close();
        DiagnosticList diagnostics;
        QVERIFY(!VoiceBankDiskState::saveAs(opened->bank, fs::path(target.path().toStdU16String()),
                                            true, diagnostics));
        QVERIFY(diagnostics.first().message.contains(QStringLiteral("not an empty folder")));
        QVERIFY(!QFileInfo::exists(target.filePath(QStringLiteral("oto.ini"))));

        diagnostics.clear();
        QVERIFY(!VoiceBankDiskState::saveAs(
            opened->bank, fs::path(target.filePath(QStringLiteral("other.txt")).toStdU16String()),
            true, diagnostics));
        QVERIFY(diagnostics.first().message.contains(QStringLiteral("not an empty folder")));
    }

    // Without the original folder, the other files are gone, and only the text files are saved.
    void saving_as_without_the_original_folder_saves_the_text_files() {
        write(QStringLiteral("oto.ini"), "a.wav=a,41.0,2,3,4,5\r\n");
        auto opened = open(root(), QStringLiteral("GBK"));
        QVERIFY(opened.has_value());
        QVERIFY(QDir(m_dir->path()).removeRecursively());

        QTemporaryDir target;
        DiagnosticList diagnostics;
        const auto saved = VoiceBankDiskState::saveAs(
            opened->bank, fs::path(target.path().toStdU16String()), true, diagnostics);
        QVERIFY(saved.has_value());
        QVERIFY(QFileInfo::exists(target.filePath(QStringLiteral("oto.ini"))));
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.first().severity, DiagnosticSeverity::Warning);
    }

    // A directory is not created where a file of its name is.
    void a_directory_is_not_created_over_a_file() {
        write(QStringLiteral("sub/oto.ini"), "b.wav=b,1,2,3,4,5\r\n");
        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &disk = opened->disk;
        const auto before = opened->bank;

        QVERIFY(QDir(pathOf(QStringLiteral("sub"))).removeRecursively());
        DiagnosticList diagnostics;
        auto reloaded = opened->bank;
        disk.reloadFromDisk(reloaded, disk.checkDisk(), nullptr, diagnostics);
        write(QStringLiteral("sub"), "a file");

        diagnostics.clear();
        QVERIFY(!disk.save(before, diagnostics));
        QVERIFY(diagnostics.first().message.contains(QStringLiteral("is a file")));
        QCOMPARE(read(QStringLiteral("sub")), QByteArray("a file"));
    }

    // The output of a save becomes the baseline for the next one. Otherwise the second save
    // would mistake its own output for an external change.
    void a_bank_can_be_saved_again() {
        write(QStringLiteral("oto.ini"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;

        DiagnosticList diagnostics;
        edit(bank, QStringLiteral("a"), [](VoiceSample &sample) { sample.offset = 10; });
        QVERIFY(disk.save(bank, diagnostics));
        edit(bank, QStringLiteral("a"), [](VoiceSample &sample) { sample.offset = 20; });
        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 QByteArray("#Charset:UTF-8\r\na.wav=a,20,2,3,4,5\r\n"));
    }

    // On a case-sensitive file system, oto.ini would be a second file beside the one UTAU
    // reads.
    void a_file_is_written_back_under_the_name_it_was_found_by() {
        write(QStringLiteral("OTO.INI"), "a.wav=a,1,2,3,4,5\r\n");
        write(QStringLiteral("a.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        edit(bank, QStringLiteral("a"), [](VoiceSample &sample) { sample.offset = 10; });

        DiagnosticList diagnostics;
        QVERIFY(disk.save(bank, diagnostics));

        const auto names = QDir(m_dir->path()).entryList({QStringLiteral("*.ini")}, QDir::Files);
        QCOMPARE(names, QStringList{QStringLiteral("OTO.INI")});
        QCOMPARE(read(QStringLiteral("OTO.INI")),
                 QByteArray("#Charset:UTF-8\r\na.wav=a,10,2,3,4,5\r\n"));
    }

    // A missing oto.ini and an empty one both mean no entries, as in UTAU, and each stays as it
    // is when another file of the directory is saved. A missing one is created only for an entry.
    void a_missing_oto_ini_differs_from_an_empty_one_on_disk_only() {
        write(QStringLiteral("character.txt"), "name=a\r\n");
        write(QStringLiteral("sub/oto.ini"), "");
        write(QStringLiteral("sub/x.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        for (const auto &sample : bank.samples()) {
            QVERIFY(!sample.hasEntry);
        }

        auto directory = bank.directories().at(0);
        directory.character->name = QStringLiteral("b");
        bank.setDirectory(0, directory);
        DiagnosticList diagnostics;
        QVERIFY(disk.save(bank, diagnostics));
        QVERIFY(!exists(QStringLiteral("oto.ini")));
        QCOMPARE(read(QStringLiteral("sub/oto.ini")), QByteArray());
    }

    // A directory of bare files has no encoding, and its first oto.ini requires one. The user
    // is asked rather than the encoding guessed, as when reading.
    void a_first_oto_ini_needs_an_encoding() {
        write(QStringLiteral("ka.wav"), "RIFF");

        auto opened = open(root(), QStringLiteral("UTF-8"));
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        QVERIFY(bank.directories().at(0).charset.isEmpty());

        auto samples = bank.samples();
        samples[0].hasEntry = true;
        samples[0].alias = QStringLiteral("ka");
        samples[0].offset = 5;
        bank.setSamples(samples);

        DiagnosticList diagnostics;
        QVERIFY(!disk.save(bank, diagnostics));
        QVERIFY(!exists(QStringLiteral("oto.ini")));

        auto directory = bank.directories().at(0);
        directory.charset = QStringLiteral("UTF-8");
        bank.setDirectory(0, directory);
        QVERIFY(disk.save(bank, diagnostics));
        // Setting UTF-8 declares it in the oto.ini, as converting to it does.
        QCOMPARE(read(QStringLiteral("oto.ini")),
                 QByteArray("#Charset:UTF-8\r\nka.wav=ka,5,0,0,0,0\r\n"));
        QVERIFY(exists(QStringLiteral("hello-config.json")));
    }

    // Editing one directory writes only that directory. The others remain untouched and
    // receive no configuration file.
    void only_the_directory_that_changed_is_written() {
        const QByteArray jp = "a.wav=" + kShiftJisA + ",1,2,3,4,5\r\n";
        write(QStringLiteral("jp/oto.ini"), jp);
        write(QStringLiteral("jp/a.wav"), "RIFF");
        write(QStringLiteral("en/oto.ini"), "b.wav=b,1,2,3,4,5\r\n");
        write(QStringLiteral("en/b.wav"), "RIFF");

        FixedCharsetSelector selector(QStringLiteral("Shift_JIS"));
        DiagnosticList diagnostics;
        auto opened = VoiceBankDiskState::open(root(), &selector, diagnostics);
        QVERIFY(opened.has_value());
        auto &bank = opened->bank;
        auto &disk = opened->disk;
        edit(bank, QStringLiteral("b"), [](VoiceSample &sample) { sample.offset = 9; });

        QVERIFY(disk.save(bank, diagnostics));
        QCOMPARE(read(QStringLiteral("jp/oto.ini")), jp);
        QVERIFY(!exists(QStringLiteral("jp/hello-config.json")));
        QCOMPARE(read(QStringLiteral("en/oto.ini")), QByteArray("b.wav=b,9,2,3,4,5\r\n"));
        QVERIFY(exists(QStringLiteral("en/hello-config.json")));
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankDiskState_Save)

#include "test_VoiceBankDiskState_Save.moc"
