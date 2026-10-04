#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/VoiceBankSource.h>

using namespace hello::kit;

// あ in Shift_JIS, which is not valid UTF-8 and therefore reveals immediately whether it was
// decoded.
static const QByteArray kA = QByteArray("\x82\xa0", 2);

class test_VoiceBankSource : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    std::filesystem::path root() const {
        return std::filesystem::path(m_dir->path().toStdU16String());
    }

    void write(const QString &relative, const QByteArray &bytes) {
        const QString path = m_dir->path() + QLatin1Char('/') + relative;
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    static const VoiceBankDirectorySource *at(const VoiceBankSource &source,
                                              const std::filesystem::path &relative) {
        for (const auto &directory : source.directories()) {
            if (directory.path == relative) {
                return &directory;
            }
        }
        return nullptr;
    }

private Q_SLOTS:
    // A new directory per test case, because each builds its own tree.
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() {
        m_dir.reset();
    }

    void it_reads_what_a_bank_holds() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kA + ",1,2,3,4,5\n");
        write(QStringLiteral("prefix.map"), "C4\t\t_B\n");
        write(QStringLiteral("character.txt"), "name=" + kA + "\n");
        write(QStringLiteral("readme.txt"), kA);
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        const auto source = VoiceBankSource::open(root(), diagnostics);
        QVERIFY(source.has_value());
        QVERIFY(!hasError(diagnostics));

        const auto directory = at(*source, {});
        QVERIFY(directory);
        QVERIFY(directory->oto.has_value());
        QVERIFY(directory->prefixMap.has_value());
        QVERIFY(directory->character.has_value());
        QCOMPARE(directory->contents.at(VoiceBankDirectorySource::Readme), kA);
        QCOMPARE(directory->contents.at(VoiceBankDirectorySource::Character), "name=" + kA + "\n");
        QCOMPARE(directory->textFiles(),
                 (std::vector<VoiceBankDirectorySource::File>{
                     VoiceBankDirectorySource::Oto, VoiceBankDirectorySource::PrefixMap,
                     VoiceBankDirectorySource::Character, VoiceBankDirectorySource::Readme}));
        QCOMPARE(directory->audioFiles.size(), size_t(1));
        QCOMPARE(directory->audioFiles.at(0), std::filesystem::path("a.wav"));
    }

    // The purpose of reading before decoding: a selector must display the bytes under each
    // candidate encoding, which is impossible if reading already required an encoding.
    void nothing_is_decoded() {
        write(QStringLiteral("oto.ini"), "a.wav=" + kA + ",0,0,0,0,0\n");

        DiagnosticList diagnostics;
        const auto source = VoiceBankSource::open(root(), diagnostics);
        QVERIFY(source.has_value());

        const auto aliases = at(*source, {})->rawAliases();
        QCOMPARE(aliases.size(), 1);
        QCOMPARE(QByteArray(aliases.at(0).data(), aliases.at(0).size()), kA);
    }

    void it_goes_into_subdirectories() {
        write(QStringLiteral("oto.ini"), "a.wav=a,0,0,0,0,0\n");
        write(QStringLiteral("A4/oto.ini"), "b.wav=b,0,0,0,0,0\n");
        write(QStringLiteral("A4/deeper/oto.ini"), "c.wav=c,0,0,0,0,0\n");

        DiagnosticList diagnostics;
        const auto source = VoiceBankSource::open(root(), diagnostics);
        QVERIFY(source.has_value());

        QVERIFY(at(*source, {})->oto.has_value());
        QVERIFY(at(*source, "A4")->oto.has_value());
        QVERIFY(at(*source, std::filesystem::path("A4") / "deeper")->oto.has_value());
    }

    // A voice bank is a folder selected by the user, so its structure cannot be trusted.
    void a_tree_deeper_than_the_limit_is_truncated_with_a_warning() {
        write(QStringLiteral("a/b/c/oto.ini"), "x.wav=x,0,0,0,0,0\n");

        VoiceBankLimits limits;
        limits.maxDepth = 1;

        DiagnosticList diagnostics;
        const auto source = VoiceBankSource::open(root(), diagnostics, limits);
        QVERIFY(source.has_value());
        QVERIFY(!at(*source, std::filesystem::path("a") / "b"));
        QCOMPARE(diagnostics.size(), 1);
        QCOMPARE(diagnostics.at(0).severity, DiagnosticSeverity::Warning);
    }

    void a_directory_with_a_record_is_settled_and_one_without_is_not() {
        write(QStringLiteral("oto.ini"), "a.wav=a,0,0,0,0,0\n");
        write(QStringLiteral("A4/oto.ini"), "b.wav=b,0,0,0,0,0\n");
        write(QStringLiteral("A4/hello-config.json"),
              R"({"$format":"hello-voicebank","charset":"Shift_JIS"})");

        DiagnosticList diagnostics;
        const auto source = VoiceBankSource::open(root(), diagnostics);
        QVERIFY(source.has_value());

        const auto unsettled = source->unsettled();
        QCOMPARE(unsettled.size(), 1);
        QCOMPARE(unsettled.at(0)->path, std::filesystem::path());

        QVERIFY(at(*source, "A4")->config.has_value());
        QCOMPARE(at(*source, "A4")->settledCharset(), QStringLiteral("Shift_JIS"));
    }

    // A declaration of UTF-8 settles the entire directory, the other files of the root with it,
    // and takes precedence over the configuration.
    void a_declaration_settles_the_directory() {
        write(QStringLiteral("oto.ini"), "#Charset:utf8\na.wav=a,0,0,0,0,0\n");
        write(QStringLiteral("character.txt"), "name=a\n");
        write(QStringLiteral("hello-config.json"),
              R"({"$format":"hello-voicebank","charset":"GBK"})");

        DiagnosticList diagnostics;
        const auto source = VoiceBankSource::open(root(), diagnostics);
        QVERIFY(source.has_value());
        QCOMPARE(at(*source, {})->settledCharset(), QStringLiteral("UTF-8"));
        QVERIFY(source->unsettled().isEmpty());
    }

    // A directory containing only samples has no text, so no encoding is required and asking
    // the user would be pointless.
    void a_directory_with_nothing_to_decode_is_not_asked_about() {
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        const auto source = VoiceBankSource::open(root(), diagnostics);
        QVERIFY(source.has_value());
        QVERIFY(at(*source, {})->textFiles().empty());
        QVERIFY(at(*source, {})->isSettled());
        QVERIFY(source->unsettled().isEmpty());
    }

    void a_path_that_is_not_a_folder_is_refused() {
        write(QStringLiteral("a.wav"), "RIFF");

        DiagnosticList diagnostics;
        const auto path = root() / "a.wav";
        QVERIFY(!VoiceBankSource::open(path, diagnostics).has_value());
        QVERIFY(hasError(diagnostics));
    }
};

QTEST_APPLESS_MAIN(test_VoiceBankSource)

#include "test_VoiceBankSource.moc"
