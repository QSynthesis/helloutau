#include <QtTest/QTest>

#include <hellokit/Support/TextCodec.h>
#include <hellokit/VoiceBank/MisdecodedNameConversion.h>

using namespace hello::kit;

using FileNameEncoding = VoiceBankConfiguration::FileNameEncoding;

// あ, い and う in Shift_JIS read as GBK, the names of a Japanese voice bank extracted on a
// Chinese system.
static const QString kMisdecodedA = QString::fromUtf8("偁");
static const QString kMisdecodedI = QString::fromUtf8("偄");
static const QString kMisdecodedU = QString::fromUtf8("偆");

// A name that GBK cannot represent, which therefore does not restore from GBK.
static const QString kKorean = QString::fromUtf8("한");

class test_MisdecodedNameConversion : public QObject {
    Q_OBJECT

private:
    static QString wav(const QString &name) {
        return name + QStringLiteral(".wav");
    }

    /// The record of Shift_JIS read as GBK, with lists if \a files .
    static FileNameEncoding japaneseOnChinese(std::optional<FileNameEncoding::Files> files = {}) {
        FileNameEncoding record;
        record.original = QStringLiteral("Shift_JIS");
        record.misdecodedAs = QStringLiteral("GBK");
        record.files = files;
        return record;
    }

    /// A directory whose entries name あ.wav and い.wav, which the disk holds as 偁.wav and
    /// 偄.wav, beside 偆.wav, which an entry names as it is, an ASCII name, a name that does not
    /// restore and a subdirectory 偁.
    static MisdecodedNameConversion::Listing listing() {
        MisdecodedNameConversion::Listing listing;
        listing.directory = QStringLiteral("sub").toStdU16String();
        listing.charset = QStringLiteral("Shift_JIS");
        listing.audioFiles = {wav(kMisdecodedA), wav(kMisdecodedI), wav(kMisdecodedU),
                              QStringLiteral("a.wav"), wav(kKorean)};
        listing.subdirectories = {kMisdecodedA, QStringLiteral("b")};
        listing.entries = {QString::fromUtf8("あ.wav"), QString::fromUtf8("い.wav"),
                           wav(kMisdecodedU)};
        return listing;
    }

    static QStringList namesOf(const MisdecodedNameConversion &conversion) {
        QStringList names;
        for (const auto &choice : conversion.choices()) {
            names.push_back(choice.name);
        }
        return names;
    }

    static QStringList checkedOf(const MisdecodedNameConversion &conversion) {
        QStringList names;
        for (const auto &choice : conversion.choices()) {
            if (choice.misdecoded) {
                names.push_back(choice.name);
            }
        }
        return names;
    }

private Q_SLOTS:
    // Without a record, the entries infer the encodings. Every name that restores is listed and
    // checked unless an entry names it as it is.
    void a_directory_without_a_record_checks_the_names_no_entry_names() {
        const auto conversion = MisdecodedNameConversion::of(listing());
        QCOMPARE(conversion.directory(), QStringLiteral("sub").toStdU16String());
        QCOMPARE(conversion.original(), TextCodec(QStringLiteral("Shift_JIS")).name());
        QCOMPARE(conversion.misdecodedAs(), TextCodec(QStringLiteral("GBK")).name());
        QVERIFY(!conversion.isRecorded());
        QCOMPARE(namesOf(conversion), QStringList({wav(kMisdecodedA), wav(kMisdecodedI),
                                                   wav(kMisdecodedU), kMisdecodedA}));
        QCOMPARE(checkedOf(conversion),
                 QStringList({wav(kMisdecodedA), wav(kMisdecodedI), kMisdecodedA}));
        QCOMPARE(conversion.choices().at(0).restored, QString::fromUtf8("あ.wav"));
        QVERIFY(!conversion.choices().at(0).directory);
        QVERIFY(conversion.choices().at(3).directory);
        QCOMPARE(conversion.unrestorable(), QStringList({wav(kKorean)}));
    }

    // Without an entry to infer from, the names are misdecoded in the system encoding. The bytes
    // of the names read as kana in Shift_JIS only if the system encoding is GBK.
    void a_directory_without_entries_is_ranked_in_the_system_encoding() {
        auto source = listing();
        source.entries.clear();
        source.charset = QStringLiteral("Big5");
        const auto conversion = MisdecodedNameConversion::of(source);
        const auto system = TextCodec(TextCodec::systemName()).name();
        QCOMPARE(conversion.misdecodedAs(), system);
        QVERIFY(conversion.original() != system);
        if (system != TextCodec(QStringLiteral("GBK")).name()) {
            QSKIP("The names are misdecoded as GBK.");
        }
        QCOMPARE(conversion.original(), TextCodec(QStringLiteral("Shift_JIS")).name());
    }

    // The bytes of correct Chinese names read best in GBK, which is no original encoding if the
    // names are misdecoded as GBK.
    void the_system_encoding_is_no_original_encoding() {
        MisdecodedNameConversion::Listing source;
        source.audioFiles = {QString::fromUtf8("你好.wav")};
        const auto conversion = MisdecodedNameConversion::of(source);
        QVERIFY(conversion.original() != conversion.misdecodedAs());
    }

    // The record determines the states of the names it lists, and the names in neither list
    // are proposed as without a record.
    void a_record_with_lists_checks_its_misdecoded_names() {
        auto source = listing();
        source.record =
            japaneseOnChinese(FileNameEncoding::Files{{wav(kMisdecodedU)}, {wav(kMisdecodedA)}});
        const auto conversion = MisdecodedNameConversion::of(source);
        QVERIFY(conversion.isRecorded());
        QCOMPARE(checkedOf(conversion),
                 QStringList({wav(kMisdecodedI), wav(kMisdecodedU), kMisdecodedA}));
    }

    void a_record_without_lists_checks_every_name() {
        auto source = listing();
        source.record = japaneseOnChinese();
        const auto conversion = MisdecodedNameConversion::of(source);
        QVERIFY(conversion.isRecorded());
        QCOMPARE(checkedOf(conversion), namesOf(conversion));
    }

    // A record without encodings determines every name as correct, under the inferred
    // encodings.
    void a_record_without_encodings_checks_no_name() {
        auto source = listing();
        source.record = FileNameEncoding();
        const auto conversion = MisdecodedNameConversion::of(source);
        QVERIFY(conversion.isRecorded());
        QCOMPARE(conversion.original(), TextCodec(QStringLiteral("Shift_JIS")).name());
        QCOMPARE(namesOf(conversion).size(), 4);
        QVERIFY(checkedOf(conversion).isEmpty());
    }

    // The lists of a record of other encodings determine the states under the encodings that
    // the user chose, which restore the names, and the names that a rename leaves keep them.
    void a_record_of_other_encodings_applies() {
        auto source = listing();
        auto record = japaneseOnChinese(FileNameEncoding::Files{{}, {wav(kMisdecodedA)}});
        record.original = QStringLiteral("Big5");
        source.record = record;
        QCOMPARE(MisdecodedNameConversion::of(source).original(),
                 TextCodec(QStringLiteral("Big5")).name());

        const auto conversion = MisdecodedNameConversion::of(source, QStringLiteral("Shift_JIS"),
                                                             QStringLiteral("GBK"));
        QVERIFY(conversion.isRecorded());
        QCOMPARE(conversion.choices().at(1).restored, QString::fromUtf8("い.wav"));
        QCOMPARE(checkedOf(conversion), QStringList({wav(kMisdecodedI), kMisdecodedA}));
        QCOMPARE(conversion.keptRecord(),
                 japaneseOnChinese(FileNameEncoding::Files{{}, {wav(kMisdecodedA)}}));
    }

    // Reading as misdecoded determines every name that is not ASCII, the unchecked names and
    // the names that do not restore as correct.
    void the_record_of_the_checked_names_determines_every_name() {
        const auto conversion = MisdecodedNameConversion::of(listing());
        const QStringList checked{wav(kMisdecodedA)};
        QCOMPARE(conversion.record(checked),
                 japaneseOnChinese(FileNameEncoding::Files{
                     {wav(kMisdecodedA)},
                     {wav(kMisdecodedI), wav(kMisdecodedU), kMisdecodedA, wav(kKorean)}
        }));
        QCOMPARE(conversion.record(namesOf(conversion)), japaneseOnChinese());
        QVERIFY(conversion.record({}).isAsIs());
    }

    // The names that a rename leaves keep the states of the record, and are correct without
    // one.
    void the_kept_record_is_the_record_of_the_directory() {
        auto source = listing();
        const auto record = japaneseOnChinese(FileNameEncoding::Files{{wav(kMisdecodedU)}, {}});
        source.record = record;
        QCOMPARE(MisdecodedNameConversion::of(source).keptRecord(), record);

        source.record = FileNameEncoding();
        QCOMPARE(MisdecodedNameConversion::of(source).keptRecord(),
                 japaneseOnChinese(FileNameEncoding::Files()));
    }
};

QTEST_GUILESS_MAIN(test_MisdecodedNameConversion)
#include "test_MisdecodedNameConversion.moc"
