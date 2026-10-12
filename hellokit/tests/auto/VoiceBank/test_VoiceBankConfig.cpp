#include <QtCore/QJsonObject>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/VoiceBankConfig.h>

using namespace hello::kit;

class test_VoiceBankConfig : public QObject {
    Q_OBJECT

private:
    static std::optional<VoiceBankConfig> parse(const QByteArray &json,
                                                DiagnosticList *sink = nullptr) {
        DiagnosticList diagnostics;
        auto config = VoiceBankConfig::fromJson(json, diagnostics);
        if (sink) {
            *sink = diagnostics;
        }
        return config;
    }

    /// The record of a Japanese voice bank extracted on a Chinese system.
    static VoiceBankConfig::FileNameEncoding japaneseOnChinese() {
        VoiceBankConfig::FileNameEncoding record;
        record.original = QStringLiteral("Shift_JIS");
        record.misdecodedAs = QStringLiteral("GBK");
        return record;
    }

private Q_SLOTS:
    void it_reads_the_encoding() {
        const auto config = parse(R"({"$format":"hello-voicebank","charset":"GBK"})");
        QVERIFY(config.has_value());
        QCOMPARE(config->charset, QStringLiteral("GBK"));
    }

    // A foreign file may still be valid JSON, and reading it field by field would yield a
    // configuration of defaults instead of a rejection.
    void a_file_that_is_not_ours_is_refused() {
        DiagnosticList diagnostics;
        QVERIFY(!parse(R"({"charset":"GBK"})", &diagnostics).has_value());
        QVERIFY(hasError(diagnostics));

        QVERIFY(!parse("not json at all", &diagnostics).has_value());
        QVERIFY(hasError(diagnostics));

        QVERIFY(!parse("[]", &diagnostics).has_value());
        QVERIFY(hasError(diagnostics));
    }

    // Otherwise a build older than the file would discard unrecognized data, and the user would
    // find it missing after saving.
    void an_unrecognized_field_is_written_back() {
        const auto config =
            parse(R"({"$format":"hello-voicebank","charset":"GBK","somethingNew":42})");
        QVERIFY(config.has_value());
        QCOMPARE(config->unknownFields.value(QStringLiteral("somethingNew")).toInt(), 42);

        const auto again = parse(config->toJson());
        QVERIFY(again.has_value());
        QCOMPARE(again->charset, QStringLiteral("GBK"));
        QCOMPARE(again->unknownFields.value(QStringLiteral("somethingNew")).toInt(), 42);
    }

    void the_format_marker_is_not_kept_as_an_unknown_field() {
        const auto config = parse(R"({"$format":"hello-voicebank","charset":"GBK"})");
        QVERIFY(config.has_value());
        QVERIFY(config->unknownFields.isEmpty());
    }

    void it_survives_a_trip_through_a_file() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path =
            std::filesystem::path(dir.path().toStdU16String()) / VoiceBankConfig::fileName;

        VoiceBankConfig config;
        config.charset = QStringLiteral("Shift_JIS");

        DiagnosticList diagnostics;
        QVERIFY(config.save(path, diagnostics));
        QVERIFY(!hasError(diagnostics));

        const auto read = VoiceBankConfig::open(path, diagnostics);
        QVERIFY(read.has_value());
        QCOMPARE(read->charset, QStringLiteral("Shift_JIS"));
    }

    void a_missing_file_is_an_error_rather_than_an_empty_record() {
        DiagnosticList diagnostics;
        const auto read = VoiceBankConfig::open("nowhere/hello-config.json", diagnostics);
        QVERIFY(!read.has_value());
        QVERIFY(hasError(diagnostics));
    }

    void the_record_of_the_file_names_is_read_and_written() {
        const auto config = parse(R"({"$format":"hello-voicebank","charset":"Shift_JIS",
            "fileNameEncoding":{"original":"Shift_JIS","misdecodedAs":"GBK",
            "files":{"misdecoded":["_偁媧偁揻.wav"],"correct":["_あ長.wav"]}}})");
        QVERIFY(config.has_value());
        QVERIFY(config->fileNameEncoding.has_value());
        QVERIFY(config->unknownFields.isEmpty());

        const auto again = parse(config->toJson());
        QVERIFY(again.has_value());
        QVERIFY(again->fileNameEncoding.has_value());
        const auto &record = *again->fileNameEncoding;
        QCOMPARE(record.original, QStringLiteral("Shift_JIS"));
        QCOMPARE(record.misdecodedAs, QStringLiteral("GBK"));
        QVERIFY(record.files.has_value());
        QCOMPARE(record.files->misdecoded, QStringList{QString::fromUtf8("_偁媧偁揻.wav")});
        QCOMPARE(record.files->correct, QStringList{QString::fromUtf8("_あ長.wav")});
    }

    // The two forms without lists record a decision as well, and are distinct from no record.
    void the_records_without_lists_are_read_and_written() {
        const auto asIs =
            parse(R"({"$format":"hello-voicebank","charset":"GBK","fileNameEncoding":{}})");
        QVERIFY(asIs.has_value());
        QCOMPARE(asIs->fileNameEncoding, VoiceBankConfig::FileNameEncoding());
        QVERIFY(asIs->fileNameEncoding->isAsIs());
        QCOMPARE(parse(asIs->toJson())->fileNameEncoding, asIs->fileNameEncoding);

        const auto restored = parse(R"({"$format":"hello-voicebank","charset":"GBK",
            "fileNameEncoding":{"original":"Shift_JIS","misdecodedAs":"GBK"}})");
        QVERIFY(restored.has_value());
        QCOMPARE(restored->fileNameEncoding, japaneseOnChinese());
        QVERIFY(!restored->fileNameEncoding->isAsIs());
        QCOMPARE(parse(restored->toJson())->fileNameEncoding, japaneseOnChinese());
        QCOMPARE(japaneseOnChinese().toJson().keys(),
                 (QStringList{QStringLiteral("misdecodedAs"), QStringLiteral("original")}));
    }

    void a_configuration_without_the_record_writes_none() {
        VoiceBankConfig config;
        config.charset = QStringLiteral("GBK");
        const auto again = parse(config.toJson());
        QVERIFY(again.has_value());
        QVERIFY(!again->fileNameEncoding.has_value());
        QVERIFY(!again->unknownFields.contains(QStringLiteral("fileNameEncoding")));
    }

    // Removing the record would make a later save erase the record of a newer version.
    void a_malformed_record_is_reported_and_written_back() {
        DiagnosticList diagnostics;
        const auto config = parse(R"({"$format":"hello-voicebank","charset":"GBK",
            "fileNameEncoding":{"original":"Shift_JIS"}})",
                                  &diagnostics);
        QVERIFY(config.has_value());
        QVERIFY(!config->fileNameEncoding.has_value());
        QVERIFY(!diagnostics.isEmpty());
        QVERIFY(!hasError(diagnostics));

        const auto again = parse(config->toJson());
        QVERIFY(again.has_value());
        const auto kept = again->unknownFields.value(QStringLiteral("fileNameEncoding")).toObject();
        QCOMPARE(kept.value(QStringLiteral("original")).toString(), QStringLiteral("Shift_JIS"));
    }

    void a_malformed_record_is_not_read() {
        QFETCH(QByteArray, record);
        DiagnosticList diagnostics;
        const auto config = parse(R"({"$format":"hello-voicebank","charset":"GBK",
            "fileNameEncoding":)" + record +
                                      "}",
                                  &diagnostics);
        QVERIFY(config.has_value());
        QVERIFY(!config->fileNameEncoding.has_value());
        QVERIFY(!diagnostics.isEmpty());
    }

    void a_malformed_record_is_not_read_data() {
        QTest::addColumn<QByteArray>("record");
        QTest::newRow("not an object") << QByteArray(R"("Shift_JIS")");
        QTest::newRow("one encoding") << QByteArray(R"({"misdecodedAs":"GBK"})");
        QTest::newRow("empty encoding") << QByteArray(R"({"original":"","misdecodedAs":"GBK"})");
        QTest::newRow("unknown field") << QByteArray(R"({"original":"Shift_JIS",
            "misdecodedAs":"GBK","misdecoded":[]})");
        QTest::newRow("lists without encodings")
            << QByteArray(R"({"files":{"misdecoded":[],"correct":[]}})");
        QTest::newRow("one list") << QByteArray(R"({"original":"Shift_JIS","misdecodedAs":"GBK",
            "files":{"misdecoded":[]}})");
        QTest::newRow("unknown list") << QByteArray(R"({"original":"Shift_JIS",
            "misdecodedAs":"GBK","files":{"misdecoded":[],"correct":[],"other":[]}})");
        QTest::newRow("not strings") << QByteArray(R"({"original":"Shift_JIS",
            "misdecodedAs":"GBK","files":{"misdecoded":[1],"correct":[]}})");
        QTest::newRow("files not an object")
            << QByteArray(R"({"original":"Shift_JIS","misdecodedAs":"GBK","files":[]})");
    }

    // The record without lists restores every name, the record with lists the misdecoded ones,
    // and the record without encodings none.
    void the_logical_name_follows_the_form_of_the_record() {
        const auto misdecoded = QString::fromUtf8("_偁媧偁揻.wav");
        const auto correct = QString::fromUtf8("が.wav");
        const auto restored = QString::fromUtf8("_あ吸あ吐.wav");

        auto record = japaneseOnChinese();
        QCOMPARE(record.logicalNameOf(misdecoded), restored);
        QCOMPARE(record.logicalNameOf(correct), QString::fromUtf8("､ｬ.wav"));
        QCOMPARE(record.logicalNameOf(QString::fromUtf8("你.wav")), QString::fromUtf8("你.wav"));

        record.files = VoiceBankConfig::FileNameEncoding::Files{{misdecoded}, {correct}};
        QCOMPARE(record.logicalNameOf(misdecoded), restored);
        QCOMPARE(record.logicalNameOf(correct), correct);
        QCOMPARE(record.logicalNameOf(misdecoded.normalized(QString::NormalizationForm_D)),
                 restored);

        QCOMPARE(VoiceBankConfig::FileNameEncoding().logicalNameOf(misdecoded), misdecoded);
    }

    void a_misdecoded_name_is_restored() {
        const auto record = japaneseOnChinese();
        QCOMPARE(record.restore(QString::fromUtf8("_偁媧偁揻.wav")),
                 QString::fromUtf8("_あ吸あ吐.wav"));
        QCOMPARE(record.restore(QString::fromUtf8("_偁丒偁_wav.frq")),
                 QString::fromUtf8("_あ・あ_wav.frq"));
    }

    // A correct name restores as well, which is why the record lists the state of each name.
    void a_correct_name_restores_to_another_name() {
        QCOMPARE(japaneseOnChinese().restore(QString::fromUtf8("が.wav")),
                 QString::fromUtf8("､ｬ.wav"));
    }

    void a_decomposed_name_is_restored_as_composed() {
        const auto composed = QString::fromUtf8("が.wav");
        const auto decomposed = composed.normalized(QString::NormalizationForm_D);
        QVERIFY(decomposed != composed);
        QCOMPARE(japaneseOnChinese().restore(decomposed), japaneseOnChinese().restore(composed));
    }

    void a_name_that_does_not_restore_is_refused() {
        const auto record = japaneseOnChinese();
        QCOMPARE(record.restore(QStringLiteral("_aa.wav")), std::nullopt);
        // Unrepresentable in GBK.
        QCOMPARE(record.restore(QString::fromUtf8("가.wav")), std::nullopt);
        // C4 E3 2E is an invalid byte sequence in Shift_JIS.
        QCOMPARE(record.restore(QString::fromUtf8("你.wav")), std::nullopt);
        // B1 5C restores to a halfwidth katakana and a backslash.
        QCOMPARE(record.restore(QString::fromUtf8("盶.wav")), std::nullopt);

        // An ASCII name is not restored even if its bytes are other characters in the original
        // encoding: 61 62 2E 77 61 76 is valid UTF-16LE.
        auto wide = record;
        wide.original = QStringLiteral("UTF-16LE");
        QCOMPARE(wide.restore(QStringLiteral("ab.wav")), std::nullopt);

        auto unavailable = record;
        unavailable.original = QStringLiteral("no-such-encoding");
        QCOMPARE(unavailable.restore(QString::fromUtf8("_偁媧偁揻.wav")), std::nullopt);
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankConfig)

#include "test_VoiceBankConfig.moc"
