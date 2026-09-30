#include "ClassicPlugin.h"

#include <algorithm>
#include <system_error>

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <stdutau/plugintxt.h>
#include <stdutau/utaconst.h>

#include <hellokit/Support/TextCodec.h>

namespace hello::daw {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        std::filesystem::path pathOf(const QString &text) {
            return std::filesystem::path(text.toStdU16String());
        }

#ifndef Q_OS_WINDOWS
        // Returns whether the extension of \a path is .exe, compared case-insensitively.
        bool isExe(const std::filesystem::path &path) {
            return textOf(path.extension()).compare(QLatin1String(".exe"), Qt::CaseInsensitive) ==
                   0;
        }
#endif

        // Returns the program that \a execute specifies in \a folder , or an empty path with
        // the reason in \a reason . The name is read from a file on disk and is untrusted. An
        // absolute path, a .. component, or a link that leaves the folder would run a program
        // outside the plugin.
        std::filesystem::path programOf(const std::filesystem::path &folder, const QString &execute,
                                        QString &reason) {
            if (execute.isEmpty()) {
                reason = ClassicPlugin::tr("Its plugin.txt specifies no program.");
                return {};
            }
            const auto relative = pathOf(execute);
            const bool parentStep =
                std::any_of(relative.begin(), relative.end(),
                            [](const auto &part) { return part == std::filesystem::path(u".."); });
            if (relative.has_root_name() || relative.has_root_directory() || parentStep) {
                reason =
                    ClassicPlugin::tr("Its program \"%1\" is outside its folder.").arg(execute);
                return {};
            }

            std::error_code error;
            const auto base = std::filesystem::weakly_canonical(folder, error);
            const auto program = std::filesystem::weakly_canonical(folder / relative, error);
            const auto inside = program.lexically_relative(base);
            if (error || inside.empty() || *inside.begin() == std::filesystem::path(u"..")) {
                reason =
                    ClassicPlugin::tr("Its program \"%1\" is outside its folder.").arg(execute);
                return {};
            }
            if (!std::filesystem::is_regular_file(program, error)) {
                reason = ClassicPlugin::tr("Its program \"%1\" was not found.").arg(execute);
                return {};
            }
            return program;
        }

    }

    std::optional<ClassicPlugin> ClassicPlugin::read(const std::filesystem::path &folder,
                                                     kit::DiagnosticList &diagnostics) {
        ClassicPlugin plugin;
        plugin.folder = folder;
        QString execute;
        const auto unreadable = [&](const QString &reason) {
            diagnostics.push_back(
                {kit::DiagnosticSeverity::Warning,
                 tr("The plugin in \"%1\" could not be read: %2").arg(textOf(folder), reason)});
            return std::nullopt;
        };

        std::error_code error;
        if (const auto file = folder / u"plugin.json";
            std::filesystem::is_regular_file(file, error)) {
            // The UTF-8 manifest of a plugin that supports HelloUtau. It replaces plugin.txt
            // entirely, and plugin.txt is not read. See the plugin section of docs/note.md.
            QFile in(textOf(file));
            if (!in.open(QIODevice::ReadOnly)) {
                return unreadable(in.errorString());
            }
            QJsonParseError parseError;
            const auto document = QJsonDocument::fromJson(in.readAll(), &parseError);
            if (!document.isObject()) {
                return unreadable(parseError.errorString());
            }
            const auto manifest = document.object();
            plugin.name = manifest.value(QLatin1String("name")).toString().trimmed();
            execute = manifest.value(QLatin1String("execute")).toString().trimmed();
            plugin.shell = manifest.value(QLatin1String("shell")).toBool();

            const auto notes =
                manifest.value(QLatin1String("notes")).toString(QStringLiteral("selection"));
            plugin.wholeTrack = notes == QLatin1String("all");
            if (!plugin.wholeTrack && notes != QLatin1String("selection")) {
                plugin.unavailableReason =
                    tr("Its plugin.json specifies the unknown value \"%1\" for notes.").arg(notes);
            }

            // The encoding of the temporary file, which defaults to the encoding of UTAU
            const auto charset = manifest.value(QLatin1String("charset")).toString();
            const kit::TextCodec codec(charset.isEmpty() ? localCharset() : charset);
            plugin.charset = codec.name();
            if (!codec.isValid()) {
                plugin.charset = charset;
                plugin.unavailableReason = tr("Its encoding \"%1\" is not available.").arg(charset);
            }
        } else if (const auto file = folder / u"plugin.txt";
                   std::filesystem::is_regular_file(file, error)) {
            utau::PluginTxt txt;
            if (!txt.load(file)) {
                return unreadable(tr("plugin.txt cannot be opened."));
            }
            plugin.charset = localCharset();
            const kit::TextCodec codec(plugin.charset);
            const auto decode = [&](const std::string &bytes) {
                return codec.decodeReplacing(QByteArrayView(bytes.data(), qsizetype(bytes.size())));
            };
            plugin.name = decode(txt.name).trimmed();
            execute = decode(txt.execute).trimmed();
            plugin.shell = txt.shell == utau::VALUE_PLUGIN_SHELL_USE;
            plugin.wholeTrack = txt.notes.has_value();
        } else {
            return std::nullopt;
        }

        if (plugin.name.isEmpty()) {
            plugin.name = textOf(folder.filename());
        }
        QString programReason;
        plugin.program = programOf(folder, execute, programReason);
        if (plugin.isAvailable()) {
            plugin.unavailableReason = programReason;
        }

#ifndef Q_OS_WINDOWS
        // Support for Wine is deferred. See docs/ClassicPluginHost.md.
        if (plugin.isAvailable() && (plugin.shell || isExe(plugin.program))) {
            plugin.unavailableReason = tr("It runs on Windows only.");
        }
#endif
        return plugin;
    }

    QList<ClassicPlugin> ClassicPlugin::discover(const QList<std::filesystem::path> &directories,
                                                 kit::DiagnosticList &diagnostics) {
        QList<ClassicPlugin> plugins;
        for (const auto &directory : directories) {
            std::error_code error;
            std::vector<std::filesystem::path> folders;
            for (const auto &entry : std::filesystem::directory_iterator(directory, error)) {
                std::error_code entryError;
                if (entry.is_directory(entryError)) {
                    folders.push_back(entry.path());
                }
            }
            std::sort(folders.begin(), folders.end());
            for (const auto &folder : folders) {
                if (auto plugin = read(folder, diagnostics)) {
                    plugins.push_back(std::move(*plugin));
                }
            }
        }
        return plugins;
    }

    QString ClassicPlugin::localCharset() {
#ifdef Q_OS_WINDOWS
        return kit::TextCodec().name();
#else
        return QStringLiteral("Shift_JIS");
#endif
    }

}
