#include "ActionLayoutsFile_p.h"

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

    }

    QString ActionLayoutsFile::fileNameFor(const QString &settingsFile) {
        return QFileInfo(settingsFile).dir().filePath(QStringLiteral("actionLayouts.json"));
    }

    void ActionLayoutsFile::read(QAK::ActionRegistry *registry, const QString &fileName) {
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
        const auto changes = document.object().value(QLatin1String(ChangesKey));
        if (error.error != QJsonParseError::NoError || !changes.isArray()) {
            qWarning("Layouts: %s is not a list of changes and is ignored.", qPrintable(fileName));
            return;
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

    bool ActionLayoutsFile::write(const QAK::ActionRegistry *registry, const QString &fileName,
                            QString *error) {
        QJsonArray changes;
        for (const auto &change : registry->layoutChanges()) {
            changes.push_back(change.toJsonObject());
        }
        QJsonObject object;
        object.insert(QLatin1String(ChangesKey), changes);

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
