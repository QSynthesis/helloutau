#include <filesystem>
#include <future>
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
        // An extra field in the local header and in the central directory
        std::optional<std::pair<zip_uint16_t, QByteArray>> extraField;
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
                                                            ZIP_EM_TRAD_PKWARE, "password") != 0) ||
                (entry.extraField &&
                 zip_file_extra_field_set(
                     zip, zip_uint64_t(index), entry.extraField->first, ZIP_EXTRA_FIELD_NEW,
                     reinterpret_cast<const zip_uint8_t *>(entry.extraField->second.constData()),
                     zip_uint16_t(entry.extraField->second.size()),
                     ZIP_FL_LOCAL | ZIP_FL_CENTRAL) != 0)) {
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

    // Returns the CRC-32 of data, as the Info-ZIP Unicode Path field records that of the name
    quint32 crc32Of(QByteArrayView data) {
        quint32 crc = 0xFFFFFFFF;
        for (const auto c : data) {
            crc ^= quint8(c);
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1) ^ (0xEDB88320 & (0u - (crc & 1)));
            }
        }
        return ~crc;
    }

    // Returns the payload of an Info-ZIP Unicode Path field (0x7075) that gives utf8 as the name
    // of the entry whose name is original
    QByteArray unicodePathOf(const QByteArray &original, const QByteArray &utf8) {
        QByteArray payload(1, '\x01');
        const auto crc = crc32Of(original);
        for (int i = 0; i < 4; ++i) {
            payload.append(char((crc >> (8 * i)) & 0xFF));
        }
        return payload + utf8;
    }

    // An extra field ID that libzip does not interpret. libzip refuses to write 0x7075, so the
    // tests write this ID and replace it afterwards.
    constexpr zip_uint16_t placeholderField = 0x7070;

    // Replaces the ID of every extra field of placeholderField that holds payload with 0x7075, in
    // the local headers and in the central directory. Returns the number of fields replaced.
    int markUnicodePath(const QString &path, const QByteArray &payload) {
        QFile file(path);
        if (!file.open(QIODevice::ReadWrite)) {
            return 0;
        }
        auto data = file.readAll();
        QByteArray header("\x70\x70", 2);
        header.append(char(payload.size() & 0xFF));
        header.append(char(payload.size() >> 8));
        header += payload;
        int count = 0;
        // The ID is little-endian, so 0x7075 is stored as 75 70.
        for (auto i = data.indexOf(header); i >= 0; i = data.indexOf(header, i + 1)) {
            data[i] = '\x75';
            ++count;
        }
        file.seek(0);
        return file.write(data) == data.size() ? count : 0;
    }

    // Replaces every occurrence of the name from by the name to of the same length, in the local
    // headers and in the central directory, and returns the number of occurrences
    int renameEntries(const QString &path, const QByteArray &from, const QByteArray &to) {
        QFile file(path);
        if (from.size() != to.size() || !file.open(QIODevice::ReadWrite)) {
            return 0;
        }
        auto data = file.readAll();
        const auto count = int(data.count(from));
        data.replace(from, to);
        file.seek(0);
        return file.write(data) == data.size() ? count : 0;
    }

    // Sets the UTF-8 flag (bit 11) of every entry in its local header and in the central
    // directory, and returns the number of entries changed
    int markUtf8(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadWrite)) {
            return 0;
        }
        auto data = file.readAll();
        int count = 0;
        const std::pair<QByteArrayView, int> headers[] = {
            {QByteArrayView("PK\x03\x04", 4), 7},
            {QByteArrayView("PK\x01\x02", 4), 9},
        };
        for (const auto &[signature, offset] : headers) {
            for (auto i = data.indexOf(signature); i >= 0; i = data.indexOf(signature, i + 4)) {
                data[i + offset] = char(data[i + offset] | 0x08);
                ++count;
            }
        }
        file.seek(0);
        return file.write(data) == data.size() ? count / 2 : 0;
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

    // libzip replaces the name of an entry by the name of its Info-ZIP Unicode Path field if
    // the CRC-32 of the field matches the original name, so that the entry counts as marked as
    // UTF-8. A field whose CRC-32 does not match is ignored.
    void a_unicode_path_field_names_the_entry_in_utf8() {
        // U+3042 in Shift_JIS and in UTF-8
        const QByteArray original("\x82\xA0.txt");
        const QByteArray utf8("\xE3\x81\x82.txt");
        const auto payload = unicodePathOf(original, utf8);
        auto entry = file(original, "a");
        entry.encoding = ZIP_FL_ENC_CP437;
        entry.extraField = std::pair(placeholderField, payload);
        QVERIFY(writeArchive(archivePath(), {entry}));
        QCOMPARE(markUnicodePath(archivePath(), payload), 2);

        ZipExtractor extractor;
        const auto entries = extractor.listEntries(pathOf(archivePath()));
        QVERIFY(entries);
        QCOMPARE(entries->size(), 1);
        QCOMPARE(entries->front().name, utf8);
        QVERIFY(entries->front().utf8);
        QVERIFY(extract(extractor));
        QCOMPARE(listing(outPath()), QStringList({QString(QChar(0x3042)) + u".txt"}));

        const auto wrong = unicodePathOf(QByteArray("other.txt"), utf8);
        entry.extraField = std::pair(placeholderField, wrong);
        QVERIFY(writeArchive(archivePath(), {entry}));
        QCOMPARE(markUnicodePath(archivePath(), wrong), 2);
        const auto ignored = extractor.listEntries(pathOf(archivePath()));
        QVERIFY(ignored);
        QCOMPARE(ignored->front().name, original);
        QVERIFY(!ignored->front().utf8);
    }

    // A name marked as UTF-8 that is not valid UTF-8 makes libzip reject the archive.
    void an_invalid_name_marked_as_utf8_rejects_the_archive() {
        auto entry = file("\x82\xA0.txt", "a");
        entry.encoding = ZIP_FL_ENC_CP437;
        QVERIFY(writeArchive(archivePath(), {entry}));
        QCOMPARE(markUtf8(archivePath()), 1);
        ZipExtractor extractor;
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::OpenError);
        QVERIFY(!extractor.listEntries(pathOf(archivePath())));
        QVERIFY(!QFileInfo::exists(outPath()));
    }

    void names_equal_apart_from_case_reject_the_archive_data() {
        QTest::addColumn<QByteArray>("first");
        QTest::addColumn<QByteArray>("second");
        QTest::newRow("same name") << QByteArray("a.txt") << QByteArray("a.txt");
        QTest::newRow("other case") << QByteArray("a.txt") << QByteArray("A.TXT");
        QTest::newRow("in a folder") << QByteArray("dir/a.txt") << QByteArray("Dir/A.txt");
        QTest::newRow("file and folder") << QByteArray("name") << QByteArray("NAME/a.txt");
        QTest::newRow("folder and file") << QByteArray("name/a.txt") << QByteArray("Name");
    }

    void names_equal_apart_from_case_reject_the_archive() {
        QFETCH(QByteArray, first);
        QFETCH(QByteArray, second);
        if (first == second) {
            // libzip refuses to add a name twice, so the second name is replaced afterwards.
            QVERIFY(writeArchive(archivePath(), {file(first, "1"), file("b.txt", "2")}));
            QCOMPARE(renameEntries(archivePath(), "b.txt", second), 2);
        } else {
            QVERIFY(writeArchive(archivePath(), {file(first, "1"), file(second, "2")}));
        }
        ZipExtractor extractor;
        QVERIFY(!extract(extractor));
        // libzip itself rejects an archive with a name twice when it opens the archive.
        QCOMPARE(extractor.error(),
                 first == second ? ZipExtractor::OpenError : ZipExtractor::DuplicateName);
        QVERIFY(!QFileInfo::exists(outPath()));
    }

    // Folders of the same name apart from case are one folder, as on Windows.
    void folders_equal_apart_from_case_are_accepted() {
        QVERIFY(writeArchive(archivePath(),
                             {directory("dir/"), file("DIR/a.txt", "a"), file("Dir/b.txt", "b")}));
        ZipExtractor extractor;
        QVERIFY(extractor.check(pathOf(archivePath()), pathOf(outPath())));
    }

    // The entries are listed with their raw names, before any check.
    void the_entries_are_listed_raw() {
        auto unmarked = file("\x82\xA0.txt", "aa");
        unmarked.encoding = ZIP_FL_ENC_CP437;
        auto marked = file("\xE3\x81\x82.txt", "bbb");
        marked.encoding = ZIP_FL_ENC_UTF_8;
        QVERIFY(writeArchive(archivePath(), {file("a.txt", "a"), directory("sub/"), unmarked,
                                             marked, file("../evil.txt")}));
        ZipExtractor extractor;
        const auto entries = extractor.listEntries(pathOf(archivePath()));
        QVERIFY(entries);
        QCOMPARE(entries->size(), 5);
        QCOMPARE(entries->at(0).name, QByteArray("a.txt"));
        QVERIFY(entries->at(0).utf8);
        QCOMPARE(entries->at(0).size, 1);
        QVERIFY(entries->at(1).directory);
        QCOMPARE(entries->at(2).name, QByteArray("\x82\xA0.txt"));
        QVERIFY(!entries->at(2).utf8);
        QCOMPARE(entries->at(2).size, 2);
        QVERIFY(entries->at(3).utf8);
        QCOMPARE(entries->at(4).name, QByteArray("../evil.txt"));
        QCOMPARE(entries->at(4).index, 4);

        extractor.setMaximumEntryCount(4);
        QVERIFY(!extractor.listEntries(pathOf(archivePath())));
        QCOMPARE(extractor.error(), ZipExtractor::TooManyEntries);
    }

    // An entry is read within a limit, and a forged declared size does not raise the limit.
    void an_entry_is_read_within_a_limit() {
        QVERIFY(writeArchive(archivePath(), {file("a.txt", "alpha"), file("b.txt", "beta")}));
        ZipExtractor extractor;
        QCOMPARE(extractor.readEntry(pathOf(archivePath()), 1, 4),
                 std::optional(QByteArray("beta")));
        QVERIFY(!extractor.readEntry(pathOf(archivePath()), 0, 4));
        QCOMPARE(extractor.error(), ZipExtractor::TooLarge);
        QVERIFY(!extractor.readEntry(pathOf(archivePath()), 2, 100));
        QCOMPARE(extractor.error(), ZipExtractor::ReadError);

        QVERIFY(writeArchive(archivePath(), {file("bomb.txt", QByteArray(100000, 'x'))}));
        QCOMPARE(forgeSizes(archivePath(), 10), 1);
        QVERIFY(!extractor.readEntry(pathOf(archivePath()), 0, 1000));
        QCOMPARE(extractor.error(), ZipExtractor::TooLarge);
    }

    // Only the subdirectory is written, without the subdirectory itself. Its name is compared
    // apart from case. The entries outside it are checked as well.
    void a_subdirectory_is_extracted_alone() {
        QVERIFY(writeArchive(archivePath(),
                             {file("install.txt", "type=voiceset"), directory("contents/"),
                              file("contents/a.txt", "a"), file("contents/sub/b.txt", "b"),
                              file("contents-other/c.txt"), file("other/d.txt")}));
        ZipExtractor extractor;
        extractor.setSubdirectory(QStringLiteral("Contents"));
        QVERIFY(extract(extractor));
        QCOMPARE(listing(outPath()), QStringList({"a.txt", "sub", "sub/b.txt"}));

        QVERIFY(
            writeArchive(archivePath(), {file("contents/a.txt"), file("other/../../evil.txt")}));
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::UnsafePath);

        extractor.setSubdirectory(QStringLiteral("../contents"));
        QVERIFY(writeArchive(archivePath(), {file("contents/a.txt")}));
        QVERIFY(!extract(extractor));
        QCOMPARE(extractor.error(), ZipExtractor::UnsafePath);
    }

    // check() runs the checks of extract() and writes nothing.
    void check_writes_nothing() {
        QVERIFY(writeArchive(archivePath(), {file("a.txt", "a"), file("sub/b.txt", "b")}));
        ZipExtractor extractor;
        QVERIFY(extractor.check(pathOf(archivePath()), pathOf(outPath())));
        QCOMPARE(extractor.error(), ZipExtractor::NoError);
        QVERIFY(!QFileInfo::exists(outPath()));

        QVERIFY(writeArchive(archivePath(), {file("a.txt"), file("../evil.txt")}));
        QVERIFY(!extractor.check(pathOf(archivePath()), pathOf(outPath())));
        QCOMPARE(extractor.error(), ZipExtractor::UnsafePath);
        QVERIFY(!QFileInfo::exists(outPath()));
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

    void an_async_extraction_returns_the_result_of_extract() {
        QVERIFY(writeArchive(archivePath(), {file("a.txt", "alpha"), file("../evil.txt")}));
        ZipExtractor extractor;
        QVERIFY(!extractor.extractAsync(pathOf(archivePath()), pathOf(outPath())).get());
        QCOMPARE(extractor.error(), ZipExtractor::UnsafePath);

        QVERIFY(writeArchive(archivePath(), {file("a.txt", "alpha")}));
        QVERIFY(extractor.extractAsync(pathOf(archivePath()), pathOf(outPath())).get());
        QCOMPARE(extractor.error(), ZipExtractor::NoError);
        QCOMPARE(contentOf(outPath("a.txt")), QByteArray("alpha"));
    }

    void cancel_stops_an_async_extraction_while_writing() {
        // Larger than one block, so that the extraction reads the file in several blocks
        QVERIFY(writeArchive(archivePath(),
                             {file("a.txt", QByteArray(1 << 20, 'a')), file("b.txt", "beta")}));
        ZipExtractor extractor;
        std::promise<void> started;
        std::promise<void> canceled;
        auto resumed = canceled.get_future();
        int calls = 0;
        extractor.setProgressHandler([&](qint64, qint64) {
            if (++calls == 1) {
                started.set_value();
                resumed.wait();
            }
            return true;
        });
        auto result = extractor.extractAsync(pathOf(archivePath()), pathOf(outPath()));
        QCOMPARE(started.get_future().wait_for(std::chrono::seconds(30)),
                 std::future_status::ready);
        extractor.cancel();
        canceled.set_value();
        QVERIFY(!result.get());
        QCOMPARE(extractor.error(), ZipExtractor::Canceled);
        QCOMPARE(calls, 1);
        QVERIFY(!QFileInfo::exists(outPath("b.txt")));
    }

    void cancel_stops_an_async_extraction_while_checking() {
        // U+3042 and U+3044 in Shift_JIS, stored without the UTF-8 flag
        auto first = file("\x82\xA0.txt");
        first.encoding = ZIP_FL_ENC_CP437;
        auto second = file("\x82\xA2.txt");
        second.encoding = ZIP_FL_ENC_CP437;
        QVERIFY(writeArchive(archivePath(), {first, second}));
        ZipExtractor extractor;
        int calls = 0;
        extractor.setNameDecoder([&](QByteArrayView) -> std::optional<QString> {
            ++calls;
            extractor.cancel();
            return QStringLiteral("name.txt");
        });
        QVERIFY(!extractor.extractAsync(pathOf(archivePath()), pathOf(outPath())).get());
        QCOMPARE(extractor.error(), ZipExtractor::Canceled);
        QCOMPARE(calls, 1);
        QVERIFY(!QFileInfo::exists(outPath()));

        // Canceled during the check of the last entry
        QVERIFY(writeArchive(archivePath(), {first}));
        QVERIFY(!extractor.extractAsync(pathOf(archivePath()), pathOf(outPath())).get());
        QCOMPARE(extractor.error(), ZipExtractor::Canceled);
        QCOMPARE(calls, 2);
        QVERIFY(!QFileInfo::exists(outPath()));
    }

    void cancel_before_extract_is_discarded() {
        QVERIFY(writeArchive(archivePath(), {file("a.txt", "alpha")}));
        ZipExtractor extractor;
        extractor.cancel();
        QVERIFY(extract(extractor));
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
