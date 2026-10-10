#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <QAKCore/actionregistry.h>

#include <helloutau/Editor/ActionLayoutsFile.h>

using namespace hello::daw;

namespace {

    QVector<QAK::ActionLayoutChange> someChanges() {
        QAK::ActionLayoutChange remove;
        remove.kind = QAK::ActionLayoutChange::Remove;
        remove.container = QStringLiteral("helloutau.menu.edit");
        remove.entry = QAK::ActionLayoutEntry(QStringLiteral("helloutau.edit.cut"));
        QAK::ActionLayoutChange add;
        add.container = QStringLiteral("helloutau.menu.file");
        add.entry = QAK::ActionLayoutEntry(QStringLiteral("helloutau.edit.cut"));
        add.anchor = QAK::ActionInsertion::After;
        add.relativeTo = QStringLiteral("helloutau.file.save");
        add.moved = true;
        return {remove, add};
    }

    void writeText(const QString &fileName, const QByteArray &text) {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(text);
    }

}

class test_ActionLayoutsFile : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    QString fileName() const {
        return m_dir->filePath(QStringLiteral("actionLayouts.json"));
    }

    // The changes that reading the file gives a fresh registry
    QVector<QAK::ActionLayoutChange> readBack() const {
        QAK::ActionRegistry registry;
        ActionLayoutsFile::read(
            {
                {QStringLiteral("projectWindow"), &registry}
        },
            fileName());
        return registry.layoutChanges();
    }

private Q_SLOTS:
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
    }

    void the_changes_survive_a_round_trip() {
        QAK::ActionRegistry registry;
        registry.setLayoutChanges(someChanges());
        QString error;
        QVERIFY2(ActionLayoutsFile::write(
                     {
                         {QStringLiteral("projectWindow"), &registry}
        },
                     fileName(), &error),
                 qPrintable(error));
        QCOMPARE(readBack(), someChanges());

        QFile file(fileName());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll())
                     .object()
                     .value(QStringLiteral("version"))
                     .toInt(),
                 ActionLayoutsFile::version);
    }

    void a_missing_file_records_no_changes() {
        QVERIFY(readBack().isEmpty());
    }

    // A file that is not a list of changes is ignored with a warning, and a change that does
    // not read is skipped with a warning.
    void malformed_content_is_ignored() {
        const auto version = QByteArray::number(ActionLayoutsFile::version);
        writeText(fileName(), "not json");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral(".")));
        QVERIFY(readBack().isEmpty());

        writeText(fileName(), "{\"version\": " + version + ", \"projectWindow\": {}}");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("not a list")));
        QVERIFY(readBack().isEmpty());

        writeText(fileName(), "{\"version\": " + version +
                                  ", \"projectWindow\": {\"changes\": [{\"kind\": \"bogus\"}]}}");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("skipped")));
        QVERIFY(readBack().isEmpty());
    }

    // A file of another version, or without a version, is not read.
    void a_file_of_another_version_is_ignored() {
        QAK::ActionRegistry registry;
        registry.setLayoutChanges(someChanges());
        QVERIFY(ActionLayoutsFile::write(
            {
                {QStringLiteral("projectWindow"), &registry}
        },
            fileName(), nullptr));
        QFile file(fileName());
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();

        for (const QJsonValue &version :
             {QJsonValue(QJsonValue::Undefined), QJsonValue(ActionLayoutsFile::version - 1),
              QJsonValue(ActionLayoutsFile::version + 1)}) {
            auto changed = root;
            if (version.isUndefined()) {
                changed.remove(QStringLiteral("version"));
            } else {
                changed.insert(QStringLiteral("version"), version);
            }
            writeText(fileName(), QJsonDocument(changed).toJson());
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("instead of")));
            QVERIFY(readBack().isEmpty());
        }
    }
};

QTEST_GUILESS_MAIN(test_ActionLayoutsFile)

#include "test_ActionLayoutsFile.moc"
