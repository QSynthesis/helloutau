#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <hellokit/Support/SettingsFile.h>

using namespace hello::kit;

namespace json = stdc::json;

namespace {

    // Returns an object with the single entry \a name set to \a value.
    json::Value objectOf(const char *name, int value) {
        json::Object object;
        object.emplace(name, json::Value(value));
        return json::Value(std::move(object));
    }

    // Returns the entry "value" of the settings in \a fileName, or -1 if absent.
    int valueIn(const QString &fileName) {
        const auto object = SettingsFile::read(fileName);
        const auto it = object.find("value");
        return it == object.end() ? -1 : int(it->second.toInt());
    }

    void writeText(const QString &fileName, const QByteArray &text) {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(text);
    }

}

class test_SettingsFile : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void a_missing_file_reads_as_an_empty_object() {
        QVERIFY(SettingsFile::read(m_dir->filePath(QStringLiteral("absent.json"))).empty());
    }

    void a_file_without_a_json_object_is_reported_and_read_as_empty() {
        const auto array = m_dir->filePath(QStringLiteral("array.json"));
        writeText(array, "[1, 2]");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cannot be read")));
        QVERIFY(SettingsFile::read(array).empty());

        const auto broken = m_dir->filePath(QStringLiteral("broken.json"));
        writeText(broken, "{\"a\": ");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("cannot be read")));
        QVERIFY(SettingsFile::read(broken).empty());
    }

    // The changes of one pass of the event loop result in one write, of the content at the time
    // of the write.
    void the_changes_of_one_pass_are_written_once() {
        const auto fileName = m_dir->filePath(QStringLiteral("settings.json"));
        int writes = 0;
        int value = 1;
        SettingsFile file(fileName, [&] {
            ++writes;
            return objectOf("value", value);
        });

        file.syncLater();
        value = 2;
        file.syncLater();
        QCOMPARE(writes, 0);
        QVERIFY(!QFile::exists(fileName));

        QCoreApplication::processEvents();
        QCOMPARE(writes, 1);
        QCOMPARE(valueIn(fileName), 2);

        QCoreApplication::processEvents();
        QCOMPARE(writes, 1);
    }

    void sync_writes_the_pending_changes_at_once() {
        const auto fileName = m_dir->filePath(QStringLiteral("settings.json"));
        int writes = 0;
        SettingsFile file(fileName, [&] {
            ++writes;
            return objectOf("value", 3);
        });

        // Nothing is pending yet
        file.sync();
        QCOMPARE(writes, 0);

        file.syncLater();
        file.sync();
        QCOMPARE(writes, 1);
        QCOMPARE(valueIn(fileName), 3);

        // The pending write has been done, and the event loop does not repeat it.
        QCoreApplication::processEvents();
        QCOMPARE(writes, 1);
    }

    void destruction_writes_the_pending_changes() {
        const auto fileName = m_dir->filePath(QStringLiteral("settings.json"));
        {
            SettingsFile file(fileName, [] { return objectOf("value", 4); });
            file.syncLater();
        }
        QCOMPARE(valueIn(fileName), 4);
    }

    void a_missing_directory_is_created() {
        const auto fileName = m_dir->filePath(QStringLiteral("a/b/settings.json"));
        SettingsFile file(fileName, [] { return objectOf("value", 5); });
        file.syncLater();
        file.sync();
        QCOMPARE(valueIn(fileName), 5);
    }

    void a_failed_write_is_reported() {
        // A directory of that name prevents the file from being written.
        const auto fileName = m_dir->filePath(QStringLiteral("taken"));
        QVERIFY(QDir(m_dir->path()).mkdir(QStringLiteral("taken")));
        SettingsFile file(fileName, [] { return objectOf("value", 6); });
        file.syncLater();
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("cannot be written to")));
        file.sync();
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
};

QTEST_GUILESS_MAIN(test_SettingsFile)

#include "test_SettingsFile.moc"
