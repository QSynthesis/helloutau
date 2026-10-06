#include "Project.h"

#include <fstream>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

#include "DocumentConstants.h"
#include "JsonFields_p.h"
#include "PayloadCodec.h"

namespace hello::kit {

    namespace {

        constexpr char KeyFormat[] = "$format";
        constexpr char KeyVersion[] = "version";
        constexpr char KeySettings[] = "settings";
        constexpr char KeyTracks[] = "tracks";

        constexpr char FormatName[] = "usth";

        QJsonObject settingsToJson(const ProjectSettings &settings) {
            return QJsonObject{
                {QLatin1String("name"),       settings.name                              },
                {QLatin1String("tempo"),      settings.tempo                             },
                {QLatin1String("flags"),      settings.flags                             },
                {QLatin1String("outputFile"), Project::savedPathText(settings.outputFile)},
                {QLatin1String("cacheDir"),   Project::savedPathText(settings.cacheDir)  },
                {QLatin1String("wavtool"),    Project::savedPathText(settings.wavtool)   },
                {QLatin1String("resampler"),  Project::savedPathText(settings.resampler) },
                {QLatin1String("mode2"),      settings.mode2                             },
            };
        }

        ProjectSettings settingsFromJson(const QJsonObject &object, DiagnosticList &diagnostics) {
            ProjectSettings settings;
            settings.name = JsonFields::readString(object, "name");
            settings.tempo = JsonFields::readOptionalDouble(object, "tempo", diagnostics)
                                 .value_or(utau::DEFAULT_VALUE_TEMPO);
            settings.flags = JsonFields::readString(object, "flags");
            settings.outputFile = JsonFields::readString(object, "outputFile");
            settings.cacheDir = JsonFields::readString(object, "cacheDir");

            // Stored verbatim. Executing them is a separate, guarded decision, whereas discarding
            // them here would delete a deliberate user setting. See the security section of
            // CLAUDE.md.
            settings.wavtool = JsonFields::readString(object, "wavtool");
            settings.resampler = JsonFields::readString(object, "resampler");

            const auto mode2 = object.value(QLatin1String("mode2"));
            settings.mode2 = mode2.isBool() ? mode2.toBool() : true;
            return settings;
        }

        // A path in a project file uses the Windows separator, which std::filesystem recognizes
        // only on Windows.
        std::filesystem::path pathOf(QString text) {
            text.replace(u'\\', u'/');
            return std::filesystem::path(text.toStdU16String());
        }

        // The settings are a parameter, so that save() can write its own value of the cache
        // directory without copying the notes.
        QByteArray jsonOf(const Project &project, const ProjectSettings &settings,
                          QJsonDocument::JsonFormat format) {
            // Starts from the fields not recognized when the file was read, so that they are
            // preserved. The known fields are inserted afterward, so that a stale copy of a known
            // field cannot take precedence.
            QJsonObject root = project.unknownFields;
            root.insert(QLatin1String(KeyFormat), QLatin1String(FormatName));
            root.insert(QLatin1String(KeyVersion), usthFormatVersion);
            root.insert(QLatin1String(KeySettings), settingsToJson(settings));

            QJsonArray trackArray;
            for (const auto &track : project.tracks) {
                QJsonArray notes;
                for (const auto &note : track.notes) {
                    notes.append(note.toJson());
                }
                trackArray.append(QJsonObject{
                    {QLatin1String("name"),     track.name                            },
                    {QLatin1String("voiceDir"), Project::savedPathText(track.voiceDir)},
                    {QLatin1String("notes"),    notes                                 },
                });
            }
            root.insert(QLatin1String(KeyTracks), trackArray);

            return QJsonDocument(root).toJson(format);
        }

        // Without a trailing separator, which would otherwise count as an empty last element
        std::filesystem::path directoryOf(const std::filesystem::path &path) {
            const auto normal = path.lexically_normal();
            return normal.has_filename() ? normal : normal.parent_path();
        }

    }

    std::filesystem::path Track::voiceDirectory(const std::filesystem::path &utauDirectory) const {
        if (voiceDir.isEmpty()) {
            return {};
        }
        if (voiceDir.startsWith(voicePrefix)) {
            if (utauDirectory.empty()) {
                return {};
            }
            // A separator after the prefix would otherwise make the rest an absolute path.
            auto rest = voiceDir.mid(voicePrefix.size());
            while (rest.startsWith(u'\\') || rest.startsWith(u'/')) {
                rest.remove(0, 1);
            }
            return utauDirectory / u"voice" / pathOf(rest);
        }
        const auto path = pathOf(voiceDir);
        if (path.is_absolute()) {
            return path;
        }
        if (utauDirectory.empty()) {
            return {};
        }
        return utauDirectory / path;
    }

    QString Track::voiceDirOf(const std::filesystem::path &directory,
                              const std::filesystem::path &utauDirectory) {
        const auto bank = directoryOf(directory);
        if (!utauDirectory.empty()) {
            const auto relative = bank.lexically_relative(directoryOf(utauDirectory / u"voice"));
            if (!relative.empty() && relative != u"." && *relative.begin() != u"..") {
                return voicePrefix.toString() +
                       QDir::toNativeSeparators(QString::fromStdU16String(relative.u16string()));
            }
        }
        return QDir::toNativeSeparators(QString::fromStdU16String(bank.u16string()));
    }

    std::optional<Project> Project::open(const std::filesystem::path &path,
                                         DiagnosticList &diagnostics) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            JsonFields::fail(diagnostics, tr("This file could not be opened."));
            return std::nullopt;
        }
        const std::string bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
        return fromJson(QByteArrayView(bytes.data(), qsizetype(bytes.size())), diagnostics);
    }

    bool Project::save(const std::filesystem::path &path, DiagnosticList &diagnostics,
                       QJsonDocument::JsonFormat format) const {
        if (tracks.size() != 1) {
            JsonFields::fail(diagnostics,
                             tr("This version of HelloUtau supports one track per project, but "
                                "this project contains %1.")
                                 .arg(tracks.size()));
            return false;
        }

        auto written = settings;
        written.cacheDir = cacheDirOf(path);
        const auto bytes = jsonOf(*this, written, format);

        // Written in binary mode, so that the line feeds of the indented form are not converted
        // to CRLF on Windows. The format specifies LF line endings.
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            JsonFields::fail(diagnostics, tr("This file could not be written."));
            return false;
        }
        out.write(bytes.constData(), bytes.size());
        if (!out) {
            JsonFields::fail(diagnostics, tr("This file could not be written."));
            return false;
        }
        return true;
    }


    std::optional<Project> Project::fromJson(QByteArrayView json, DiagnosticList &diagnostics) {
        QJsonParseError error{};
        const auto document = QJsonDocument::fromJson(json.toByteArray(), &error);
        if (error.error != QJsonParseError::NoError) {
            JsonFields::fail(diagnostics,
                             tr("This file is not valid JSON: %1").arg(error.errorString()));
            return std::nullopt;
        }
        if (!document.isObject()) {
            JsonFields::fail(diagnostics, tr("This file is not a HelloUtau project."));
            return std::nullopt;
        }

        const auto root = document.object();

        // Checked first, because a foreign file may still be valid JSON and would otherwise be
        // read field by field into a project consisting of defaults.
        if (root.value(QLatin1String(KeyFormat)).toString() != QLatin1String(FormatName)) {
            JsonFields::fail(diagnostics, tr("This file is not a HelloUtau project."));
            return std::nullopt;
        }

        const auto version = root.value(QLatin1String(KeyVersion));
        if (!version.isDouble()) {
            JsonFields::fail(diagnostics, tr("This project does not specify its format version."));
            return std::nullopt;
        }
        if (int(version.toDouble()) > usthFormatVersion) {
            JsonFields::fail(diagnostics,
                             tr("This project was saved by a newer version of HelloUtau and "
                                "cannot be opened by this version."));
            return std::nullopt;
        }

        const auto tracks = root.value(QLatin1String(KeyTracks));
        if (!tracks.isArray()) {
            JsonFields::fail(diagnostics, tr("This project contains no tracks."));
            return std::nullopt;
        }

        // Exactly one track. Any other count is rejected rather than truncated. The array exists
        // to allow multiple tracks later, and a build that cannot represent them must report
        // this instead of opening the file with part of the music missing.
        const auto trackArray = tracks.toArray();
        if (trackArray.size() != 1) {
            JsonFields::fail(diagnostics,
                             tr("This project contains %1 tracks, but this version of HelloUtau "
                                "supports only one.")
                                 .arg(trackArray.size()));
            return std::nullopt;
        }

        Project project;
        project.settings =
            settingsFromJson(root.value(QLatin1String(KeySettings)).toObject(), diagnostics);

        const auto trackObject = trackArray.first().toObject();
        Track track;
        track.name = JsonFields::readString(trackObject, "name");
        track.voiceDir = JsonFields::readString(trackObject, "voiceDir");

        const auto notes = trackObject.value(QLatin1String("notes")).toArray();
        track.notes.reserve(notes.size());
        for (int i = 0; i < notes.size(); ++i) {
            const auto first = diagnostics.size();
            auto note = Note::fromJson(notes.at(i).toObject(), diagnostics);
            for (auto j = first; j < diagnostics.size(); ++j) {
                diagnostics[j].noteIndex = i;
            }
            if (!note) {
                return std::nullopt;
            }
            track.notes.push_back(*note);
        }
        project.tracks.push_back(track);

        for (auto it = root.begin(); it != root.end(); ++it) {
            const auto key = it.key();
            if (key != QLatin1String(KeyFormat) && key != QLatin1String(KeyVersion) &&
                key != QLatin1String(KeySettings) && key != QLatin1String(KeyTracks)) {
                project.unknownFields.insert(key, it.value());
            }
        }

        return project;
    }

    QByteArray Project::toJson(QJsonDocument::JsonFormat format) const {
        return jsonOf(*this, settings, format);
    }

    QString Project::cacheDirOf(const std::filesystem::path &file) {
        return QString::fromStdU16String(file.stem().u16string()) + QStringLiteral(".cache");
    }

    std::filesystem::path Project::cacheDirectoryOf(const std::filesystem::path &file) {
        return file.parent_path() / cacheDirOf(file).toStdU16String();
    }

    QString Project::savedPathText(const QString &text) {
        auto result = text;
        if (text.startsWith(u'/')) {
            result.replace(u'\\', u'/');
        } else {
            result.replace(u'/', u'\\');
        }
        return result;
    }

}
