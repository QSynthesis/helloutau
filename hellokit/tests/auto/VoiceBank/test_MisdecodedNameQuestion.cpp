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

    /// Returns whether the system encoding is GBK, which misdecodes the names in the tests
    /// without an inference, whose encodings are preset to the system encoding.
    static bool systemIsGbk() {
        return TextCodec(TextCodec::systemName()).name() == TextCodec(QStringLiteral("GBK")).name();
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
        QCOMPARE(question->basis(), MisdecodedNameQuestion::Entries);
        QCOMPARE(question->original(), TextCodec(QStringLiteral("Shift_JIS")).name());
        QCOMPARE(question->misdecodedAs(), TextCodec(QStringLiteral("GBK")).name());
        QCOMPARE(question->foundEntries(), 0);
        QCOMPARE(question->missingEntries(), 2);
        QCOMPARE(question->matchedEntries(), 2);
        QCOMPARE(question->unlistedNames(), 0);
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

    // A name that no entry names is asked about without an inference, under the encoding of the
    // text files misdecoded in the system encoding, and proposed as misdecoded.
    void an_entry_that_no_name_restores_to_infers_nothing() {
        if (!systemIsGbk()) {
            QSKIP("The preset encodings depend on the system encoding.");
        }
        writeOto({QString::fromUtf8("う.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        const auto question = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(question.has_value());
        QCOMPARE(question->basis(), MisdecodedNameQuestion::None);
        QCOMPARE(question->original(), TextCodec(QStringLiteral("Shift_JIS")).name());
        QCOMPARE(question->misdecodedAs(), TextCodec(QStringLiteral("GBK")).name());
        QCOMPARE(question->matchedEntries(), 0);
        QVERIFY(choiceNamed(*question, kMisdecodedA + QStringLiteral(".wav"))->misdecoded);
    }

    // The entries are decoded in the encoding of the text files, which therefore determines the
    // result of the inference.
    void the_entries_are_read_in_the_encoding_of_the_text_files() {
        writeOto({QString::fromUtf8("あ.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        const auto japanese = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(japanese.has_value());
        QCOMPARE(japanese->basis(), MisdecodedNameQuestion::Entries);
        const auto unicode = MisdecodedNameQuestion::of(source(), QStringLiteral("UTF-8"));
        QVERIFY(!unicode || unicode->basis() != MisdecodedNameQuestion::Entries);
    }

    // Most directories have no misdecoded name. が.wav is a name in Shift_JIS, and 3 of the 4
    // entries find their files.
    void the_names_are_proposed_as_they_are_if_the_entries_find_their_files() {
        if (!systemIsGbk()) {
            QSKIP("The preset encodings depend on the system encoding.");
        }
        writeOto({QStringLiteral("a.wav"), QStringLiteral("b.wav"), QStringLiteral("c.wav"),
                  QStringLiteral("d.wav")});
        write(QStringLiteral("a.wav"), "RIFF");
        write(QStringLiteral("b.wav"), "RIFF");
        write(QStringLiteral("c.wav"), "RIFF");
        write(QString::fromUtf8("が.wav"), "RIFF");

        const auto question = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(question.has_value());
        QCOMPARE(question->basis(), MisdecodedNameQuestion::Found);
        QCOMPARE(question->foundEntries(), 3);
        QCOMPARE(question->missingEntries(), 1);
        QVERIFY(!choiceNamed(*question, QString::fromUtf8("が.wav"))->misdecoded);
        QCOMPARE(question->answer({}), VoiceBankConfig::FileNameEncoding());

        // Fewer than 75 percent
        QVERIFY(QFile::remove(m_dir->filePath(QStringLiteral("c.wav"))));
        const auto fewer = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(fewer.has_value());
        QCOMPARE(fewer->basis(), MisdecodedNameQuestion::None);
        QVERIFY(choiceNamed(*fewer, QString::fromUtf8("が.wav"))->misdecoded);
    }

    // 偁 is not a name in Shift_JIS, so the names are not proposed as they are.
    void a_name_unrepresentable_in_the_encoding_of_the_text_files_is_not_proposed_as_it_is() {
        if (!systemIsGbk()) {
            QSKIP("The preset encodings depend on the system encoding.");
        }
        QVERIFY(!TextCodec(QStringLiteral("Shift_JIS")).canEncode(kMisdecodedA));
        writeOto({QStringLiteral("a.wav")});
        write(QStringLiteral("a.wav"), "RIFF");
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");

        const auto question = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(question.has_value());
        QCOMPARE(question->basis(), MisdecodedNameQuestion::None);
        QVERIFY(choiceNamed(*question, kMisdecodedA + QStringLiteral(".wav"))->misdecoded);
    }

    // Without an oto.ini, no entry finds its file.
    void a_directory_without_entries_is_asked_about() {
        if (!systemIsGbk()) {
            QSKIP("The preset encodings depend on the system encoding.");
        }
        write(QString::fromUtf8("が.wav"), "RIFF");
        const auto question = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(question.has_value());
        QCOMPARE(question->basis(), MisdecodedNameQuestion::None);
        QCOMPARE(question->foundEntries() + question->missingEntries(), 0);
    }

    // Shift_JIS read as Big5 restores 觡.wav to 漾.wav, and Big5 read as GBK restores い.wav to
    // 中.wav, one entry each. The pair written in the encoding of the text files comes first,
    // although the other pair misdecodes in GBK, the system encoding of Chinese systems.
    void a_pair_written_in_the_encoding_of_the_text_files_comes_first() {
        writeOto({QString::fromUtf8("漾.wav"), QString::fromUtf8("中.wav")});
        write(QString::fromUtf8("觡.wav"), "RIFF");
        write(QString::fromUtf8("い.wav"), "RIFF");

        const auto question = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(question.has_value());
        QCOMPARE(question->basis(), MisdecodedNameQuestion::Entries);
        QCOMPARE(question->matchedEntries(), 1);
        QCOMPARE(question->original(), TextCodec(QStringLiteral("Shift_JIS")).name());
        QCOMPARE(question->misdecodedAs(), TextCodec(QStringLiteral("Big5")).name());
    }

    // 偄.wav restores, but to no entry.
    void a_name_without_an_entry_is_counted() {
        writeOto({QString::fromUtf8("あ.wav"), QString::fromUtf8("が.wav")});
        write(kMisdecodedA + QStringLiteral(".wav"), "RIFF");
        write(kMisdecodedI + QStringLiteral(".wav"), "RIFF");
        write(QString::fromUtf8("が.wav"), "RIFF");

        const auto question = MisdecodedNameQuestion::of(source(), QStringLiteral("Shift_JIS"));
        QVERIFY(question.has_value());
        QCOMPARE(question->basis(), MisdecodedNameQuestion::Entries);
        QCOMPARE(question->foundEntries(), 1);
        QCOMPARE(question->unlistedNames(), 1);
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
        QCOMPARE(question->basis(), MisdecodedNameQuestion::Record);
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
