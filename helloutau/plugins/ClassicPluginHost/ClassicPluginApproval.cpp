#include "ClassicPluginApproval.h"

#include <algorithm>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>

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
        // approval again.
        std::string fingerprintOf(const ClassicPlugin &plugin) {
            QFile file(QString::fromStdU16String(plugin.program.u16string()));
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (file.open(QIODevice::ReadOnly)) {
                hash.addData(&file);
            }
            return hash.result().toHex().toStdString();
        }

        // Returns a predicate that holds for the record of the folder of plugin.
        auto isRecordOf(const ClassicPlugin &plugin) {
            return [folder = textOf(plugin.folder).toStdString()](const json::Value &entry) {
                return entry[folderKey].toString(std::string()) == folder;
            };
        }

    }

    ClassicPluginApproval::State ClassicPluginApproval::stateOf(const json::Array &records,
                                                                const ClassicPlugin &plugin) {
        const auto found = std::find_if(records.begin(), records.end(), isRecordOf(plugin));
        if (found == records.end()) {
            return New;
        }
        return (*found)[sha256Key].toString(std::string()) == fingerprintOf(plugin) ? Approved
                                                                                    : Changed;
    }

    json::Array ClassicPluginApproval::approved(json::Array records, const ClassicPlugin &plugin) {
        json::Object entry;
        entry.emplace(folderKey, json::Value(textOf(plugin.folder).toStdString()));
        entry.emplace(relativePathKey, json::Value(textOf(programInFolder(plugin)).toStdString()));
        entry.emplace(sha256Key, json::Value(fingerprintOf(plugin)));
        const auto found = std::find_if(records.begin(), records.end(), isRecordOf(plugin));
        if (found != records.end()) {
            *found = json::Value(std::move(entry));
        } else {
            records.push_back(json::Value(std::move(entry)));
        }
        return records;
    }

}
