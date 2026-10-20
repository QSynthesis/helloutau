#ifndef HELLOUTAU_EDITOR_APPSETTINGS_H
#define HELLOUTAU_EDITOR_APPSETTINGS_H

#include <filesystem>
#include <memory>

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <helloutau/Editor/Settings/AppearanceSettings.h>
#include <helloutau/Editor/Settings/AudioSettings.h>
#include <helloutau/Editor/Settings/CommandPaletteSettings.h>
#include <helloutau/Editor/Settings/FileSettings.h>
#include <helloutau/Editor/Settings/FrequencyMapSettings.h>
#include <helloutau/Editor/Settings/PitchControlSettings.h>
#include <helloutau/Editor/Settings/RenderingSettings.h>
#include <helloutau/Editor/Settings/TaskSettings.h>
#include <helloutau/Editor/Settings/UpdateSettings.h>
#include <helloutau/Editor/Settings/UtauSettings.h>
#include <helloutau/Editor/Settings/ViewSettings.h>
#include <helloutau/Editor/Settings/VoiceBankSettings.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// Settings of the application, stored per user in a JSON file. The settings of the plugins
    /// are stored separately, see AppLoader.
    ///
    /// The settings are read and written through the groups of this class, such as updates(),
    /// each a class of the \c Settings directory for one group of the file. The groups exist as
    /// long as this object.
    ///
    /// The file is read once. After changes, the whole file is written once the event loop runs,
    /// so that the changes of one pass of the loop result in one write. sync() and the destructor
    /// also write the file. If two applications change the file, the last write prevails.
    class HELLOUTAU_EDITOR_EXPORT AppSettings {
    public:
        /// Constructs the settings of the current user, stored in defaultFileName(), with
        /// defaultUserDirectory() as the user directory.
        AppSettings();

        /// Constructs the settings stored in \a fileName, with the directory of \a fileName as
        /// the user directory, so that a settings directory other than the default one, such as
        /// that of a test, holds the voice banks and plugins of the user as well.
        explicit AppSettings(const QString &fileName);

        /// Constructs the settings stored in \a fileName with \a userDirectory as the user
        /// directory.
        AppSettings(const QString &fileName, const QString &userDirectory);

        ~AppSettings();

        /// Returns the settings directory of the current user: the data directory that Qt
        /// determines from the organization and application names of \c QCoreApplication.
        static QString defaultDirectory();

        /// Returns the path of \c settings.json in defaultDirectory().
        static QString defaultFileName();

        /// Returns the user directory of the current user: \c OpenVPI/HelloUtau, after the
        /// organization and application names of \c QCoreApplication, in the documents
        /// directory. See the user directories in docs/Distribution.md.
        static QString defaultUserDirectory();

        QString fileName() const;

        /// Returns the directory of the files that the user adds and manages, the voice banks
        /// and the plugins. The directory is not created by this class.
        std::filesystem::path userDirectory() const;

        /// Returns the directory of the voice banks and plugins of UTAU that the user adds for
        /// HelloUtau, the \c ClassicExtensions directory of userDirectory(). Its \c voice and
        /// \c plugins directories mirror those into which the installer of UTAU puts voice banks
        /// and plugins. The directory is not created by this class.
        std::filesystem::path classicExtensionsDirectory() const;

        /// The folders laid out as the folder of UTAU, each with the \c voice and \c plugins
        /// directories, in the order in which their voice banks and plugins are listed
        enum ClassicRoot {
            HelloUtauRoot,    ///< classicExtensionsDirectory()
            UtauDataRoot,     ///< UtauSettings::dataDirectory()
            UtauRoot,         ///< The UTAU folder
            ClassicRootCount, ///< The number of the roots
        };

        /// Returns \a root with \a utauDirectory as the UTAU folder, such as the folder that the
        /// settings page edits before it applies the folder.
        ///
        /// \return the root, or an empty path if the root does not apply: UtauDataRoot if
        ///         UtauSettings::dataDirectory() is empty, UtauRoot if \a utauDirectory is empty
        std::filesystem::path classicRoot(ClassicRoot root,
                                          const std::filesystem::path &utauDirectory) const;

        /// Returns \a root with the UTAU folder of utau().
        std::filesystem::path classicRoot(ClassicRoot root) const;

        /// Writes the pending changes immediately.
        void sync();

        /// \name Groups
        /// @{
        UtauSettings &utau();
        const UtauSettings &utau() const;
        VoiceBankSettings &voiceBanks();
        const VoiceBankSettings &voiceBanks() const;
        RenderingSettings &rendering();
        const RenderingSettings &rendering() const;
        FrequencyMapSettings &frequencyMaps();
        const FrequencyMapSettings &frequencyMaps() const;
        AudioSettings &audio();
        const AudioSettings &audio() const;
        AppearanceSettings &appearance();
        const AppearanceSettings &appearance() const;
        ViewSettings &view();
        const ViewSettings &view() const;
        PitchControlSettings &pitchControl();
        const PitchControlSettings &pitchControl() const;
        FileSettings &files();
        const FileSettings &files() const;
        CommandPaletteSettings &commandPalette();
        const CommandPaletteSettings &commandPalette() const;
        UpdateSettings &updates();
        const UpdateSettings &updates() const;
        TaskSettings &tasks();
        const TaskSettings &tasks() const;
        /// @}

        /// \name Values by key
        ///
        /// Access to any value of the file by its key. A key consists of the names of the
        /// enclosing groups and the name of the value, joined by slashes, such as
        /// \c synthTools/resampler. The groups above access the same values.
        /// @{

        /// Returns the value at \a key, or an undefined value if absent.
        QJsonValue value(const QString &key) const;

        /// Replaces the value at \a key, creating the enclosing groups, and writes the file. An
        /// undefined or null \a value removes the value, together with each group that the
        /// removal leaves empty.
        void setValue(const QString &key, const QJsonValue &value);
        /// @}

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;

        Q_DISABLE_COPY(AppSettings)
    };

}

#endif // HELLOUTAU_EDITOR_APPSETTINGS_H
