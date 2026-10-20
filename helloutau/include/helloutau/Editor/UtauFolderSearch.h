#ifndef HELLOUTAU_EDITOR_UTAUFOLDERSEARCH_H
#define HELLOUTAU_EDITOR_UTAUFOLDERSEARCH_H

#include <filesystem>
#include <optional>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// A search for the UTAU folders of this computer, the candidates of the UTAU folder of the
    /// settings. See docs/UtauSearch.md.
    ///
    /// run() looks in the sources in the order of Source and emits found() for each folder that
    /// holds a \c utau.exe, once per folder. It is intended for a worker thread, and stops early
    /// if the thread is requested to be interrupted. The static functions are the parts that
    /// depend only on their arguments.
    class HELLOUTAU_EDITOR_EXPORT UtauFolderSearch : public QObject {
        Q_OBJECT
    public:
        /// Where a folder is found
        enum Source {
            Installer,      ///< The official installer, as Windows Installer records it
            CommonLocation, ///< A folder in which UTAU is commonly placed
            RunHistory,     ///< The programs that Windows records as run
            Wine,           ///< The default prefix of Wine on macOS and Linux
        };

        struct Candidate {
            std::filesystem::path folder;
            Source source = CommonLocation;
            /// The version of \c utau.exe as versionText() writes it, or an empty string if
            /// unknown
            QString version;
        };

        explicit UtauFolderSearch(QObject *parent = nullptr);
        ~UtauFolderSearch();

        /// Looks in every source of this system and emits found() for each new candidate, then
        /// finished().
        void run();

        /// Returns the name of \a source that the dialog shows.
        static QString sourceName(Source source);

        /// Returns the common folders of UTAU on \a drives, the roots of the fixed disks, and in
        /// \a localAppData, the local application data folder of the user, unless empty.
        static QList<std::filesystem::path>
            commonLocations(const QList<std::filesystem::path> &drives,
                            const std::filesystem::path &localAppData);

        /// Returns the folders of UTAU in the Wine prefix \a prefix.
        static QList<std::filesystem::path> wineLocations(const std::filesystem::path &prefix);

        /// Returns the folders of the programs named \c utau.exe among \a names, the value
        /// names of the run history of Windows: a path to a program, followed by a suffix such
        /// as \c .FriendlyAppName in the MuiCache. Each folder is returned once, in the order of
        /// \a names.
        static QList<std::filesystem::path> foldersOfPrograms(const QStringList &names);

        /// Returns \a fileVersion, such as \c 0.04.0019, as \c 0.4.19, followed by
        /// \a productName in parentheses unless it is the name of the original UTAU. Returns
        /// \a fileVersion unchanged if its parts are not numbers.
        static QString versionText(const QString &fileVersion, const QString &productName);

        /// Returns the candidate for \a folder found in \a source.
        ///
        /// \return the candidate, or \c std::nullopt if \a folder holds no \c utau.exe, in any
        ///         letter case
        static std::optional<Candidate> candidateOf(const std::filesystem::path &folder,
                                                    Source source);

    Q_SIGNALS:
        void found(const hello::daw::UtauFolderSearch::Candidate &candidate);
        void finished();

    private:
        // The folders found so far, to which a folder already found is equivalent
        QList<std::filesystem::path> m_folders;

        // Emits found() for the candidate of folder unless an equivalent folder was found.
        // Returns false if the search is to stop.
        bool offer(const std::filesystem::path &folder, Source source);
    };

}

Q_DECLARE_METATYPE(hello::daw::UtauFolderSearch::Candidate)

#endif // HELLOUTAU_EDITOR_UTAUFOLDERSEARCH_H
