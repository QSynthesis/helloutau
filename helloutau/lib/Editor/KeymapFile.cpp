#include "KeymapFile_p.h"

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

        constexpr char ShortcutsKey[] = "shortcuts";

    }

    QString KeymapFile::fileNameFor(const QString &settingsFile) {
        return QFileInfo(settingsFile).dir().filePath(QStringLiteral("keymap.json"));
    }

    void KeymapFile::read(QAK::ActionRegistry *registry, const QString &fileName) {
        QFile file(fileName);
        if (!file.exists()) {
            return;
        }
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning("Keymap: %s could not be read.", qPrintable(fileName));
            return;
        }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        const auto shortcuts = document.object().value(QLatin1String(ShortcutsKey));
        if (error.error != QJsonParseError::NoError || !shortcuts.isArray()) {
            qWarning("Keymap: %s is not a keymap and is ignored.", qPrintable(fileName));
            return;
        }
        registry->setShortcutsFamily(
            QAK::ActionFamily::shortcutsFamilyFromJson(shortcuts.toArray()));
    }

    bool KeymapFile::write(const QAK::ActionRegistry *registry, const QString &fileName,
                           QString *error) {
        // Only the commands that the user has assigned are written.
        QAK::ActionFamily::ShortcutsFamily assigned;
        const auto family = registry->shortcutsFamily();
        for (auto it = family.begin(); it != family.end(); ++it) {
            if (it.value()) {
                assigned.insert(it.key(), it.value());
            }
        }
        QJsonObject object;
        object.insert(QLatin1String(ShortcutsKey),
                      QAK::ActionFamily::shortcutsFamilyToJson(assigned));

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
