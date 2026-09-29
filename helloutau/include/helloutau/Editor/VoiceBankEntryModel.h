#ifndef HELLOUTAU_EDITOR_VOICEBANKENTRYMODEL_H
#define HELLOUTAU_EDITOR_VOICEBANKENTRYMODEL_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QAbstractTableModel>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::kit {
    class VoiceBankSession;
}

namespace hello::daw {

    /// The oto entries of a voice bank as a table, read from the tree of its session: those of
    /// one directory, or of all of them. See the section on the entry table in
    /// docs/VoiceBankEditor.md.
    ///
    /// Besides the entries, each directory lists its audio files without an entry, which are not
    /// in the tree until the user takes them in. An entry whose audio file does not exist is
    /// marked. The model follows the changes of the session, once control returns to the event
    /// loop.
    class HELLOUTAU_EDITOR_EXPORT VoiceBankEntryModel : public QAbstractTableModel {
        Q_OBJECT
    public:
        enum Column {
            DirectoryColumn,
            FileColumn,
            AliasColumn,
            OffsetColumn,
            ConsonantColumn,
            CutoffColumn,
            PreUtteranceColumn,
            OverlapColumn,
            ColumnCount,
        };

        /// What a row shows, in RowKindRole.
        enum RowKind {
            /// An oto entry whose audio file exists
            EntryRow,
            /// An oto entry whose audio file does not exist
            MissingAudioRow,
            /// An audio file without an entry
            UnlistedAudioRow,
        };

        enum Role {
            RowKindRole = Qt::UserRole,
            /// The value by which the column sorts: a number for the values, text otherwise
            SortRole,
        };

        explicit VoiceBankEntryModel(kit::VoiceBankSession *session, QObject *parent = nullptr);
        ~VoiceBankEntryModel();

        /// The directory whose entries are shown, relative to the root, or none for all of them.
        /// All of them at first.
        std::optional<std::filesystem::path> directory() const;
        void setDirectory(const std::optional<std::filesystem::path> &directory);

        /// Rebuilds the rows now rather than once control returns to the event loop.
        void refresh();

        /// The row of the entry of \a fileName with \a alias in the folder \a directory, or of the
        /// audio file \a fileName without an entry, whose alias is empty; -1 if none is shown.
        int rowOf(const std::filesystem::path &directory, const QString &fileName,
                  const QString &alias) const;

        int rowCount(const QModelIndex &parent = {}) const override;
        int columnCount(const QModelIndex &parent = {}) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        QVariant headerData(int section, Qt::Orientation orientation,
                            int role = Qt::DisplayRole) const override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_VOICEBANKENTRYMODEL_H
