#ifndef HELLOUTAU_EDITOR_ENGINETRUST_H
#define HELLOUTAU_EDITOR_ENGINETRUST_H

#include <filesystem>

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QWidget;

namespace hello::daw {

    class AppSettings;

    /// Trust of the rendering engines that a project specifies, the wavtool and the resampler.
    ///
    /// A project file is untrusted input, and an engine path that it specifies runs only after
    /// the user has trusted the engine. A trust record holds the canonical path of the engine
    /// and the SHA-256 of its file, so that a changed file is no longer trusted. The records are
    /// kept in the application settings. A relative engine path is resolved against the UTAU
    /// directory of the settings. See the section on security in docs/note.md.
    class HELLOUTAU_EDITOR_EXPORT EngineTrust {
    public:
        /// Returns the path of the engine \a value, resolved against \a utau if relative. Returns
        /// an empty path if \a value is empty.
        static std::filesystem::path resolved(const QString &value,
                                              const std::filesystem::path &utau);

        /// Returns whether the engine \a value is a regular file.
        static bool exists(const QString &value, const std::filesystem::path &utau);

        /// Returns whether \a first and \a second resolve to the same file.
        static bool samePath(const QString &first, const QString &second,
                             const std::filesystem::path &utau);

        /// Returns whether \a settings records trust in the engine \a value with the current
        /// content of its file.
        static bool isTrusted(const AppSettings &settings, const QString &value,
                              const std::filesystem::path &utau);

        /// Returns whether the engine \a value may run: it exists, and it is an engine of
        /// \a settings or \a settings records trust in it.
        static bool isAllowed(const AppSettings &settings, const QString &value,
                              const std::filesystem::path &utau);

        /// Asks the user in one message box whether to trust those of the engines \a values
        /// that exist and are not allowed, and records the trust if the user agrees. Returns
        /// whether every engine of \a values is allowed afterwards. Returns \c false without
        /// asking if an engine does not exist.
        static bool ask(QWidget *parent, AppSettings &settings, const QStringList &values,
                        const std::filesystem::path &utau);

        /// Records trust in the engine \a value with the current content of its file. Records
        /// nothing if the file does not exist.
        static void trust(AppSettings &settings, const QString &value,
                          const std::filesystem::path &utau);
    };

}

#endif // HELLOUTAU_EDITOR_ENGINETRUST_H
