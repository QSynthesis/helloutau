#include <filesystem>
#include <optional>

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <zip.h>

#include <hellokit/Support/Diagnostic.h>
#include <hellokit/Support/TextCodec.h>
#include <hellokit/VoiceBank/InstallArchive.h>
#include <hellokit/VoiceBank/VoiceBankConfiguration.h>

using namespace hello::kit;

namespace fs = std::filesystem;

namespace {

    struct TestEntry {
        QByteArray name;
        QByteArray data;
        zip_flags_t encoding = ZIP_FL_ENC_GUESS;
    };

    TestEntry file(const QByteArray &name, const QByteArray &data = {}) {
        return {name, data, ZIP_FL_ENC_GUESS};
    }

    // A name stored as raw bytes without the UTF-8 flag, as old archivers stored Shift_JIS
    TestEntry rawFile(const QByteArray &name, const QByteArray &data = {}) {
        return {name, data, ZIP_FL_ENC_CP437};
    }

    bool writeArchive(const QString &path, const QList<TestEntry> &entries) {
        int error = 0;
        const auto zip =
            zip_open(QFile::encodeName(path).constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);
        if (!zip) {
            return false;
        }
        for (const auto &entry : entries) {
            const auto source =
                zip_source_buffer(zip, entry.data.constData(), zip_uint64_t(entry.data.size()), 0);
            if (!source || zip_file_add(zip, entry.name.constData(), source, entry.encoding) < 0) {
                zip_source_free(source);
                zip_discard(zip);
                return false;
            }
        }
        return zip_close(zip) == 0;
    }

    fs::path pathOf(const QString &path) {
        return fs::path(path.toStdU16String());
    }

    // Returns the files and directories under path, relative to it and sorted.
    QStringList listing(const QString &path) {
        QStringList names;
        QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            names.append(QDir(path).relativeFilePath(it.next()));
        }
        names.sort();
        return names;
    }

    QByteArray contentOf(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    QByteArray manifest(const QByteArray &type, const QByteArray &folder,
                        const QByteArray &contents = {}) {
        QByteArray text = "type=" + type + "\r\nfolder=" + folder + "\r\n";
        if (!contents.isEmpty()) {
            text += "contentsdir=" + contents + "\r\n";
        }
        return text + "description=A bank\r\n";
    }

    // U+3042 in Shift_JIS
    const QByteArray shiftJisA("\x82\xA0");

}

class test_InstallArchive : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;

    QString archivePath() const {
        return m_root.filePath(QStringLiteral("bank.uar"));
    }

    QString voicePath() const {
        return m_root.filePath(QStringLiteral("voice"));
    }

    QString voicePath(const QString &name) const {
        return m_root.filePath(QStringLiteral("voice/") + name);
    }

private Q_SLOTS:
    void init() {
        QVERIFY(m_root.isValid());
        QDir(voicePath()).removeRecursively();
        TextCodec::setSystemName(QString());
    }

    void cleanupTestCase() {
        TextCodec::setSystemName(QString());
    }

    // The folder of contentsdir is installed as the folder of folder, without install.txt and
    // the other files, and its oto.ini folders record the charset and the names as they are.
    void a_voice_bank_installs_into_its_folder() {
        QVERIFY(writeArchive(archivePath(),
                             {file("install.txt", manifest("voiceset", "bank", "contents")),
                              file("contents/character.txt", "name=bank\r\n"),
                              file("contents/oto.ini", "a.wav=a,0,0,0,0,0\r\n"),
                              file("contents/a.wav", "a"), file("contents/sub/oto.ini"),
                              file("contents/sub/b.wav", "b"), file("other/x.txt")}));
        InstallArchive archive;
        QVERIFY2(archive.open(pathOf(archivePath())), qPrintable(archive.errorString()));
        QVERIFY(!archive.needsNameEncoding());
        QVERIFY2(archive.setNameEncoding(QString()), qPrintable(archive.errorString()));
        QCOMPARE(archive.kind(), InstallArchive::VoiceBank);
        QCOMPARE(archive.folder(), QStringLiteral("bank"));
        QCOMPARE(archive.contentsDirectory(), QStringLiteral("contents"));
        QCOMPARE(archive.description(), QStringLiteral("A bank"));
        QCOMPARE(archive.contents(),
                 (QStringList{"character.txt", "oto.ini", "a.wav", "sub/oto.ini", "sub/b.wav"}));
        QCOMPARE(archive.readContent(QStringLiteral("a.wav"), 10), std::optional(QByteArray("a")));
        QVERIFY(!archive.readContent(QStringLiteral("missing.wav"), 10));

        QVERIFY2(archive.install(pathOf(voicePath()), QStringLiteral("bank"),
                                 InstallArchive::Decoded, QStringLiteral("Shift_JIS")),
                 qPrintable(archive.errorString()));
        QCOMPARE(listing(voicePath()),
                 (QStringList{"bank", "bank/a.wav", "bank/character.txt", "bank/hello-config.json",
                              "bank/oto.ini", "bank/sub", "bank/sub/b.wav",
                              "bank/sub/hello-config.json", "bank/sub/oto.ini"}));
        DiagnosticList diagnostics;
        const auto configuration =
            VoiceBankConfiguration::open(pathOf(voicePath("bank/hello-config.json")), diagnostics);
        QVERIFY(configuration);
        QCOMPARE(configuration->charset, QStringLiteral("Shift_JIS"));
        QVERIFY(configuration->fileNameEncoding);
        QVERIFY(configuration->fileNameEncoding->isAsIs());
    }

    // The bytes by which a dialog ranks the encodings are the names without the UTF-8 flag and
    // install.txt, not the names marked as UTF-8.
    void the_raw_texts_are_the_names_to_decode_and_install_txt() {
        const auto text = manifest("voiceset", "bank", "contents");
        QVERIFY(
            writeArchive(archivePath(), {
                                            file("install.txt", text),
                                            file("contents/character.txt"),
                                            rawFile("contents/" + shiftJisA + ".wav"),
                                            {"contents/\xE3\x81\x82.wav", {}, ZIP_FL_ENC_UTF_8}
        }));
        InstallArchive archive;
        QVERIFY2(archive.open(pathOf(archivePath())), qPrintable(archive.errorString()));
        QCOMPARE(archive.rawTexts(), (QList<QByteArray>{"contents/" + shiftJisA + ".wav", text}));
    }

    // UTAU refuses an archive without install.txt, and so does HelloUtau.
    void an_archive_without_install_txt_is_rejected() {
        QVERIFY(writeArchive(archivePath(), {file("bank/oto.ini")}));
        InstallArchive archive;
        QVERIFY(!archive.open(pathOf(archivePath())));
        QVERIFY(!archive.errorString().isEmpty());
    }

    void an_invalid_install_txt_is_rejected_data() {
        QTest::addColumn<QByteArray>("manifest");
        QTest::newRow("unknown type") << manifest("probe", "bank", "contents");
        QTest::newRow("no type") << QByteArray("folder=bank\r\ncontentsdir=contents\r\n");
        QTest::newRow("no folder") << QByteArray("type=voiceset\r\ncontentsdir=contents\r\n");
        QTest::newRow("parent folder") << manifest("voiceset", "..", "contents");
        QTest::newRow("folder with a separator") << manifest("voiceset", "a/b", "contents");
        QTest::newRow("folder with a backslash") << manifest("voiceset", "a\\b", "contents");
        QTest::newRow("folder with a drive") << manifest("voiceset", "C:bank", "contents");
        QTest::newRow("parent contents") << manifest("voiceset", "bank", "..");
        QTest::newRow("contents with a separator") << manifest("voiceset", "bank", "a/contents");
#ifdef Q_OS_WIN
        QTest::newRow("reserved folder") << manifest("voiceset", "con", "contents");
#endif
        QTest::newRow("missing contents") << manifest("voiceset", "bank", "absent");
        QTest::newRow("no character.txt") << manifest("voiceset", "bank", "a");
        QTest::newRow("no plugin.txt") << manifest("editplugin", "bank", "contents");
    }

    void an_invalid_install_txt_is_rejected() {
        QFETCH(QByteArray, manifest);
        QVERIFY(writeArchive(archivePath(),
                             {file("install.txt", manifest), file("contents/character.txt"),
                              file("contents/oto.ini"), file("a/contents/x")}));
        InstallArchive archive;
        QVERIFY(archive.open(pathOf(archivePath())));
        QVERIFY(!archive.setNameEncoding(QString()));
        QVERIFY(!archive.errorString().isEmpty());
    }

    // An entry that fails a check of ZipExtractor rejects the archive before anything is written.
    void an_unsafe_entry_rejects_the_archive() {
        QVERIFY(writeArchive(archivePath(),
                             {file("install.txt", manifest("voiceset", "bank", "contents")),
                              file("contents/oto.ini"), file("other/../../evil.txt")}));
        InstallArchive archive;
        QVERIFY(archive.open(pathOf(archivePath())));
        QVERIFY(!archive.setNameEncoding(QString()));
        QVERIFY(!QFileInfo::exists(voicePath()));
    }

    // Without contentsdir, the root of the archive is installed without install.txt, as UTAU
    // installs it, although the specification of the plugins states that folder is taken.
    void an_omitted_contentsdir_installs_the_root() {
        QVERIFY(writeArchive(archivePath(), {file("install.txt", manifest("voiceset", "bank")),
                                             file("character.txt", "name=bank\r\n"),
                                             file("oto.ini"), file("sub/a.wav", "a")}));
        InstallArchive archive;
        QVERIFY(archive.open(pathOf(archivePath())));
        QVERIFY2(archive.setNameEncoding(QString()), qPrintable(archive.errorString()));
        QVERIFY(archive.contentsDirectory().isEmpty());
        QCOMPARE(archive.contents(), (QStringList{"character.txt", "oto.ini", "sub/a.wav"}));
        QVERIFY2(archive.install(pathOf(voicePath()), QStringLiteral("bank"),
                                 InstallArchive::Decoded, QString()),
                 qPrintable(archive.errorString()));
        QCOMPARE(listing(voicePath()),
                 (QStringList{"bank", "bank/character.txt", "bank/hello-config.json",
                              "bank/oto.ini", "bank/sub", "bank/sub/a.wav"}));
    }

    // A name in Shift_JIS without the UTF-8 flag requires the encoding, which decodes it.
    void a_name_in_shift_jis_requires_its_encoding() {
        QVERIFY(writeArchive(archivePath(),
                             {file("install.txt", manifest("voiceset", "bank", "contents")),
                              file("contents/character.txt", "name=bank\r\n"),
                              rawFile("contents/" + shiftJisA + ".wav", "a")}));
        InstallArchive archive;
        QVERIFY(archive.open(pathOf(archivePath())));
        QVERIFY(archive.needsNameEncoding());
        QVERIFY(archive.canDecode(QStringLiteral("Shift_JIS")));
        QVERIFY(!archive.canDecode(QStringLiteral("UTF-8")));
        QVERIFY(archive.setNameEncoding(QStringLiteral("Shift_JIS")));
        const auto name = QString(QChar(0x3042)) + u".wav";
        QCOMPARE(archive.contents(), (QStringList{"character.txt", name}));

        QVERIFY2(archive.install(pathOf(voicePath()), QStringLiteral("bank"),
                                 InstallArchive::Decoded, QString()),
                 qPrintable(archive.errorString()));
        QCOMPARE(listing(voicePath()), (QStringList{"bank", "bank/character.txt", "bank/" + name}));
    }

    // Written as misdecoded, the names are those of UTAU on this computer, and the configuration
    // records the pair of encodings, so that HelloUtau restores the names.
    void the_names_can_be_written_as_misdecoded() {
        TextCodec::setSystemName(QStringLiteral("GBK"));
        QVERIFY(
            writeArchive(archivePath(),
                         {file("install.txt", manifest("voiceset", "bank", "contents")),
                          file("contents/character.txt", "name=bank\r\n"), file("contents/oto.ini"),
                          rawFile("contents/" + shiftJisA + ".wav", "a")}));
        InstallArchive archive;
        QVERIFY(archive.open(pathOf(archivePath())));
        QVERIFY(archive.setNameEncoding(QStringLiteral("Shift_JIS")));
        QVERIFY(archive.canWriteMisdecoded());
        QVERIFY2(archive.install(pathOf(voicePath()), QStringLiteral("bank"),
                                 InstallArchive::Misdecoded, QStringLiteral("Shift_JIS")),
                 qPrintable(archive.errorString()));
        // 82 A0 in GBK is U+5041, as UTAU installed the probe on a computer of GBK
        QVERIFY(QFileInfo::exists(voicePath(QStringLiteral("bank/") + QChar(0x5041) + u".wav")));
        DiagnosticList diagnostics;
        const auto configuration =
            VoiceBankConfiguration::open(pathOf(voicePath("bank/hello-config.json")), diagnostics);
        QVERIFY(configuration && configuration->fileNameEncoding);
        QCOMPARE(configuration->fileNameEncoding->original, QStringLiteral("Shift_JIS"));
        QCOMPARE(configuration->fileNameEncoding->misdecodedAs, QStringLiteral("GBK"));
    }

    // A name of the code page of the system is not offered as misdecoded, and the names that the
    // code page cannot represent are reported.
    void the_code_page_of_the_system_limits_the_names() {
        QVERIFY(writeArchive(archivePath(),
                             {file("install.txt", manifest("voiceset", "bank", "contents")),
                              file("contents/character.txt", "name=bank\r\n"),
                              rawFile("contents/" + shiftJisA + ".wav")}));
        InstallArchive archive;
        QVERIFY(archive.open(pathOf(archivePath())));
        QVERIFY(archive.setNameEncoding(QStringLiteral("Shift_JIS")));

        TextCodec::setSystemName(QStringLiteral("Shift_JIS"));
        QVERIFY(!archive.canWriteMisdecoded());
        QVERIFY(archive.unrepresentableNames().isEmpty());

        TextCodec::setSystemName(QStringLiteral("windows-1252"));
        QCOMPARE(archive.unrepresentableNames(), QStringList{QString(QChar(0x3042)) + u".wav"});
    }

    // A reinstallation into the existing folder merges, as UTAU does: a file of the same name is
    // replaced and the other files are kept. A new folder takes the first free number.
    void a_reinstallation_merges_or_takes_a_new_folder() {
        QVERIFY(writeArchive(archivePath(),
                             {file("install.txt", manifest("voiceset", "bank", "contents")),
                              file("contents/character.txt", "name=bank\r\n"),
                              file("contents/oto.ini", "new"), file("contents/a.wav", "new")}));
        QVERIFY(QDir().mkpath(voicePath("bank")));
        for (const auto &[name, data] :
             {std::pair("oto.ini", "old"), std::pair("mine.frq", "own")}) {
            QFile old(voicePath(QStringLiteral("bank/") + QLatin1String(name)));
            QVERIFY(old.open(QIODevice::WriteOnly));
            old.write(data);
        }
        InstallArchive archive;
        QVERIFY(archive.open(pathOf(archivePath())));
        QVERIFY(archive.setNameEncoding(QString()));
        QCOMPARE(InstallArchive::freeFolderName(pathOf(voicePath()), QStringLiteral("bank")),
                 QStringLiteral("bank0"));
        QVERIFY2(archive.install(pathOf(voicePath()), QStringLiteral("bank"),
                                 InstallArchive::Decoded, QString()),
                 qPrintable(archive.errorString()));
        QCOMPARE(contentOf(voicePath("bank/oto.ini")), QByteArray("new"));
        QCOMPARE(contentOf(voicePath("bank/mine.frq")), QByteArray("own"));
        QCOMPARE(contentOf(voicePath("bank/a.wav")), QByteArray("new"));
        // The temporary folder is gone.
        QCOMPARE(
            QDir(voicePath()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden),
            QStringList{"bank"});

        QVERIFY(QDir().mkpath(voicePath("bank0")));
        QCOMPARE(InstallArchive::freeFolderName(pathOf(voicePath()), QStringLiteral("bank")),
                 QStringLiteral("bank1"));
        QCOMPARE(InstallArchive::freeFolderName(pathOf(voicePath()), QStringLiteral("other")),
                 QStringLiteral("other"));
    }

    // A plugin installs as a voice bank does, without hello-config.json.
    void a_plugin_installs_without_a_configuration() {
        QVERIFY(writeArchive(archivePath(),
                             {file("install.txt", manifest("editplugin", "tool", "contents")),
                              file("contents/plugin.txt", "name=tool\r\nexecute=tool.exe\r\n"),
                              file("contents/oto.ini")}));
        InstallArchive archive;
        QVERIFY(archive.open(pathOf(archivePath())));
        QVERIFY(archive.setNameEncoding(QString()));
        QCOMPARE(archive.kind(), InstallArchive::Plugin);
        QVERIFY(archive.install(pathOf(voicePath()), QStringLiteral("tool"),
                                InstallArchive::Decoded, QStringLiteral("Shift_JIS")));
        QCOMPARE(listing(voicePath()), (QStringList{"tool", "tool/oto.ini", "tool/plugin.txt"}));
    }
};

QTEST_APPLESS_MAIN(test_InstallArchive)

#include "test_InstallArchive.moc"
