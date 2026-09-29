#include "VoiceBankEntryModel.h"

#include <QtCore/QLocale>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtGui/QBrush>
#include <QtGui/QFont>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>

#include <stdcorelib/pimpl.h>

#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/Edit/VoiceBankSession.h>

namespace hello::daw {

    namespace {

        // File names compare as the file system of the host does: on Windows an entry
        // for A.wav uses a.wav.
        QString fileKey(const QString &name) {
#ifdef Q_OS_WIN
            return name.toCaseFolded();
#else
            return name;
#endif
        }

        QString directoryText(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.generic_u16string());
        }

    }

    class VoiceBankEntryModel::Impl {
    public:
        using Decl = VoiceBankEntryModel;

        Impl(Decl *decl, kit::VoiceBankSession *session) : _decl(decl), session(session) {
        }

        struct Row {
            RowKind kind = EntryRow;
            std::filesystem::path directory;
            kit::VoiceOtoEntry entry;
        };

        Decl *_decl;
        kit::VoiceBankSession *session;
        std::optional<std::filesystem::path> directory;
        QList<Row> rows;
        bool refreshPending = false;

        void build() {
            QList<Row> built;
            const auto directories = kit::VoiceBankRef(session).directories();
            for (int d = 0; d < directories.size(); ++d) {
                const auto ref = directories.at(d);
                const auto path = ref.path();
                if (directory && *directory != path) {
                    continue;
                }
                QSet<QString> audio;
                const auto files = session->audioFiles(path);
                for (const auto &file : files) {
                    audio.insert(fileKey(file));
                }
                QSet<QString> listed;
                const auto entries = ref.otoEntries();
                for (int e = 0; e < entries.size(); ++e) {
                    Row row;
                    row.directory = path;
                    row.entry = entries.at(e).toVoiceOtoEntry();
                    const auto key = fileKey(row.entry.fileName);
                    listed.insert(key);
                    row.kind = audio.contains(key) ? EntryRow : MissingAudioRow;
                    built.push_back(row);
                }
                for (const auto &file : files) {
                    if (!listed.contains(fileKey(file))) {
                        Row row;
                        row.kind = UnlistedAudioRow;
                        row.directory = path;
                        row.entry.fileName = file;
                        built.push_back(row);
                    }
                }
            }
            stdc_decl_t;
            decl.beginResetModel();
            rows = std::move(built);
            decl.endResetModel();
        }

        void scheduleRefresh() {
            stdc_decl_t;
            if (refreshPending) {
                return;
            }
            refreshPending = true;
            QTimer::singleShot(0, &decl, [this] {
                if (refreshPending) {
                    refreshPending = false;
                    build();
                }
            });
        }

        // The value of column of an entry as its file writes it, or in the shortest form
        static QString valueText(const kit::VoiceOtoEntry &entry, int column) {
            const int field = column - OffsetColumn;
            if (const auto &spelling = entry.spellings[size_t(field)]) {
                return QString::fromStdString(*spelling);
            }
            const double values[] = {entry.offset, entry.consonant, entry.cutoff,
                                     entry.preUtterance, entry.voiceOverlap};
            return QLocale::c().toString(values[field], 'g', QLocale::FloatingPointShortest);
        }
    };

    VoiceBankEntryModel::VoiceBankEntryModel(kit::VoiceBankSession *session, QObject *parent)
        : QAbstractTableModel(parent), _impl(std::make_unique<Impl>(this, session)) {
        stdc_impl_t;
        connect(session, &kit::VoiceBankSession::changed, this, [this] {
            stdc_impl_t;
            impl.scheduleRefresh();
        });
        impl.build();
    }

    VoiceBankEntryModel::~VoiceBankEntryModel() = default;

    std::optional<std::filesystem::path> VoiceBankEntryModel::directory() const {
        stdc_impl_t;
        return impl.directory;
    }

    void VoiceBankEntryModel::setDirectory(const std::optional<std::filesystem::path> &directory) {
        stdc_impl_t;
        if (directory == impl.directory) {
            return;
        }
        impl.directory = directory;
        impl.refreshPending = false;
        impl.build();
    }

    void VoiceBankEntryModel::refresh() {
        stdc_impl_t;
        impl.refreshPending = false;
        impl.build();
    }

    int VoiceBankEntryModel::rowCount(const QModelIndex &parent) const {
        stdc_impl_t;
        return parent.isValid() ? 0 : int(impl.rows.size());
    }

    int VoiceBankEntryModel::columnCount(const QModelIndex &parent) const {
        return parent.isValid() ? 0 : ColumnCount;
    }

    QVariant VoiceBankEntryModel::data(const QModelIndex &index, int role) const {
        stdc_impl_t;
        if (!index.isValid() || index.row() >= impl.rows.size()) {
            return {};
        }
        const auto &row = impl.rows.at(index.row());
        switch (role) {
            case Qt::DisplayRole:
                switch (index.column()) {
                    case DirectoryColumn:
                        return directoryText(row.directory);
                    case FileColumn:
                        return row.entry.fileName;
                    case AliasColumn:
                        return row.entry.alias;
                    default:
                        return row.kind == UnlistedAudioRow
                                   ? QVariant()
                                   : Impl::valueText(row.entry, index.column());
                }
            case Qt::TextAlignmentRole:
                if (index.column() >= OffsetColumn) {
                    return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
                }
                return {};
            case Qt::ForegroundRole:
                if (row.kind == UnlistedAudioRow) {
                    return QGuiApplication::palette().brush(QPalette::Disabled, QPalette::Text);
                }
                return {};
            case Qt::FontRole:
                if (row.kind != EntryRow) {
                    QFont font;
                    font.setItalic(row.kind == UnlistedAudioRow);
                    font.setStrikeOut(row.kind == MissingAudioRow);
                    return font;
                }
                return {};
            case Qt::ToolTipRole:
                switch (row.kind) {
                    case MissingAudioRow:
                        return tr("The audio file of this entry does not exist.");
                    case UnlistedAudioRow:
                        return tr("This audio file has no entry.");
                    default:
                        return {};
                }
            case RowKindRole:
                return row.kind;
            case SortRole: {
                const double values[] = {row.entry.offset, row.entry.consonant, row.entry.cutoff,
                                         row.entry.preUtterance, row.entry.voiceOverlap};
                if (index.column() >= OffsetColumn) {
                    return values[index.column() - OffsetColumn];
                }
                return data(index, Qt::DisplayRole);
            }
            default:
                return {};
        }
    }

    QVariant VoiceBankEntryModel::headerData(int section, Qt::Orientation orientation,
                                             int role) const {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
            return QAbstractTableModel::headerData(section, orientation, role);
        }
        switch (section) {
            case DirectoryColumn:
                return tr("Folder");
            case FileColumn:
                return tr("File");
            case AliasColumn:
                return tr("Alias");
            case OffsetColumn:
                return tr("Offset");
            case ConsonantColumn:
                return tr("Consonant");
            case CutoffColumn:
                return tr("Cutoff");
            case PreUtteranceColumn:
                return tr("Pre-utterance");
            case OverlapColumn:
                return tr("Overlap");
            default:
                return {};
        }
    }

}
