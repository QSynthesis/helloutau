#ifndef HELLOUTAU_EDITOR_SETTINGSJSON_P_H
#define HELLOUTAU_EDITOR_SETTINGSJSON_P_H

#include <string_view>

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <stdcorelib/support/json.h>

namespace hello::daw {

    /// The JSON files of the settings: the settings of the application and those of the plugins.
    /// Their content is kept in the JSON of stdcorelib, whose values change in place, and crosses
    /// into Qt only at the public interfaces.
    class SettingsJson {
    public:
        /// The object in \a fileName, empty if the file does not exist. A file that is not a JSON
        /// object is reported and read as empty, and the next change replaces it.
        static stdc::json::Object read(const QString &fileName);

        /// Writes \a value to \a fileName whole, creating its directory, or reports why not.
        static void write(const QString &fileName, const stdc::json::Value &value);

        /// The value at \a key in \a object, null if there is none. A key is the path of a value
        /// in the groups of the object, the names joined by slashes, such as
        /// <tt>engines/resampler</tt>.
        static const stdc::json::Value &valueAt(const stdc::json::Object &object,
                                                std::string_view key);

        /// Replaces the value at \a key in \a object, creating the groups it lies in, or removes
        /// it if \a value is null, and with it each group that it leaves empty.
        static void insertAt(stdc::json::Object &object, std::string_view key,
                             stdc::json::Value value);

        /// A value of Qt in stdcorelib. A number without a fractional part that fits an integer
        /// becomes one, as Qt keeps it. Undefined becomes null.
        static stdc::json::Value stdcOf(const QJsonValue &value);

        /// The other way. Binary data, which no setting holds, has no counterpart and reads as
        /// null.
        static QJsonValue qtOf(const stdc::json::Value &value);
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGSJSON_P_H
