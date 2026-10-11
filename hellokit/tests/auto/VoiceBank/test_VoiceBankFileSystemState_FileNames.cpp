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

using Source = VoiceBankDirectorySource;

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

    static std::optional<VoiceBankConfig::FileNameEncoding>
        recordOf(const VoiceBankFileSystemState::Opened &opened, const fs::path &directory) {
        const int index = opened.bank.indexOf(directory);
        return index >= 0 ? opened.bank.directories().at(index).fileNameEncoding : std::nullopt;
    }

    static QStringList audioFilesOf(const VoiceBankFileSystemState::Opened &opened,
                                    const fs::path &directory = {}) {
        return opened.files.audioFiles(directory, recordOf(opened, directory));
    }

    static fs::path audioPathOf(const VoiceBankFileSystemState::Opened &opened,
                                const fs::path &directory, const QString &fileName) {
        return opened.files.audioPathOf(directory, recordOf(opened, directory), fileName);
    }

    static QStringList undeterminedOf(const VoiceBankFileSystemState::Opened &opened,
                                      const fs::path &directory = {}) {
        return opened.files.undeterminedNames(directory, recordOf(opened, directory));
    }

    /// Replaces the record of the directory at \a directory of \a opened with \a record .
    static void setRecord(VoiceBankFileSystemState::Opened &opened, const fs::path &directory,
                          const std::optional<VoiceBankConfig::FileNameEncoding> &record) {
        const int index = opened.bank.indexOf(directory);
        QVERIFY(index >= 0);
        auto changed = opened.bank.directories().at(index);
        changed.fileNameEncoding = record;
        opened.bank.setDirectory(index, changed);
    }

    std::optional<VoiceBankConfig> configAt(const QString &relative) const {
        DiagnosticList ignored;
        return VoiceBankConfig::open(root() / pathOf(relative) / "hello-config.json", ignored);
    }

    static const VoiceSample *sampleNamed(const VoiceBank &bank, const QString &fileName) {
        for (const auto &sample : bank.samples()) {
            if (sample.fileName == fileName) {
                return &sample;
            }
        }
        return nullptr;
    }

    /// \a kinds of the directory at \a directory , the files that a save is told to write.
    static VoiceBankFileSystemState::Files filesOf(const fs::path &directory,
                                                   std::set<VoiceBankDirectorySource::File> kinds) {
        return {
            {directory, std::move(kinds)}
        };
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
        QVERIFY(!audioFilesOf(*opened).contains(kMisdecodedA + QStringLiteral(".wav")));
    }

    void a_misdecoded_file_without_an_entry_is_listed_under_its_restored_name() {
        writeBank();
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        const auto restored = QString::fromUtf8("い.wav");
        const auto physical = root() / pathOf(kMisdecodedI + QStringLiteral(".wav"));
        QVERIFY(audioFilesOf(*opened).contains(restored));
        QCOMPARE(audioPathOf(*opened, {}, restored), physical);
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
        const auto files = audioFilesOf(*opened);
        QVERIFY(files.contains(QString::fromUtf8("が.wav")));
        QVERIFY(!files.contains(QString::fromUtf8("､ｬ.wav")));
    }

    void an_unlisted_name_is_undetermined_and_used_as_it_is() {
        writeBank();
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        QCOMPARE(undeterminedOf(*opened),
                 (QStringList{QString::fromUtf8("新.wav"), QString::fromUtf8("未")}));
        QVERIFY(audioFilesOf(*opened).contains(QString::fromUtf8("新.wav")));
        QCOMPARE(opened->bank.logicalPathOf(pathOf(QString::fromUtf8("未"))),
                 pathOf(QString::fromUtf8("未")));
        // The subdirectories have no record.
        QVERIFY(undeterminedOf(*opened, pathOf(kMisdecodedU)).isEmpty());
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
        QVERIFY(undeterminedOf(*opened).isEmpty());
    }

    void a_subdirectory_is_named_by_the_record_of_its_parent() {
        writeBank();
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        QCOMPARE(opened->bank.logicalPathOf(pathOf(kMisdecodedU)), pathOf(QString::fromUtf8("う")));
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
        QCOMPARE(audioFilesOf(*opened), QStringList{kMisdecodedI + QStringLiteral(".wav")});
        QVERIFY(undeterminedOf(*opened).isEmpty());
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
        const auto files = audioFilesOf(*opened);
        QCOMPARE(files.size(), 2);
        QVERIFY(files.contains(kMisdecodedA + QStringLiteral(".wav")));
        QVERIFY(files.contains(QString::fromUtf8("あ.wav")));
        QCOMPARE(audioPathOf(*opened, {}, QString::fromUtf8("あ.wav")),
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
        QCOMPARE(audioFilesOf(*opened), QStringList{QString::fromUtf8("你.wav")});
        QVERIFY(!diagnostics.isEmpty());
        QVERIFY(!hasError(diagnostics));
    }

    // The record is part of the contents, which a reread keeps as it keeps the encoding.
    void rereading_keeps_the_record_of_the_contents() {
        writeBank();
        DiagnosticList diagnostics;
        auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        writeConfig(QString(), japaneseOnChinese());
        QVERIFY(opened->files.reread(opened->bank, {}, QStringLiteral("Shift_JIS"), diagnostics));
        QVERIFY(audioFilesOf(*opened).contains(QString::fromUtf8("い.wav")));
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
        const auto files = audioFilesOf(*opened);
        QVERIFY(files.contains(QStringLiteral("d.wav")));
        QVERIFY(files.contains(QString::fromUtf8("い.wav")));
        const auto sample = sampleNamed(opened->bank, QString::fromUtf8("い.wav"));
        QVERIFY(sample);
        QCOMPARE(sample->path, root() / pathOf(kMisdecodedI + QStringLiteral(".wav")));
    }

    void the_names_follow_the_record_given() {
        writeBank();
        DiagnosticList diagnostics;
        const auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        QVERIFY(audioFilesOf(*opened).contains(QString::fromUtf8("い.wav")));
        const auto physical = opened->files.audioFiles({}, std::nullopt);
        QVERIFY(physical.contains(kMisdecodedI + QStringLiteral(".wav")));
        QVERIFY(!physical.contains(QString::fromUtf8("い.wav")));
        QVERIFY(opened->files.undeterminedNames({}, std::nullopt).isEmpty());
    }

    void a_changed_record_is_saved() {
        writeBank();
        DiagnosticList diagnostics;
        auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());

        auto record = *recordOf(*opened, {});
        record.correct.push_back(QString::fromUtf8("新.wav"));
        setRecord(*opened, {}, record);
        QVERIFY(opened->files.save(opened->bank, filesOf({}, {Source::Config}), diagnostics));
        const auto config = configAt(QString());
        QVERIFY(config.has_value());
        QCOMPARE(config->fileNameEncoding, std::optional(record));
        QCOMPARE(config->charset, TextCodec(QStringLiteral("Shift_JIS")).name());
    }

    void a_removed_record_is_removed_from_the_configuration() {
        writeBank();
        DiagnosticList diagnostics;
        auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        setRecord(*opened, {}, std::nullopt);
        QVERIFY(opened->files.save(opened->bank, filesOf({}, {Source::Config}), diagnostics));
        const auto config = configAt(QString());
        QVERIFY(config.has_value());
        QVERIFY(!config->fileNameEncoding.has_value());
    }

    // A directory of audio files alone has no encoding, and its configuration names none.
    void a_directory_without_text_files_saves_its_record() {
        writeBank();
        DiagnosticList diagnostics;
        auto opened = open(root(), diagnostics);
        QVERIFY(opened.has_value());
        auto record = japaneseOnChinese();
        record.correct = QStringList{QStringLiteral("b.wav")};
        setRecord(*opened, pathOf(kMisdecodedU), record);
        QVERIFY(opened->files.save(opened->bank, filesOf(pathOf(kMisdecodedU), {Source::Config}),
                                   diagnostics));
        const auto config = configAt(kMisdecodedU);
        QVERIFY(config.has_value());
        QCOMPARE(config->fileNameEncoding, std::optional(record));

        const auto again = open(root(), diagnostics);
        QVERIFY(again.has_value());
        QCOMPARE(recordOf(*again, pathOf(kMisdecodedU)), std::optional(record));
        QVERIFY(again->bank.directories()
                    .at(again->bank.indexOf(pathOf(kMisdecodedU)))
                    .charset.isEmpty());
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankFileSystemState_FileNames)

#include "test_VoiceBankFileSystemState_FileNames.moc"
