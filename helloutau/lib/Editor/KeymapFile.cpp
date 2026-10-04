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
        constexpr char ModifiersKey[] = "modifiers";

        QJsonArray modifiersToJson(Qt::KeyboardModifiers modifiers) {
            QJsonArray result;
            const auto add = [&](Qt::KeyboardModifier modifier, const char *name) {
                if (modifiers & modifier) {
                    result.push_back(QLatin1String(name));
                }
            };
            add(Qt::ControlModifier, "Ctrl");
            add(Qt::AltModifier, "Alt");
            add(Qt::ShiftModifier, "Shift");
            add(Qt::MetaModifier, "Meta");
            return result;
        }

        Qt::KeyboardModifiers modifiersFromJson(const QJsonValue &value) {
            if (!value.isArray()) {
                return Qt::NoModifier;
            }
            Qt::KeyboardModifiers result = Qt::NoModifier;
            for (const auto &part : value.toArray()) {
                const auto name = part.toString();
                if (name == QLatin1String("Ctrl")) {
                    result |= Qt::ControlModifier;
                } else if (name == QLatin1String("Alt")) {
                    result |= Qt::AltModifier;
                } else if (name == QLatin1String("Shift")) {
                    result |= Qt::ShiftModifier;
                } else if (name == QLatin1String("Meta")) {
                    result |= Qt::MetaModifier;
                }
            }
            return result;
        }

        void readModifier(const QJsonObject &object, const char *key,
                          Qt::KeyboardModifiers &target) {
            const auto value = object.value(QLatin1String(key));
            if (value.isArray()) {
                target = modifiersFromJson(value);
            }
        }

        QJsonObject modifiersToJson(const EditorModifierBindings &bindings) {
            QJsonObject object;
            object.insert("horizontalScroll", modifiersToJson(bindings.horizontalScroll));
            object.insert("timeZoom", modifiersToJson(bindings.timeZoom));
            object.insert("keyZoom", modifiersToJson(bindings.keyZoom));
            object.insert("dragZoom", modifiersToJson(bindings.dragZoom));
            object.insert("dragZoomAxisLock", modifiersToJson(bindings.dragZoomAxisLock));
            object.insert("disableNoteSnap", modifiersToJson(bindings.disableNoteSnap));
            object.insert("lockParameterTime", modifiersToJson(bindings.lockParameterTime));
            object.insert("snapParameterValue", modifiersToJson(bindings.snapParameterValue));
            return object;
        }

        void readModifiers(const QJsonObject &object, EditorModifierBindings &bindings) {
            readModifier(object, "horizontalScroll", bindings.horizontalScroll);
            readModifier(object, "timeZoom", bindings.timeZoom);
            readModifier(object, "keyZoom", bindings.keyZoom);
            readModifier(object, "dragZoom", bindings.dragZoom);
            readModifier(object, "dragZoomAxisLock", bindings.dragZoomAxisLock);
            readModifier(object, "disableNoteSnap", bindings.disableNoteSnap);
            readModifier(object, "lockParameterTime", bindings.lockParameterTime);
            readModifier(object, "snapParameterValue", bindings.snapParameterValue);
        }

    }

    QString KeymapFile::fileNameFor(const QString &settingsFile) {
        return QFileInfo(settingsFile).dir().filePath(QStringLiteral("keymap.json"));
    }

    void KeymapFile::read(const Sections &sections, const QString &fileName,
                          EditorModifierBindings *modifiers) {
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
        for (const auto &[key, registry] : sections) {
            const auto section = root.value(key);
            if (section.isUndefined()) {
                continue;
            }
            const auto shortcuts = section.toObject().value(QLatin1String(ShortcutsKey));
            if (!shortcuts.isArray()) {
                qWarning("Keymap: the section %s of %s is not a keymap and is ignored.",
                         qPrintable(key), qPrintable(fileName));
                continue;
            }
            registry->setShortcutsFamily(
                QAK::ActionFamily::shortcutsFamilyFromJson(shortcuts.toArray()));
            if (modifiers && key == QLatin1String("projectWindow")) {
                const auto value = section.toObject().value(QLatin1String(ModifiersKey));
                if (value.isObject()) {
                    readModifiers(value.toObject(), *modifiers);
                }
            }
        }
    }

    bool KeymapFile::write(const Sections &sections, const QString &fileName, QString *error,
                           const EditorModifierBindings *modifiers) {
        QJsonObject object;
        for (const auto &[key, registry] : sections) {
            // Only the commands that the user has assigned are written.
            QAK::ActionFamily::ShortcutsFamily assigned;
            const auto family = registry->shortcutsFamily();
            for (auto it = family.begin(); it != family.end(); ++it) {
                if (it.value()) {
                    assigned.insert(it.key(), it.value());
                }
            }
            QJsonObject section;
            section.insert(QLatin1String(ShortcutsKey),
                           QAK::ActionFamily::shortcutsFamilyToJson(assigned));
            if (modifiers && key == QLatin1String("projectWindow") &&
                *modifiers != EditorModifierBindings{}) {
                section.insert(QLatin1String(ModifiersKey), modifiersToJson(*modifiers));
            }
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
