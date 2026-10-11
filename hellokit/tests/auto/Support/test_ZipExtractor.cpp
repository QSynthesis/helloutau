#include <filesystem>
#include <optional>
#include <system_error>

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <zip.h>

#include <hellokit/Support/ZipExtractor.h>

using namespace hello::kit;

namespace fs = std::filesystem;

namespace {

    struct TestEntry {
        QByteArray name;
        QByteArray data;
        zip_flags_t encoding = ZIP_FL_ENC_GUESS;
        bool directory = false;
        std::optional<zip_uint32_t> unixMode;
        bool encrypted = false;
    };

    TestEntry file(const QByteArray &name, const QByteArray &data = {}) {
        TestEntry entry;
        entry.name = name;
        entry.data = data;
        return entry;
    }

    TestEntry directory(const QByteArray &name) {
        TestEntry entry;
        entry.name = name;
        entry.directory = true;
        return entry;
    }

    // Writes the entries as libzip stores them, without any check of the names.
    bool writeArchive(const QString &path, const QList<TestEntry> &entries) {
        int error = 0;
        const auto zip =
            zip_open(QFile::encodeName(path).constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);
        if (!zip) {
            return false;
        }
        for (const auto &entry : entries) {
            zip_int64_t index = -1;
            if (entry.directory) {
                index = zip_dir_add(zip, entry.name.constData(), entry.encoding);
            } else if (const auto source = zip_source_buffer(zip, entry.data.constData(),
                                                             zip_uint64_t(entry.data.size()), 0)) {
                index = zip_file_add(zip, entry.name.constData(), source, entry.encoding);
                if (index < 0) {
                    zip_source_free(source);
                }
            }
            if (index < 0 ||
                (entry.unixMode &&
                 zip_file_set_external_attributes(zip, zip_uint64_t(index), 0, ZIP_OPSYS_UNIX,
                                                  *entry.unixMode << 16) != 0) ||
                (entry.encrypted && zip_file_set_encryption(zip, zip_uint64_t(index),
                                                            ZIP_EM_TRAD_PKWARE, "password") != 0)) {
                zip_discard(zip);
                return false;
            }
        }
        return zip_close(zip) == 0;
    }

    fs::path pathOf(const QString &path) {
        return fs::path(path.toStdU16String());
    }

    QByteArray contentOf(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
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

    // Replaces the uncompressed size of every entry in its local header and in the central
    // directory, so that both copies agree, and returns the number of entries changed. The
    // compressed data of the test archives does not contain the signatures.
    int forgeSizes(const QString &path, quint32 size) {
        QFile file(path);
        if (!file.open(QIODevice::ReadWrite)) {
            return 0;
        }
        auto data = file.readAll();
        QList<qsizetype> offsets;
        for (auto i = data.indexOf(QByteArrayView("PK\x03\x04", 4)); i >= 0;
             i = data.indexOf(QByteArrayView("PK\x03\x04", 4), i + 4)) {
            offsets.append(i + 22);
        }
        const auto localCount = offsets.size();
        for (auto i = data.indexOf(QByteArrayView("PK\x01\x02", 4)); i >= 0;
             i = data.indexOf(QByteArrayView("PK\x01\x02", 4), i + 4)) {
            offsets.append(i + 24);
        }
        if (offsets.size() != 2 * localCount) {
            return 0;
        }
        for (const auto offset : offsets) {
            for (int i = 0; i < 4; ++i) {
                data[offset + i] = char((size >> (8 * i)) & 0xFF);
            }
        }
        file.seek(0);
        return file.write(data) == data.size() ? int(localCount) : 0;
    }

}

class test_ZipExtractor : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;

    QString archivePath() const {
        return m_root.filePath(QStringLiteral("archive.zip"));
    }

    QString outPath() const {
        return m_root.filePath(QStringLiteral("out"));
    }

    QString outPath(const QString &name) const {
        return m_root.filePath(QStringLiteral("out/") + name);
    }

    bool extract(ZipExtractor &extractor) const {
        return extractor.extract(pathOf(archivePath()), pathOf(outPath()));
    }

private Q_SLOTS:
    void init() {
        QVERIFY(m_root.isValid());
        // QDir::removeRecursively() cannot remove a symbolic link to a directory on Windows.
        // std::filesystem::remove_all() removes the link without following it.
        std::error_code ec;
        fs::remove_all(pathOf(outPath()), ec);
        QVERIFY2(!ec, ec.message().c_str());
        QFile::remove(m_root.filePath(QStringLiteral("evil.txt")));
    }

    void files_and_directories_are_extracted() {
        QVERIFY(writeArchive(archivePath(), {file("a.txt", "alpha"), file("sub/b.txt", "beta"),
                                             directory("empty"), file("sub/deeper/c.txt")}));
        ZipExtractor extractor;
        QVERIFY(extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::NoError);
        QVERIFY(extractor.errorString().isEmpty());
        QCOMPARE(listing(outPath()), QStringList({"a.txt", "empty", "sub", "sub/b.txt",
                                                  "sub/deeper", "sub/deeper/c.txt"}));
        QCOMPARE(contentOf(outPath("a.txt")), QByteArray("alpha"));
        QCOMPARE(contentOf(outPath("sub/b.txt")), QByteArray("beta"));
        QCOMPARE(QFileInfo(outPath("sub/deeper/c.txt")).size(), 0);
    }

    void an_existing_file_is_replaced() {
        QVERIFY(QDir().mkpath(outPath()));
        QFile old(outPath("a.txt"));
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write("an older and longer content");
        old.close();
        QVERIFY(writeArchive(archivePath(), {file("a.txt", "alpha")}));
        ZipExtractor extractor;
        QVERIFY(extract(extractor));
        QCOMPARE(contentOf(outPath("a.txt")), QByteArray("alpha"));
    }

    void the_single_top_directory_is_stripped() {
        QVERIFY(writeArchive(archivePath(), {directory("pkg"), file("pkg/bin/app.txt", "app"),
                                             file("pkg/readme.txt", "readme")}));
        ZipExtractor extractor;
        extractor.setTopDirectoryStripped(true);
        QVERIFY(extract(extractor));
        QCOMPARE(listing(outPath()), QStringList({"bin", "bin/app.txt", "readme.txt"}));
    }

    void a_top_directory_beside_another_entry_is_kept() {
        QVERIFY(writeArchive(archivePath(), {file("pkg/a.txt"), file("other.txt")}));
        ZipExtractor extractor;
        extractor.setTopDirectoryStripped(true);
        QVERIFY(extract(extractor));
        QCOMPARE(listing(outPath()), QStringList({"other.txt", "pkg", "pkg/a.txt"}));
    }

    void an_unsafe_path_rejects_the_whole_archive_data() {
        QTest::addColumn<QByteArray>("name");
        QTest::newRow("parent") << QByteArray("../evil.txt");
        QTest::newRow("nested parent") << QByteArray("a/../../evil.txt");
        QTest::newRow("backslash parent") << QByteArray("a\\..\\..\\evil.txt");
        QTest::newRow("trailing parent") << QByteArray("a/..");
        QTest::newRow("inner parent") << QByteArray("a/../b.txt");
        QTest::newRow("absolute") << QByteArray("/evil.txt");
        QTest::newRow("backslash absolute") << QByteArray("\\evil.txt");
        QTest::newRow("UNC") << QByteArray("//server/share/evil.txt");
        QTest::newRow("drive") << QByteArray("C:/evil.txt");
        QTest::newRow("drive relative") << QByteArray("C:evil.txt");
        QTest::newRow("stream") << QByteArray("a/b.txt:stream");
        QTest::newRow("empty component") << QByteArray("a//b.txt");
        QTest::newRow("current") << QByteArray("./a.txt");
        QTest::newRow("control character") << QByteArray("a\nb.txt");
    }

    void an_unsafe_path_rejects_the_whole_archive() {
        QFETCH(QByteArray, name);
        QVERIFY(writeArchive(archivePath(), {file("first.txt", "1"), file(name, "evil")}));
        ZipExtractor extractor;
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::UnsafePath);
        QVERIFY(!extractor.errorString().isEmpty());
        QVERIFY(!QFileInfo::exists(outPath()));
        QVERIFY(!QFileInfo::exists(m_root.filePath(QStringLiteral("evil.txt"))));
    }

    void an_existing_link_out_of_the_directory_rejects_the_archive() {
        const auto outside = m_root.filePath(QStringLiteral("outside"));
        QVERIFY(QDir().mkpath(outside));
        QVERIFY(QDir().mkpath(outPath()));
        std::error_code ec;
        fs::create_directory_symlink(pathOf(outside), pathOf(outPath("link")), ec);
        if (ec) {
            QSKIP("Symbolic links cannot be created by this account.");
        }
        QVERIFY(writeArchive(archivePath(), {file("link/evil.txt", "evil")}));
        ZipExtractor extractor;
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::UnsafePath);
        QVERIFY(!QFileInfo::exists(outside + QStringLiteral("/evil.txt")));
    }

    void a_name_reserved_by_windows_rejects_the_archive_data() {
        QTest::addColumn<QByteArray>("name");
        QTest::newRow("device") << QByteArray("CON");
        QTest::newRow("device with extension") << QByteArray("a/nul.txt");
        QTest::newRow("numbered device") << QByteArray("com1.log");
        QTest::newRow("superscript device") << QByteArray("LPT\xC2\xB9");
        QTest::newRow("device with space") << QByteArray("aux .txt");
        QTest::newRow("trailing dot") << QByteArray("a./b.txt");
        QTest::newRow("trailing space") << QByteArray("a /b.txt");
    }

    void a_name_reserved_by_windows_rejects_the_archive() {
#ifndef Q_OS_WIN
        QSKIP("The names are reserved only on Windows.");
#endif
        QFETCH(QByteArray, name);
        auto entry = file(name, "evil");
        entry.encoding = ZIP_FL_ENC_UTF_8;
        QVERIFY(writeArchive(archivePath(), {entry}));
        ZipExtractor extractor;
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::UnsafePath);
        QVERIFY(!QFileInfo::exists(outPath()));
    }

    void a_symbolic_link_rejects_the_archive() {
        auto link = file("link", "../evil.txt");
        link.unixMode = 0120777;
        QVERIFY(writeArchive(archivePath(), {file("first.txt", "1"), link}));
        ZipExtractor extractor;
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::SymbolicLink);
        QVERIFY(!QFileInfo::exists(outPath()));
    }

    void an_encrypted_entry_rejects_the_archive() {
        auto entry = file("secret.txt", "secret");
        entry.encrypted = true;
        QVERIFY(writeArchive(archivePath(), {entry}));
        ZipExtractor extractor;
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::EncryptedEntry);
        QVERIFY(!QFileInfo::exists(outPath()));
    }

    void a_name_marked_as_utf8_is_decoded() {
        // U+3042 and U+3044
        auto entry = file("\xE3\x81\x82/\xE3\x81\x84.txt", "a");
        entry.encoding = ZIP_FL_ENC_UTF_8;
        QVERIFY(writeArchive(archivePath(), {entry}));
        ZipExtractor extractor;
        QVERIFY(extract(extractor));
        QCOMPARE(listing(outPath()),
                 QStringList({QString(QChar(0x3042)),
                              QString(QChar(0x3042)) + u'/' + QChar(0x3044) + u".txt"}));
    }

    void an_unmarked_name_requires_the_decoder() {
        // U+3042 in Shift_JIS, stored without the UTF-8 flag
        const QByteArray raw("\x82\xA0.txt");
        auto entry = file(raw, "a");
        entry.encoding = ZIP_FL_ENC_CP437;
        QVERIFY(writeArchive(archivePath(), {entry}));

        ZipExtractor extractor;
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::UndecodableName);
        QVERIFY(!QFileInfo::exists(outPath()));

        extractor.setNameDecoder([](QByteArrayView) { return std::optional<QString>(); });
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::UndecodableName);

        const auto decoded = QString(QChar(0x3042)) + u".txt";
        extractor.setNameDecoder([&](QByteArrayView name) -> std::optional<QString> {
            if (name == raw) {
                return decoded;
            }
            return std::nullopt;
        });
        QVERIFY(extract(extractor));
        QCOMPARE(listing(outPath()), QStringList({decoded}));
    }

    void the_number_of_entries_is_limited() {
        QVERIFY(writeArchive(archivePath(), {file("a.txt"), file("b.txt"), file("c.txt")}));
        ZipExtractor extractor;
        extractor.setMaximumEntryCount(2);
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::TooManyEntries);
        QVERIFY(!QFileInfo::exists(outPath()));
        extractor.setMaximumEntryCount(3);
        QVERIFY(extract(extractor));
    }

    void the_declared_size_is_limited() {
        QVERIFY(writeArchive(archivePath(), {file("a.txt", QByteArray(60, 'a')),
                                             file("b.txt", QByteArray(40, 'b'))}));
        ZipExtractor extractor;
        extractor.setMaximumSize(99);
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::TooLarge);
        QVERIFY(!QFileInfo::exists(outPath()));
        extractor.setMaximumSize(100);
        QVERIFY(extract(extractor));
    }

    void the_written_size_is_limited_if_the_declared_size_is_forged_data() {
        QTest::addColumn<int>("threads");
        QTest::newRow("one thread") << 1;
        QTest::newRow("four threads") << 4;
    }

    void the_written_size_is_limited_if_the_declared_size_is_forged() {
        QFETCH(int, threads);
        QList<TestEntry> entries;
        for (char c = 'a'; c < 'a' + 4; ++c) {
            entries.append(file(QByteArray("bomb-") + c + ".txt", QByteArray(100000, c)));
        }
        QVERIFY(writeArchive(archivePath(), entries));
        QCOMPARE(forgeSizes(archivePath(), 10), 4);
        ZipExtractor extractor;
        extractor.setMaximumSize(1000);
        extractor.setThreadCount(threads);
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::TooLarge);
        qint64 size = 0;
        for (const auto &name : listing(outPath())) {
            size += QFileInfo(outPath(name)).size();
        }
        QVERIFY(size <= 1000);
    }

    void several_threads_extract_every_file() {
        QList<TestEntry> entries;
        for (int i = 0; i < 40; ++i) {
            const auto name = QByteArray("dir") + QByteArray::number(i % 3) + "/file" +
                              QByteArray::number(i) + ".txt";
            entries.append(file(name, QByteArray(1000 * i, char('a' + i % 26))));
        }
        QVERIFY(writeArchive(archivePath(), entries));
        ZipExtractor extractor;
        extractor.setThreadCount(4);
        qint64 reported = 0;
        extractor.setProgressHandler([&](qint64 written, qint64 total) {
            reported = written;
            return total == 780000;
        });
        QVERIFY(extract(extractor));
        QCOMPARE(reported, 780000);
        for (const auto &entry : entries) {
            QCOMPARE(contentOf(outPath(QString::fromLatin1(entry.name))), entry.data);
        }
    }

    void the_progress_handler_cancels_the_extraction() {
        QVERIFY(writeArchive(archivePath(), {file("a.txt", "alpha"), file("b.txt", "beta")}));
        ZipExtractor extractor;
        QList<std::pair<qint64, qint64>> reports;
        extractor.setProgressHandler([&](qint64 written, qint64 total) {
            reports.append({written, total});
            return false;
        });
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::Canceled);
        QCOMPARE(reports, (QList<std::pair<qint64, qint64>>{
                              {5, 9}
        }));
        QVERIFY(!QFileInfo::exists(outPath("b.txt")));
    }

    void a_file_that_is_not_an_archive_fails_to_open() {
        QFile archive(archivePath());
        QVERIFY(archive.open(QIODevice::WriteOnly));
        archive.write("not a zip archive");
        archive.close();
        ZipExtractor extractor;
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::OpenError);
        QVERIFY(!QFileInfo::exists(outPath()));
    }

    void the_permissions_of_a_unix_entry_are_applied() {
#ifdef Q_OS_WIN
        QSKIP("Windows has no permission bits.");
#else
        auto program = file("program", "#!/bin/sh\n");
        program.unixMode = 0104755;
        QVERIFY(writeArchive(archivePath(), {program}));
        ZipExtractor extractor;
        QVERIFY(extract(extractor));
        const auto permissions = fs::status(pathOf(outPath("program"))).permissions();
        QCOMPARE(permissions, fs::perms(0755));
#endif
    }
};

QTEST_APPLESS_MAIN(test_ZipExtractor)

#include "test_ZipExtractor.moc"
