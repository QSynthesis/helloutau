#include "ClassicPluginPreviewInstallation.h"

#include <optional>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

#include <stdutau/plugintxt.h>
#include <stdutau/utaconst.h>

#include <hellokit/Support/TextCodec.h>

namespace hello::daw {

    namespace {

        // The record is at hostData/ClassicPluginHost/playinplugin of plugin.json (docs/note.md).
        const QLatin1String hostDataKey("hostData");
        const QLatin1String hostKey("ClassicPluginHost");
        const QLatin1String recordKey("playinplugin");
        const QLatin1String originalKey("original");

        QString textOf(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        QString manifestOf(const std::filesystem::path &folder) {
            return textOf(folder / u"plugin.json");
        }

        QString fileIn(const std::filesystem::path &folder, const QString &name) {
            return textOf(folder / name.toStdU16String());
        }

        void fail(kit::DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({kit::Diagnostic::Error, message, std::nullopt});
        }

        // Returns the plugin.json of folder, or std::nullopt if it does not exist or is no
        // JSON object.
        std::optional<QJsonObject> readManifest(const std::filesystem::path &folder) {
            QFile in(manifestOf(folder));
            if (!in.open(QIODevice::ReadOnly)) {
                return std::nullopt;
            }
            const auto document = QJsonDocument::fromJson(in.readAll());
            if (!document.isObject()) {
                return std::nullopt;
            }
            return document.object();
        }

        bool writeManifest(const std::filesystem::path &folder, const QJsonObject &manifest) {
            QSaveFile out(manifestOf(folder));
            return out.open(QIODevice::WriteOnly) &&
                   out.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) >= 0 &&
                   out.commit();
        }

        // Returns the record of the installation in folder, or std::nullopt if none is recorded.
        std::optional<QJsonObject> recordOf(const std::filesystem::path &folder) {
            const auto manifest = readManifest(folder);
            if (!manifest) {
                return std::nullopt;
            }
            const auto record =
                manifest->value(hostDataKey).toObject().value(hostKey).toObject().value(recordKey);
            if (!record.isObject()) {
                return std::nullopt;
            }
            return record.toObject();
        }

        // Returns manifest with record as the record of the installation, or without a record if
        // record is std::nullopt. An object of hostData that becomes empty is removed.
        QJsonObject withRecord(QJsonObject manifest, const std::optional<QJsonObject> &record) {
            auto hostData = manifest.value(hostDataKey).toObject();
            auto host = hostData.value(hostKey).toObject();
            if (record) {
                host.insert(recordKey, *record);
            } else {
                host.remove(recordKey);
            }
            if (host.isEmpty()) {
                hostData.remove(hostKey);
            } else {
                hostData.insert(hostKey, host);
            }
            if (hostData.isEmpty()) {
                manifest.remove(hostDataKey);
            } else {
                manifest.insert(hostDataKey, hostData);
            }
            return manifest;
        }

        // Returns the plugin.json transcribed from the plugin.txt of folder, decoded in charset,
        // or std::nullopt if plugin.txt cannot be read.
        std::optional<QJsonObject> transcription(const std::filesystem::path &folder,
                                                 const QString &charset) {
            utau::PluginTxt txt;
            if (!txt.load(folder / u"plugin.txt")) {
                return std::nullopt;
            }
            const kit::TextCodec codec(charset);
            const auto decode = [&](const std::string &bytes) {
                return codec.decodeReplacing(QByteArrayView(bytes.data(), qsizetype(bytes.size())))
                    .trimmed();
            };
            QJsonObject manifest;
            if (const auto name = decode(txt.name); !name.isEmpty()) {
                manifest.insert(QLatin1String("name"), name);
            }
            manifest.insert(QLatin1String("execute"), decode(txt.execute));
            manifest.insert(QLatin1String("shell"), txt.shell == utau::VALUE_PLUGIN_SHELL_USE);
            manifest.insert(QLatin1String("notes"),
                            txt.notes ? QLatin1String("all") : QLatin1String("selection"));
            if (const auto version = decode(txt.ustVersion); !version.isEmpty()) {
                manifest.insert(QLatin1String("ustversion"), version);
            }
            // The manifest has no charset, so that the temporary file stays in the local
            // encoding, as in UTAU.
            return manifest;
        }

        // Returns whether the files at first and second have the same content.
        bool sameContent(const QString &first, const QString &second) {
            QFile a(first);
            QFile b(second);
            if (!a.open(QIODevice::ReadOnly) || !b.open(QIODevice::ReadOnly) ||
                a.size() != b.size()) {
                return false;
            }
            return a.readAll() == b.readAll();
        }

        // Copies program to target, replacing the file at target.
        bool copyProgram(const std::filesystem::path &program, const QString &target) {
            if (QFile::exists(target) && !QFile::remove(target)) {
                return false;
            }
            return QFile::copy(textOf(program), target);
        }

    }

    const QString ClassicPluginPreviewInstallation::programName =
        QStringLiteral("playinplugin.exe");

    const QString ClassicPluginPreviewInstallation::originalName =
        QStringLiteral("playinplugin-original.exe");

    std::filesystem::path ClassicPluginPreviewInstallation::bundledProgram() {
        const auto path = QDir(QCoreApplication::applicationDirPath())
                              .filePath(QStringLiteral("hello-plugin-play.exe"));
        if (!QFile::exists(path)) {
            return {};
        }
        return std::filesystem::path(path.toStdU16String());
    }

    ClassicPluginPreviewInstallation::State
        ClassicPluginPreviewInstallation::stateOf(const std::filesystem::path &folder,
                                                  const std::filesystem::path &program) {
        if (!recordOf(folder)) {
            return Absent;
        }
        return sameContent(fileIn(folder, programName), textOf(program)) ? Installed : Outdated;
    }

    bool ClassicPluginPreviewInstallation::isRecommended(const std::filesystem::path &folder,
                                                         const std::filesystem::path &program) {
        switch (stateOf(folder, program)) {
            case Absent:
                return QFile::exists(fileIn(folder, programName));
            case Outdated:
                // The installation kept a program of the plugin.
                return !recordOf(folder)->value(originalKey).toString().isEmpty();
            case Installed:
                break;
        }
        return false;
    }

    bool ClassicPluginPreviewInstallation::transcribes(const std::filesystem::path &folder) {
        return !QFile::exists(manifestOf(folder));
    }

    bool ClassicPluginPreviewInstallation::install(const std::filesystem::path &folder,
                                                   const std::filesystem::path &program,
                                                   const QString &charset,
                                                   kit::DiagnosticList &diagnostics) {
        if (recordOf(folder)) {
            fail(diagnostics, tr("The preview program is already installed."));
            return false;
        }
        if (!QFile::exists(textOf(program))) {
            fail(diagnostics,
                 tr("The preview program \"%1\" does not exist.").arg(textOf(program)));
            return false;
        }

        // The plugin.json to write is read or transcribed before any file changes
        const bool transcribed = transcribes(folder);
        auto manifest = transcribed ? transcription(folder, charset) : readManifest(folder);
        if (!manifest) {
            fail(diagnostics, transcribed ? tr("The plugin.txt of the plugin cannot be read.")
                                          : tr("The plugin.json of the plugin cannot be read."));
            return false;
        }

        const auto target = fileIn(folder, programName);
        const auto original = fileIn(folder, originalName);
        const bool keepsOriginal = QFile::exists(target);
        if (keepsOriginal && QFile::exists(original)) {
            fail(diagnostics, tr("The file \"%1\" already exists.").arg(original));
            return false;
        }
        if (keepsOriginal && !QFile::rename(target, original)) {
            fail(diagnostics, tr("The file \"%1\" cannot be renamed.").arg(target));
            return false;
        }
        // Undoes the steps so far, so that after a failure the folder is as it was
        const auto undo = [&] {
            QFile::remove(target);
            if (keepsOriginal) {
                QFile::rename(original, target);
            }
        };
        if (!QFile::copy(textOf(program), target)) {
            undo();
            fail(diagnostics, tr("The preview program cannot be copied to \"%1\".").arg(target));
            return false;
        }

        QJsonObject record;
        record.insert(originalKey, keepsOriginal ? QJsonValue(originalName) : QJsonValue());
        if (!writeManifest(folder, withRecord(*manifest, record))) {
            undo();
            fail(diagnostics, tr("The file \"%1\" cannot be written.").arg(manifestOf(folder)));
            return false;
        }
        return true;
    }

    bool ClassicPluginPreviewInstallation::update(const std::filesystem::path &folder,
                                                  const std::filesystem::path &program,
                                                  kit::DiagnosticList &diagnostics) {
        if (!recordOf(folder)) {
            fail(diagnostics, tr("The preview program is not installed."));
            return false;
        }
        const auto target = fileIn(folder, programName);
        if (!copyProgram(program, target)) {
            fail(diagnostics, tr("The preview program cannot be copied to \"%1\".").arg(target));
            return false;
        }
        return true;
    }

    bool ClassicPluginPreviewInstallation::remove(const std::filesystem::path &folder,
                                                  kit::DiagnosticList &diagnostics) {
        auto manifest = readManifest(folder);
        const auto record = recordOf(folder);
        if (!manifest || !record) {
            fail(diagnostics, tr("The preview program is not installed."));
            return false;
        }
        const auto target = fileIn(folder, programName);
        if (QFile::exists(target) && !QFile::remove(target)) {
            fail(diagnostics, tr("The file \"%1\" cannot be removed.").arg(target));
            return false;
        }
        const auto kept = record->value(originalKey).toString();
        if (!kept.isEmpty() && !QFile::rename(fileIn(folder, kept), target)) {
            fail(diagnostics, tr("The file \"%1\" cannot be renamed.").arg(fileIn(folder, kept)));
            return false;
        }
        // Only the record is removed. A plugin.json that the installation transcribed is kept,
        // because the user or another native plugin may have changed it since, and it reads as
        // the plugin.txt did (author's decision of 2026-10-15).
        if (!writeManifest(folder, withRecord(*manifest, std::nullopt))) {
            fail(diagnostics, tr("The file \"%1\" cannot be written.").arg(manifestOf(folder)));
            return false;
        }
        return true;
    }

}
