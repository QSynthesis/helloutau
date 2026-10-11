#ifndef HELLOKIT_SUPPORT_SETTINGSFILE_H
#define HELLOKIT_SUPPORT_SETTINGSFILE_H

#include <functional>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include <stdcorelib/support/json.h>

#include <hellokit/Support/Diagnostic.h>
#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// A JSON file of settings, the settings of the application or the settings of the plugins,
    /// that is written once the event loop runs after its changes, so that the changes of one
    /// pass of the loop result in one write. sync() and the destructor write the pending
    /// changes. The content is held in the JSON types of stdcorelib, whose values are mutable in
    /// place. JsonInterop addresses a value by its path.
    class HELLOKIT_SUPPORT_EXPORT SettingsFile {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::SettingsFile)
    public:
        /// Reads the object in \a fileName.
        ///
        /// \return the object, an empty object if the file does not exist, or \c std::nullopt
        ///         with an error in \a diagnostics if the file cannot be opened or does not
        ///         contain a JSON object. The next change replaces such a file.
        static std::optional<stdc::json::Object> read(const QString &fileName,
                                                      DiagnosticList &diagnostics);

        /// Constructs the settings file \a fileName. \a content returns the content at each
        /// write.
        SettingsFile(QString fileName, std::function<stdc::json::Value()> content);

        /// Writes the pending changes.
        ~SettingsFile();

        inline const QString &fileName() const {
            return m_fileName;
        }

        /// Writes the file once the event loop runs, unless a write is already pending.
        void syncLater();

        /// Writes the pending changes immediately.
        void sync();

    private:
        // Writes the whole value to the file, creating its directory, or reports the failure.
        void write(const stdc::json::Value &value) const;

        QString m_fileName;
        std::function<stdc::json::Value()> m_content;
        QTimer m_timer;
        bool m_pending = false;

        Q_DISABLE_COPY(SettingsFile)
    };

}

#endif // HELLOKIT_SUPPORT_SETTINGSFILE_H
