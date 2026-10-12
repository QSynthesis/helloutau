#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Support/TextCodec.h>
#include <hellokit/VoiceBank/MisdecodedNameQuestion.h>
#include <hellokit/VoiceBank/VoiceBankSource.h>

using namespace hello::kit;

namespace fs = std::filesystem;

// あ, い and う in Shift_JIS read as GBK, the names of a Japanese voice bank extracted on a
// Chinese system.
static const QString kMisdecodedA = QString::fromUtf8("偁");
static const QString kMisdecodedI = QString::fromUtf8("偄");
static const QString kMisdecodedU = QString::fromUtf8("偆");

class test_MisdecodedNameQuestion : public QObject {
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

    /// Writes an oto.ini in Shift_JIS with an entry for each of \a fileNames .
    void writeOto(const QStringList &fileNames) {
        QString text;
        for (const auto &fileName : fileNames) {
            text += fileName + QStringLiteral("=,1,2,3,4,5\r\n");
        }
        write(QStringLiteral("oto.ini"), TextCodec(QStringLiteral("Shift_JIS")).encode(text));
    }

    void writeRecord(const VoiceBankConfig::FileNameEncoding &record) {
        VoiceBankConfig config;
        config.charset = QStringLiteral("Shift_JIS");
        config.fileNameEncoding = record;
        write(QStringLiteral("hello-config.json"), config.toJson());
    }

    /// The record of Shift_JIS read as GBK, with lists if \a files .
    static VoiceBankConfig::FileNameEncoding
        japaneseOnChinese(std::optional<VoiceBankConfig::FileNameEncoding::Files> files = {}) {
        VoiceBankConfig::FileNameEncoding record;
        record.original = QStringLiteral("Shift_JIS");
        record.misdecodedAs = QStringLiteral("GBK");
        record.files = files;
        return record;
    }

    void writeRecord(const QStringList &misdecoded, const QStringList &correct) {
        writeRecord(
            japaneseOnChinese(VoiceBankConfig::FileNameEncoding::Files{misdecoded, correct}));
    }

    VoiceBankDirectorySource source() const {
        DiagnosticList diagnostics;
        const auto read = VoiceBankSource::readDirectory(root(), {}, diagnostics);
        return read.value_or(VoiceBankDirectorySource());
    }

    static QStringList namesOf(const MisdecodedNameQuestion &question) {
        QStringList names;
        for (const auto &choice : question.choices()) {
            names.push_back(choice.name);
        }
        return names;
    }

    static const MisdecodedNameQuestion::Choice *choiceNamed(const MisdecodedNameQuestion &question,
                                                             const QString &name) {
        for (const auto &choice : question.choices()) {
            if (choice.name == name) {
                return &choice;
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

    // The entries name あ.wav and い.wav, which the disk holds as 偁.wav and 偄.wav. Only
    // Shift_JIS read as GBK restores them.
    void a_directory_without_a_record_is_inferred_from_its_entries() {
        writeOto({QString::fromUtf8("あ.wav"), QString::fromUtf8("い.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        write(kMisdecodedI + QStringLiteral(".wav"), "RIFF");
        write(QStringLiteral("a.wav"), "RIFF");

        const auto question = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(question.has_value());
        QVERIFY(!question->isRecorded());
        QCOMPARE(question->original(), TextCodec(QStringLiteral("Shift_JIS")).name());
        QCOMPARE(question->misdecodedAs(), TextCodec(QStringLiteral("GBK")).name());
        QCOMPARE(question->missingEntries(), 2);
        QCOMPARE(question->matchedEntries(), 2);
        // ASCII names are never asked about.
        QCOMPARE(namesOf(*question), (QStringList{kMisdecodedA + QStringLiteral(".wav"),
                                                  kMisdecodedI + QStringLiteral(".wav")}));
        const auto a = choiceNamed(*question, kMisdecodedA + QStringLiteral(".wav"));
        QVERIFY(a);
        QCOMPARE(a->restored, QString::fromUtf8("あ.wav"));
        QVERIFY(a->misdecoded);
        QVERIFY(!a->directory);
    }

    // が.wav restores to ､ｬ.wav, but an entry names it as it is.
    void a_name_that_an_entry_names_is_not_proposed() {
        writeOto({QString::fromUtf8("あ.wav"), QString::fromUtf8("が.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        write(QString::fromUtf8("が.wav"), "RIFF");

        const auto question = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(question.has_value());
        const auto ga = choiceNamed(*question, QString::fromUtf8("が.wav"));
        QVERIFY(ga);
        QVERIFY(!ga->misdecoded);
        QVERIFY(choiceNamed(*question, kMisdecodedA + QStringLiteral(".wav"))->misdecoded);
    }

    void every_entry_finding_its_file_asks_nothing() {
        writeOto({QString::fromUtf8("が.wav")});
        write(QString::fromUtf8("が.wav"), "RIFF");
        QVERIFY(!MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS")).has_value());
    }

    void an_entry_that_no_name_restores_to_asks_nothing() {
        writeOto({QString::fromUtf8("う.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        QVERIFY(!MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS")).has_value());
    }

    // The entries are decoded in the encoding of the text files, which therefore determines the
    // result of the inference.
    void the_entries_are_read_in_the_encoding_of_the_text_files() {
        writeOto({QString::fromUtf8("あ.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        QVERIFY(MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS")).has_value());
        QVERIFY(!MisdecodedNameQuestion::of(source(), QStringLiteral("UTF-8")).has_value());
    }

    void a_subdirectory_is_asked_about() {
        writeOto({QString::fromUtf8("あ.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        write(kMisdecodedU + QStringLiteral("/b.wav"), "RIFF");

        const auto question = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(question.has_value());
        const auto u = choiceNamed(*question, kMisdecodedU);
        QVERIFY(u);
        QVERIFY(u->directory);
        QCOMPARE(u->restored, QString::fromUtf8("う"));
    }

    // A directory with a record takes its encodings, and is asked about its new names only.
    void a_recorded_directory_is_asked_about_its_new_names() {
        writeRecord({kMisdecodedA + QStringLiteral(".wav")}, {});
        writeOto({QString::fromUtf8("あ.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        write(kMisdecodedI + QStringLiteral(".wav"), "RIFF");

        const auto question = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(question.has_value());
        QVERIFY(question->isRecorded());
        QCOMPARE(namesOf(*question), QStringList{kMisdecodedI + QStringLiteral(".wav")});

        // Every name misdecoded, which the record without lists records.
        QCOMPARE(question->answer({kMisdecodedI + QStringLiteral(".wav")}), japaneseOnChinese());
    }

    void a_record_of_every_name_asks_nothing() {
        writeRecord({kMisdecodedA + QStringLiteral(".wav")}, {QString::fromUtf8("が.wav")});
        writeOto({QString::fromUtf8("あ.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        write(QString::fromUtf8("が.wav"), "RIFF");
        QVERIFY(!MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS")).has_value());
    }

    // The user has decided, and new names follow the decision.
    void a_record_without_lists_asks_nothing() {
        writeOto({QString::fromUtf8("あ.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        write(kMisdecodedI + QStringLiteral(".wav"), "RIFF");
        writeRecord(japaneseOnChinese());
        QVERIFY(!MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS")).has_value());
        writeRecord(VoiceBankConfig::FileNameEncoding());
        QVERIFY(!MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS")).has_value());
    }

    // A new name that does not restore cannot be misdecoded, so it is not a choice, and an answer
    // records it as correct. Alone, it raises no question. 你 in GBK is an invalid byte sequence
    // in Shift_JIS.
    void a_name_that_does_not_restore_is_answered_as_correct() {
        writeRecord({kMisdecodedA + QStringLiteral(".wav")}, {});
        writeOto({QString::fromUtf8("あ.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        write(QString::fromUtf8("你.wav"), "RIFF");
        QVERIFY(!MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS")).has_value());

        write(kMisdecodedI + QStringLiteral(".wav"), "RIFF");
        const auto question = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(question.has_value());
        QCOMPARE(namesOf(*question), QStringList{kMisdecodedI + QStringLiteral(".wav")});
        const auto record = question->answer({});
        QVERIFY(record.files.has_value());
        QCOMPARE(record.files->correct,
                 (QStringList{kMisdecodedI + QStringLiteral(".wav"), QString::fromUtf8("你.wav")}));
        QCOMPARE(record.files->misdecoded, QStringList{kMisdecodedA + QStringLiteral(".wav")});

        // A correct name that does not restore leaves the record without lists.
        QCOMPARE(question->answer({kMisdecodedI + QStringLiteral(".wav")}), japaneseOnChinese());
    }

    // Every name that is not ASCII, and no ASCII name.
    void the_answer_records_every_name() {
        writeOto({QString::fromUtf8("あ.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        write(kMisdecodedI + QStringLiteral(".wav"), "RIFF");
        write(QString::fromUtf8("你.wav"), "RIFF");
        write(QStringLiteral("a.wav"), "RIFF");

        const auto question = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(question.has_value());
        const auto record = question->answer({kMisdecodedA + QStringLiteral(".wav")});
        QCOMPARE(record.original, question->original());
        QCOMPARE(record.misdecodedAs, question->misdecodedAs());
        QVERIFY(record.files.has_value());
        QCOMPARE(record.files->misdecoded, QStringList{kMisdecodedA + QStringLiteral(".wav")});
        QCOMPARE(record.files->correct,
                 (QStringList{kMisdecodedI + QStringLiteral(".wav"), QString::fromUtf8("你.wav")}));

        // No name misdecoded, which the record without encodings records.
        QCOMPARE(question->answer({}), VoiceBankConfig::FileNameEncoding());
    }

    // The user may choose another pair, under which the names restore differently.
    void chosen_encodings_replace_the_inferred_ones() {
        writeOto({QString::fromUtf8("あ.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");

        const auto question = MisdecodedNameQuestion::of(
            source(), QStringLiteral("Shift_JIS"), QStringLiteral("Big5"), QStringLiteral("GBK"));
        QCOMPARE(question.original(), TextCodec(QStringLiteral("Big5")).name());
        QCOMPARE(question.misdecodedAs(), TextCodec(QStringLiteral("GBK")).name());
        QCOMPARE(question.missingEntries(), 1);
        QCOMPARE(question.matchedEntries(), 0);
        const auto expected =
            TextCodec(QStringLiteral("Big5"))
                .decode(
                    TextCodec(QStringLiteral("GBK")).encode(kMisdecodedA + QStringLiteral(".wav")));
        const auto a = choiceNamed(question, kMisdecodedA + QStringLiteral(".wav"));
        QCOMPARE(a != nullptr, expected.has_value());
        if (a) {
            QCOMPARE(a->restored, *expected);
        }
    }
};

QTEST_APPLESS_MAIN(test_MisdecodedNameQuestion)

#include "test_MisdecodedNameQuestion.moc"
