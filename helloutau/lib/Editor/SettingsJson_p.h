#ifndef HELLOUTAU_EDITOR_SETTINGSJSON_P_H
#define HELLOUTAU_EDITOR_SETTINGSJSON_P_H

#include <functional>
#include <string_view>

#include <QtCore/QJsonValue>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include <stdcorelib/support/json.h>

namespace hello::daw {

    /// JSON files of the settings: the settings of the application and the settings of the
    /// plugins. The content is held in the JSON types of stdcorelib, whose values are mutable in
    /// place, and is converted to Qt types only at the public interfaces.
    class SettingsJson {
    public:
        /// Returns the object in \a fileName, or an empty object if the file does not exist. A
        /// file that does not contain a JSON object is reported and read as empty. The next
        /// change replaces the file.
        static stdc::json::Object read(const QString &fileName);

        /// Writes the whole \a value to \a fileName, creating its directory, or reports the
        /// failure.
        static void write(const QString &fileName, const stdc::json::Value &value);

        /// Returns the value at \a key in \a object, or null if absent. A key is the path of a
        /// value in the groups of the object, with the names joined by slashes, such as
        /// <tt>engines/resampler</tt>.
        static const stdc::json::Value &valueAt(const stdc::json::Object &object,
                                                std::string_view key);

        /// Replaces the value at \a key in \a object, creating the enclosing groups. If \a value
        /// is null, removes the value together with each group that the removal leaves empty.
        static void insertAt(stdc::json::Object &object, std::string_view key,
                             stdc::json::Value value);

        /// Converts a Qt JSON value to a stdcorelib JSON value. A number without a fractional
        /// part that fits an integer becomes an integer, as Qt stores it. Undefined becomes null.
        static stdc::json::Value stdcOf(const QJsonValue &value);

        /// Converts a stdcorelib JSON value to a Qt JSON value. Binary data, which no setting
        /// holds, has no Qt counterpart and converts to null.
        static QJsonValue qtOf(const stdc::json::Value &value);
    };

    /// A settings file that is written once the event loop runs after its changes, so that the
    /// changes of one pass of the loop result in one write. sync() and the destructor write the
    /// pending changes.
    class SettingsFile {
    public:
        /// Constructs the settings file \a fileName. \a content returns the content at each
        /// write.
        SettingsFile(QString fileName, std::function<stdc::json::Value()> content);

        /// Writes the pending changes.
        ~SettingsFile();

        inline const QString &fileName() const {
            return m_fileName;
        }

        /// Writes the file once the event loop runs, unless a write is already pending.
        void changed();

        /// Writes the pending changes immediately.
        void sync();

    private:
        QString m_fileName;
        std::function<stdc::json::Value()> m_content;
        QTimer m_timer;
        bool m_pending = false;

        Q_DISABLE_COPY(SettingsFile)
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGSJSON_P_H
