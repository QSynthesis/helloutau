#ifndef HELLOUTAU_EDITOR_VOICEBANKENTRYMODEL_H
#define HELLOUTAU_EDITOR_VOICEBANKENTRYMODEL_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QAbstractTableModel>

#include <hellokit/Support/Diagnostic.h>
#include <hellokit/VoiceBank/VoiceBank.h>

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
    /// marked, and so is an alias that another entry of the same audio file has. The model
    /// follows the changes of the session, once control returns to the event loop: a change that
    /// keeps the number of rows updates them in place, which keeps the selection of a view.
    ///
    /// Every cell of an entry but its folder is editable, each edit one undo step. Editing the
    /// alias or a value of an audio file without an entry includes the file with that value, in
    /// the same step.
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
            /// Whether another entry of the same audio file has the alias of the row, where an
            /// empty alias counts as the stem of the file name, as the edit layer counts it
            DuplicateAliasRole,
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

        /// The folder of \a row, relative to the root.
        std::filesystem::path directoryOf(int row) const;

        /// The index of the entry of \a row among the entries of its folder, or -1 for an audio
        /// file without an entry.
        int entryIndexOf(int row) const;

        /// The entry that \a row shows. For an audio file without an entry, the file name and
        /// zero values.
        kit::VoiceOtoEntry entryOf(int row) const;

        /// Replaces the entry of \a row with \a value as one undo step, including the audio file
        /// of an unlisted row with \a value.
        ///
        /// \return whether the step is committed, with the reasons in \a diagnostics otherwise
        bool setEntry(int row, const kit::VoiceOtoEntry &value, kit::DiagnosticList &diagnostics);

        int rowCount(const QModelIndex &parent = {}) const override;
        int columnCount(const QModelIndex &parent = {}) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        QVariant headerData(int section, Qt::Orientation orientation,
                            int role = Qt::DisplayRole) const override;
        Qt::ItemFlags flags(const QModelIndex &index) const override;

        /// Edits the entry of the row of \a index, see setEntry(). A value is read as a number
        /// in the C locale, as \c oto.ini writes it. Emits editRejected() if the edit is refused.
        bool setData(const QModelIndex &index, const QVariant &value,
                     int role = Qt::EditRole) override;

    Q_SIGNALS:
        /// An edit of a cell was refused, for the reasons in \a diagnostics.
        void editRejected(const kit::DiagnosticList &diagnostics);

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_VOICEBANKENTRYMODEL_H
