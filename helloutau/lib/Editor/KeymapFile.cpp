#include "KeymapFile.h"

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
        constexpr char ModifiersKey[] = "modifiers";

    }

    QString KeymapFile::fileNameFor(const QString &settingsFile) {
        return QFileInfo(settingsFile).dir().filePath(QStringLiteral("keymap.json"));
    }

    void KeymapFile::read(Sections &sections, const QString &fileName) {
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
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qWarning("Keymap: %s is not a keymap and is ignored.", qPrintable(fileName));
            return;
        }
        const auto root = document.object();
        for (auto &section : sections) {
            const auto value = root.value(section.key);
            if (value.isUndefined()) {
                continue;
            }
            const auto shortcuts = value.toObject().value(QLatin1String(ShortcutsKey));
            if (!shortcuts.isArray()) {
                qWarning("Keymap: the section %s of %s is not a keymap and is ignored.",
                         qPrintable(section.key), qPrintable(fileName));
                continue;
            }
            section.registry->setShortcutsFamily(
                QAK::ActionFamily::shortcutsFamilyFromJson(shortcuts.toArray()));
            const auto modifiers = value.toObject().value(QLatin1String(ModifiersKey)).toObject();
            for (auto &bindings : section.modifiers) {
                bindings.readJson(modifiers.value(bindings.scheme().key()).toObject());
            }
        }
    }

    bool KeymapFile::write(const Sections &sections, const QString &fileName, QString *error) {
        QJsonObject object;
        for (const auto &section : sections) {
            // Only the commands that the user has assigned are written.
            QAK::ActionFamily::ShortcutsFamily assigned;
            const auto family = section.registry->shortcutsFamily();
            for (auto it = family.begin(); it != family.end(); ++it) {
                if (it.value()) {
                    assigned.insert(it.key(), it.value());
                }
            }
            QJsonObject value;
            value.insert(QLatin1String(ShortcutsKey),
                         QAK::ActionFamily::shortcutsFamilyToJson(assigned));
            QJsonObject modifiers;
            for (const auto &bindings : section.modifiers) {
                if (const auto roles = bindings.toJson(); !roles.isEmpty()) {
                    modifiers.insert(bindings.scheme().key(), roles);
                }
            }
            if (!modifiers.isEmpty()) {
                value.insert(QLatin1String(ModifiersKey), modifiers);
            }
            object.insert(section.key, value);
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
