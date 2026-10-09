#include "ActionLayoutsFile.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

#include <QAKCore/actionregistry.h>

namespace hello::daw {

    namespace {

        constexpr char ChangesKey[] = "changes";
        constexpr char VersionKey[] = "version";

    }

    QString ActionLayoutsFile::fileNameFor(const QString &settingsFile) {
        return QFileInfo(settingsFile).dir().filePath(QStringLiteral("actionLayouts.json"));
    }

    void ActionLayoutsFile::read(const Sections &sections, const QString &fileName) {
        QFile file(fileName);
        if (!file.exists()) {
            return;
        }
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning("Layouts: %s could not be read.", qPrintable(fileName));
            return;
        }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qWarning("Layouts: %s is not a list of changes and is ignored.", qPrintable(fileName));
            return;
        }
        const auto root = document.object();
        if (const auto found = root.value(QLatin1String(VersionKey)); found.toInt(-1) != version) {
            const auto text =
                found.isDouble() ? QString::number(found.toDouble()) : QStringLiteral("none");
            qWarning("Layouts: %s has the version %s instead of %d and is ignored.",
                     qPrintable(fileName), qPrintable(text), version);
            return;
        }
        for (const auto &[key, registry] : sections) {
            const auto section = root.value(key);
            if (section.isUndefined()) {
                continue;
            }
            const auto changes = section.toObject().value(QLatin1String(ChangesKey));
            if (!changes.isArray()) {
                qWarning("Layouts: the section %s of %s is not a list of changes and is ignored.",
                         qPrintable(key), qPrintable(fileName));
                continue;
            }
            QVector<QAK::ActionLayoutChange> read;
            for (const auto &value : changes.toArray()) {
                if (const auto change = QAK::ActionLayoutChange::fromJsonObject(value.toObject())) {
                    read.push_back(*change);
                } else {
                    qWarning("Layouts: a change of %s does not read and is skipped.",
                             qPrintable(fileName));
                }
            }
            registry->setLayoutChanges(read);
        }
    }

    bool ActionLayoutsFile::write(const Sections &sections, const QString &fileName,
                                  QString *error) {
        QJsonObject object;
        object.insert(QLatin1String(VersionKey), version);
        for (const auto &[key, registry] : sections) {
            QJsonArray changes;
            for (const auto &change : registry->layoutChanges()) {
                changes.push_back(change.toJsonObject());
            }
            QJsonObject section;
            section.insert(QLatin1String(ChangesKey), changes);
            object.insert(key, section);
        }

        QDir().mkpath(QFileInfo(fileName).absolutePath());
        QSaveFile file(fileName);
        if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(object).toJson()) < 0 ||
            !file.commit()) {
            if (error) {
                *error = file.errorString();
            }
            return false;
        }
        return true;
    }

}
