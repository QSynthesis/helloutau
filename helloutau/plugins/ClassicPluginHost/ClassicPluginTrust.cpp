#include "ClassicPluginTrust.h"

#include <algorithm>
#include <system_error>

#include <QtCore/QDir>

#include <hellokit/Support/FileIntegrity.h>

#include "ClassicPlugin.h"

namespace hello::daw {

    namespace json = stdc::json;

    namespace {

        const char folderKey[] = "folder";
        const char relativePathKey[] = "relativePath";
        const char sha256Key[] = "sha256";

        QString textOf(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        // Returns the path of the program relative to the plugin folder. ClassicPlugin::program
        // always lies inside the folder.
        std::filesystem::path programInFolder(const ClassicPlugin &plugin) {
            std::error_code error;
            auto relative = std::filesystem::relative(plugin.program, plugin.folder, error);
            return error || relative.empty() ? plugin.program.filename() : relative;
        }

        // Returns the SHA-256 of the program content, so that a changed program requires
        // trust again, or std::nullopt if the program cannot be read.
        std::optional<std::string> fingerprintOf(const ClassicPlugin &plugin,
                                                 QString *reason = nullptr) {
            const auto fingerprint =
                kit::FileFingerprintCache::fingerprintOf(plugin.program, reason);
            if (!fingerprint) {
                return std::nullopt;
            }
            return fingerprint->sha256().toStdString();
        }

        // Returns whether entry is to be kept: its program exists, or its plugin folder cannot
        // be reached, as on a disconnected drive.
        bool isLive(const json::Value &entry) {
            const auto folder = std::filesystem::path(
                QString::fromStdString(entry[folderKey].toString(std::string())).toStdU16String());
            const auto program =
                folder / std::filesystem::path(
                             QString::fromStdString(entry[relativePathKey].toString(std::string()))
                                 .toStdU16String());
            std::error_code error;
            return std::filesystem::exists(program, error) ||
                   !std::filesystem::is_directory(folder, error);
        }

        // Returns a predicate that holds for the record of the folder of plugin.
        auto isRecordOf(const ClassicPlugin &plugin) {
            return [folder = textOf(plugin.folder).toStdString()](const json::Value &entry) {
                return entry[folderKey].toString(std::string()) == folder;
            };
        }

    }

    ClassicPluginTrust::State ClassicPluginTrust::stateOf(const json::Array &records,
                                                          const ClassicPlugin &plugin) {
        const auto sha256 = fingerprintOf(plugin);
        if (!sha256) {
            return Unreadable;
        }
        const auto found = std::find_if(records.begin(), records.end(), isRecordOf(plugin));
        if (found == records.end()) {
            return New;
        }
        return (*found)[sha256Key].toString(std::string()) == *sha256 ? Trusted : Changed;
    }

    std::optional<QString> ClassicPluginTrust::refusalOf(const ClassicPlugin &plugin) {
        QString reason;
        if (!fingerprintOf(plugin, &reason)) {
            return reason;
        }
        return kit::FilePermissions::writableByOthers(plugin.program);
    }

    std::optional<json::Array> ClassicPluginTrust::trusted(json::Array records,
                                                           const ClassicPlugin &plugin) {
        const auto sha256 = fingerprintOf(plugin);
        if (!sha256) {
            return std::nullopt;
        }
        json::Object entry;
        entry.emplace(folderKey, json::Value(textOf(plugin.folder).toStdString()));
        entry.emplace(relativePathKey, json::Value(textOf(programInFolder(plugin)).toStdString()));
        entry.emplace(sha256Key, json::Value(*sha256));
        // One record for each folder, and none for a program that is gone
        const auto ofPlugin = isRecordOf(plugin);
        records.erase(std::remove_if(records.begin(), records.end(),
                                     [&](const json::Value &record) {
                                         return ofPlugin(record) || !isLive(record);
                                     }),
                      records.end());
        records.push_back(json::Value(std::move(entry)));
        return records;
    }

}
