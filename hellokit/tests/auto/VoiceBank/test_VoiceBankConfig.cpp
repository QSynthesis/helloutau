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
};

QTEST_APPLESS_MAIN(test_VoiceBankConfig)

#include "test_VoiceBankConfig.moc"
