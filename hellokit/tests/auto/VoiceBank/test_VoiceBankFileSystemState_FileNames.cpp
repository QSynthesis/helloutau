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

namespace fs = std::filesystem;

// The names of a Japanese voice bank extracted on a Chinese system: あ, い and う in Shift_JIS read
// as GBK.
static const QString kMisdecodedA = QString::fromUtf8("偁");
static const QString kMisdecodedI = QString::fromUtf8("偄");
static const QString kMisdecodedU = QString::fromUtf8("偆");

class test_VoiceBankFileSystemState_FileNames : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    fs::path root() const {
        return fs::path(m_dir->path().toStdU16String());
    }

    static fs::path pathOf(const QString &text) {
        return fs::path(text.toStdU16String());
    }

    void write(const QString &relative, const QByteArray &bytes) {
        const QString path = m_dir->path() + QLatin1Char('/') + relative;
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    static QByteArray shiftJis(const QString &text) {
        return TextCodec(QStringLiteral("Shift_JIS")).encode(text);
    }

    static VoiceBankConfig::FileNameEncoding japaneseOnChinese() {
        VoiceBankConfig::FileNameEncoding record;
        record.original = QStringLiteral("Shift_JIS");
        record.misdecodedAs = QStringLiteral("GBK");
        return record;
    }

    void writeConfig(const QString &directory, const VoiceBankConfig::FileNameEncoding &record) {
        VoiceBankConfig config;
        config.charset = QStringLiteral("Shift_JIS");
        config.fileNameEncoding = record;
        write(directory.isEmpty() ? QStringLiteral("hello-config.json")
                                  : directory + QStringLiteral("/hello-config.json"),
              config.toJson());
    }

    /// Writes the voice bank of most tests: an entry of あ.wav on disk as 偁.wav, い.wav without
    /// an entry on disk as 偄.wav, a correct が.wav, an unlisted 新.wav, an ASCII a.wav, the
    /// subdirectory う on disk as 偆, and an unlisted subdirectory 未.
    void writeBank() {
        auto record = japaneseOnChinese();
        record.misdecoded = QStringList{kMisdecodedA + QStringLiteral(".wav"),
                                        kMisdecodedI + QStringLiteral(".wav"), kMisdecodedU};
        record.correct = QStringList{QString::fromUtf8("が.wav")};
        writeConfig(QString(), record);
        write(QStringLiteral("oto.ini"), shiftJis(QString::fromUtf8("あ.wav=あ,1,2,3,4,5\r\n")));
        for (const auto &name :
             {kMisdecodedA + QStringLiteral(".wav"), kMisdecodedI + QStringLiteral(".wav"),
              QString::fromUtf8("が.wav"), QString::fromUtf8("新.wav"), QStringLiteral("a.wav"),
              kMisdecodedU + QStringLiteral("/b.wav"), QString::fromUtf8("未/c.wav")}) {
            write(name, "RIFF");
        }
    }

    static std::optional<VoiceBankFileSystemState::Opened> open(const fs::path &root,
                                                                DiagnosticList &diagnostics) {
        FixedCharsetSelector selector(QStringLiteral("Shift_JIS"));
        return VoiceBankFileSystemState::open(root, &selector, diagnostics);
    }

    static const VoiceSample *sampleNamed(const VoiceBank &bank, const QString &fileName) {
        for (const auto &sample : bank.samples()) {
            if (sample.fileName == fileName) {
                return &sample;
            }
        }
        return nullptr;
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    void an_entry_finds_its_misdecoded_audio_file() {
        writeBank();
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        const auto sample = sampleNamed(opened->bank, QString::fromUtf8("あ.wav"));
        QVERIFY(sample);
        QVERIFY(sample->hasEntry);
        QCOMPARE(sample->path, root() / pathOf(kMisdecodedA + QStringLiteral(".wav")));
        QVERIFY(!opened->files.audioFiles({}).contains(kMisdecodedA + QStringLiteral(".wav")));
    }

    void a_misdecoded_file_without_an_entry_is_listed_under_its_restored_name() {
        writeBank();
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        const auto restored = QString::fromUtf8("い.wav");
        const auto physical = root() / pathOf(kMisdecodedI + QStringLiteral(".wav"));
        QVERIFY(opened->files.audioFiles({}).contains(restored));
        QCOMPARE(opened->files.audioPathOf({}, restored), physical);
        const auto sample = sampleNamed(opened->bank, restored);
        QVERIFY(sample);
        QVERIFY(!sample->hasEntry);
        QCOMPARE(sample->path, physical);
    }

    // が.wav would restore to ､ｬ.wav, which is why the record lists the correct names.
    void a_correct_name_is_used_as_it_is() {
        writeBank();
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        const auto files = opened->files.audioFiles({});
        QVERIFY(files.contains(QString::fromUtf8("が.wav")));
        QVERIFY(!files.contains(QString::fromUtf8("､ｬ.wav")));
    }

    void an_unlisted_name_is_undetermined_and_used_as_it_is() {
        writeBank();
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        QCOMPARE(opened->files.undeterminedNames({}),
                 (QStringList{QString::fromUtf8("新.wav"), QString::fromUtf8("未")}));
        QVERIFY(opened->files.audioFiles({}).contains(QString::fromUtf8("新.wav")));
        QCOMPARE(opened->files.logicalPathOf(pathOf(QString::fromUtf8("未"))),
                 pathOf(QString::fromUtf8("未")));
        // The subdirectories have no record.
        QVERIFY(opened->files.undeterminedNames(pathOf(kMisdecodedU)).isEmpty());
    }

    // A name on disk is looked up in the lists in Normalization Form C, as on macOS a file
    // system may store が as か followed by a combining mark.
    void a_decomposed_name_is_found_in_the_lists() {
        auto record = japaneseOnChinese();
        record.correct = QStringList{QString::fromUtf8("が.wav")};
        writeConfig(QString(), record);
        write(QString::fromUtf8("が.wav").normalized(QString::NormalizationForm_D), "RIFF");
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        QVERIFY(opened->files.undeterminedNames({}).isEmpty());
    }

    void a_subdirectory_is_named_by_the_record_of_its_parent() {
        writeBank();
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        QCOMPARE(opened->files.logicalPathOf(pathOf(kMisdecodedU)),
                 pathOf(QString::fromUtf8("う")));
        const auto sample = sampleNamed(opened->bank, QStringLiteral("b.wav"));
        QVERIFY(sample);
        QCOMPARE(sample->path, root() / pathOf(kMisdecodedU) / "b.wav");
    }

    void a_directory_without_a_record_restores_nothing() {
        write(QStringLiteral("oto.ini"), shiftJis(QString::fromUtf8("あ.wav=あ,1,2,3,4,5\r\n")));
        write(kMisdecodedI + QStringLiteral(".wav"), "RIFF");
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        QCOMPARE(opened->files.audioFiles({}), QStringList{kMisdecodedI + QStringLiteral(".wav")});
        QVERIFY(opened->files.undeterminedNames({}).isEmpty());
    }

    void a_restored_name_that_another_file_has_is_used_as_it_is() {
        auto record = japaneseOnChinese();
        record.misdecoded = QStringList{kMisdecodedA + QStringLiteral(".wav")};
        record.correct = QStringList{QString::fromUtf8("あ.wav")};
        writeConfig(QString(), record);
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        write(QString::fromUtf8("あ.wav"), "RIFF");
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        const auto files = opened->files.audioFiles({});
        QCOMPARE(files.size(), 2);
        QVERIFY(files.contains(kMisdecodedA + QStringLiteral(".wav")));
        QVERIFY(files.contains(QString::fromUtf8("あ.wav")));
        QCOMPARE(opened->files.audioPathOf({}, QString::fromUtf8("あ.wav")),
                 root() / pathOf(QString::fromUtf8("あ.wav")));
        QVERIFY(!diagnostics.isEmpty());
    }

    // 你 in GBK is an invalid byte sequence in Shift_JIS.
    void a_listed_name_that_does_not_restore_is_used_as_it_is() {
        auto record = japaneseOnChinese();
        record.misdecoded = QStringList{QString::fromUtf8("你.wav")};
        writeConfig(QString(), record);
        write(QString::fromUtf8("你.wav"), "RIFF");
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        QCOMPARE(opened->files.audioFiles({}), QStringList{QString::fromUtf8("你.wav")});
        QVERIFY(!diagnostics.isEmpty());
        QVERIFY(!hasError(diagnostics));
    }

    // A configuration modified elsewhere is not read again, see docs/Editing.md.
    void rereading_keeps_the_record_that_the_directory_was_read_with() {
        writeBank();
        DiagnosticList diagnostics;
        auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        writeConfig(QString(), japaneseOnChinese());
        QVERIFY(opened->files.reread(opened->bank, {}, QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(opened->files.audioFiles({}).contains(QString::fromUtf8("い.wav")));
        QVERIFY(sampleNamed(opened->bank, QString::fromUtf8("い.wav")));
    }

    void a_new_audio_file_keeps_the_restored_names() {
        writeBank();
        DiagnosticList diagnostics;
        auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        write(QStringLiteral("d.wav"), "RIFF");
        const auto changes = opened->files.checkDisk();
        QCOMPARE(changes.audio, QList<fs::path>{fs::path()});
        opened->files.reloadFromDisk(opened->bank, changes, nullptr, diagnostics);
        const auto files = opened->files.audioFiles({});
        QVERIFY(files.contains(QStringLiteral("d.wav")));
        QVERIFY(files.contains(QString::fromUtf8("い.wav")));
        const auto sample = sampleNamed(opened->bank, QString::fromUtf8("い.wav"));
        QVERIFY(sample);
        QCOMPARE(sample->path, root() / pathOf(kMisdecodedI + QStringLiteral(".wav")));
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankFileSystemState_FileNames)

#include "test_VoiceBankFileSystemState_FileNames.moc"
