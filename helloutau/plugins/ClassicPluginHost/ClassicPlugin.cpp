#include "ClassicPlugin.h"

#include <algorithm>
#include <system_error>

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
        // Whether the extension of \a path is .exe, in any case.
        bool isExe(const std::filesystem::path &path) {
            return textOf(path.extension()).compare(QLatin1String(".exe"), Qt::CaseInsensitive) ==
                   0;
        }
#endif

        // The program that \a execute names in \a folder , or the reason it may not run. The
        // name comes from a file on disk and is untrusted: an absolute path, a parent step or a
        // link out of the folder would run a program that the plugin does not contain.
        std::filesystem::path programOf(const std::filesystem::path &folder, const QString &execute,
                                        QString &reason) {
            if (execute.isEmpty()) {
                reason = ClassicPlugin::tr("Its plugin.txt names no program.");
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
        const auto file = folder / u"plugin.txt";
        std::error_code error;
        if (!std::filesystem::is_regular_file(file, error)) {
            return std::nullopt;
        }
        utau::PluginTxt txt;
        if (!txt.load(file)) {
            diagnostics.push_back(
                {kit::DiagnosticSeverity::Warning,
                 tr("The plugin in \"%1\" could not be read.").arg(textOf(folder))});
            return std::nullopt;
        }

        ClassicPlugin plugin;
        plugin.folder = folder;
        plugin.charset = localCharset();
        const kit::TextCodec codec(plugin.charset);
        const auto decode = [&](const std::string &bytes) {
            return codec.decodeReplacing(QByteArrayView(bytes.data(), qsizetype(bytes.size())));
        };

        plugin.name = decode(txt.name).trimmed();
        if (plugin.name.isEmpty()) {
            plugin.name = textOf(folder.filename());
        }
        plugin.shell = txt.shell == utau::VALUE_PLUGIN_SHELL_USE;
        plugin.wholeTrack = txt.notes.has_value();
        plugin.program = programOf(folder, decode(txt.execute).trimmed(), plugin.unavailableReason);

#ifndef Q_OS_WINDOWS
        // Wine is to be considered later, see docs/ClassicPluginHost.md.
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
