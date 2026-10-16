#include "VoiceBankEntryModel.h"

#include <map>

#include <QtCore/QHash>
#include <QtCore/QLocale>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtGui/QBrush>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>

#include <stdcorelib/pimpl.h>

#include <hellokit/Edit/VoiceBankEdits.h>
#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/Edit/VoiceBankSession.h>
#include <hellokit/VoiceBank/FrequencyFormat.h>

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

        // The name under which an entry is matched, as the validation of the edit layer counts
        // it: the alias, or the stem of the file name without one
        QString matchedName(const kit::VoiceOtoEntry &entry) {
            if (!entry.alias.isEmpty()) {
                return entry.alias;
            }
            return QString::fromStdU16String(
                std::filesystem::path(entry.fileName.toStdU16String()).stem().u16string());
        }

        double &valueOf(kit::VoiceOtoEntry &entry, int column) {
            switch (column) {
                case VoiceBankEntryModel::OffsetColumn:
                    return entry.offset;
                case VoiceBankEntryModel::ConsonantColumn:
                    return entry.consonant;
                case VoiceBankEntryModel::CutoffColumn:
                    return entry.cutoff;
                case VoiceBankEntryModel::PreUtteranceColumn:
                    return entry.preUtterance;
                default:
                    return entry.voiceOverlap;
            }
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
            // The index of the folder in the tree, and of the entry in the folder, or -1
            int directoryIndex = 0;
            int entryIndex = -1;
            kit::VoiceOtoEntry entry;
            bool duplicate = false;
            // Whether the audio file has a map, none without a format or an audio file
            std::optional<bool> map;
            // Whether the audio file has each cache of caches, empty without an audio file
            QList<bool> caches;
        };

        // Whether an audio file has a map of format, and each cache of the format
        struct Analysis {
            bool map = false;
            QList<bool> caches;
        };

        Decl *_decl;
        kit::VoiceBankSession *session;
        std::optional<std::filesystem::path> directory;
        QList<Row> rows;
        bool refreshPending = false;
        const kit::FrequencyFormat *format = nullptr;
        // The analysis caches of format, one column each
        QList<kit::FrequencyFormat::AnalysisCache> caches;
        // The analysis of each audio file of a folder, by the key of its name
        std::map<std::filesystem::path, QHash<QString, Analysis>> analyses;

        // The analyses of the audio files of the folder at path, queried at once on first use
        const QHash<QString, Analysis> &analysesOf(const std::filesystem::path &path,
                                                   const QStringList &files) {
            auto found = analyses.find(path);
            if (found != analyses.end()) {
                return found->second;
            }
            auto &result = analyses[path];
            if (files.isEmpty()) {
                return result;
            }
            const auto folder = session->audioPathOf(path, files.first()).parent_path();
            QList<std::filesystem::path> names;
            for (const auto &file : files) {
                names.push_back(session->audioPathOf(path, file).filename());
            }
            const auto exist = format->mapsExist(folder, names);
            for (qsizetype i = 0; i < files.size(); ++i) {
                Analysis analysis;
                analysis.map = exist.value(i);
                for (const auto &cache : std::as_const(caches)) {
                    std::error_code error;
                    analysis.caches.push_back(std::filesystem::is_regular_file(
                        folder / cache.fileNameOf(names.at(i)), error));
                }
                result.insert(fileKey(files.at(i)), analysis);
            }
            return result;
        }

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
                QHash<QString, int> names;
                const auto first = built.size();
                const auto entries = ref.otoEntries();
                for (int e = 0; e < entries.size(); ++e) {
                    Row row;
                    row.directory = path;
                    row.directoryIndex = d;
                    row.entryIndex = e;
                    row.entry = entries.at(e).toVoiceOtoEntry();
                    const auto key = fileKey(row.entry.fileName);
                    listed.insert(key);
                    row.kind = audio.contains(key) ? EntryRow : MissingAudioRow;
                    ++names[row.entry.fileName + QChar(0) + matchedName(row.entry)];
                    built.push_back(row);
                }
                for (auto i = first; i < built.size(); ++i) {
                    auto &row = built[i];
                    row.duplicate =
                        names.value(row.entry.fileName + QChar(0) + matchedName(row.entry)) > 1;
                }
                for (const auto &file : files) {
                    if (!listed.contains(fileKey(file))) {
                        Row row;
                        row.kind = UnlistedAudioRow;
                        row.directory = path;
                        row.directoryIndex = d;
                        row.entry.fileName = file;
                        built.push_back(row);
                    }
                }
                if (format) {
                    const auto &found = analysesOf(path, files);
                    for (auto i = first; i < built.size(); ++i) {
                        auto &row = built[i];
                        if (row.kind != MissingAudioRow) {
                            const auto analysis = found.value(fileKey(row.entry.fileName));
                            row.map = analysis.map;
                            row.caches = analysis.caches;
                        }
                    }
                }
            }
            stdc_decl_t;
            // The same number of rows is updated in place, which keeps the selection of a view
            // through an edit of a cell or a value.
            if (built.size() == rows.size() && !rows.isEmpty()) {
                rows = std::move(built);
                Q_EMIT decl.dataChanged(decl.index(0, 0),
                                        decl.index(int(rows.size()) - 1, decl.columnCount() - 1));
                return;
            }
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

        // The value of column of an entry as the file records it while the spelling still reads
        // as the value, as saving keeps it, or else in the shortest form
        static QString valueText(const kit::VoiceOtoEntry &entry, int column) {
            const int field = column - OffsetColumn;
            const double values[] = {entry.offset, entry.consonant, entry.cutoff,
                                     entry.preUtterance, entry.voiceOverlap};
            if (const auto &spelling = entry.spellings[size_t(field)]) {
                const auto text = QString::fromStdString(*spelling);
                bool ok = false;
                if (QLocale::c().toDouble(text.trimmed(), &ok) == values[field] && ok) {
                    return text;
                }
            }
            return QLocale::c().toString(values[field], 'g', QLocale::FloatingPointShortest);
        }

        static void fail(kit::DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({kit::Diagnostic::Error, message, std::nullopt});
        }

        // The entry of row in the tree: at its index while the tree holds it there, else the
        // first equal entry of its folder, as the rows may not yet follow the latest change
        std::optional<kit::OtoEntryRef> entryRefOf(const Row &row) const {
            const auto directories = kit::VoiceBankRef(session).directories();
            if (row.directoryIndex >= directories.size() ||
                directories.at(row.directoryIndex).path() != row.directory) {
                return std::nullopt;
            }
            const auto list = directories.at(row.directoryIndex).otoEntries();
            if (row.entryIndex < list.size() &&
                list.at(row.entryIndex).toVoiceOtoEntry() == row.entry) {
                return list.at(row.entryIndex);
            }
            for (int i = 0; i < list.size(); ++i) {
                if (list.at(i).toVoiceOtoEntry() == row.entry) {
                    return list.at(i);
                }
            }
            return std::nullopt;
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
        // Other rows, never updated in place
        beginResetModel();
        impl.rows.clear();
        endResetModel();
        impl.build();
    }

    void VoiceBankEntryModel::refresh() {
        stdc_impl_t;
        impl.refreshPending = false;
        impl.build();
    }

    const kit::FrequencyFormat *VoiceBankEntryModel::frequencyFormat() const {
        stdc_impl_t;
        return impl.format;
    }

    void VoiceBankEntryModel::setFrequencyFormat(const kit::FrequencyFormat *format) {
        stdc_impl_t;
        if (format == impl.format) {
            return;
        }
        // The cache columns are inserted or removed rather than the model reset, which keeps the
        // selection of a view.
        const auto caches =
            format ? format->analysisCaches() : QList<kit::FrequencyFormat::AnalysisCache>();
        const auto before = int(impl.caches.size());
        const auto after = int(caches.size());
        if (after < before) {
            beginRemoveColumns({}, CacheColumn + after, CacheColumn + before - 1);
        } else if (after > before) {
            beginInsertColumns({}, CacheColumn + before, CacheColumn + after - 1);
        }
        impl.format = format;
        impl.caches = caches;
        for (auto &row : impl.rows) {
            row.caches.clear();
        }
        if (after < before) {
            endRemoveColumns();
        } else if (after > before) {
            endInsertColumns();
        }
        if (after > 0) {
            Q_EMIT headerDataChanged(Qt::Horizontal, CacheColumn, CacheColumn + after - 1);
        }
        refreshFrequencyMaps();
    }

    void VoiceBankEntryModel::refreshFrequencyMaps() {
        stdc_impl_t;
        impl.analyses.clear();
        refresh();
    }

    int VoiceBankEntryModel::rowOf(const std::filesystem::path &directory, const QString &fileName,
                                   const QString &alias) const {
        stdc_impl_t;
        for (int i = 0; i < impl.rows.size(); ++i) {
            const auto &row = impl.rows.at(i);
            if (row.directory == directory && fileKey(row.entry.fileName) == fileKey(fileName) &&
                row.entry.alias == alias) {
                return i;
            }
        }
        return -1;
    }

    std::filesystem::path VoiceBankEntryModel::directoryOf(int row) const {
        stdc_impl_t;
        return impl.rows.value(row).directory;
    }

    int VoiceBankEntryModel::entryIndexOf(int row) const {
        stdc_impl_t;
        return row >= 0 && row < impl.rows.size() ? impl.rows.at(row).entryIndex : -1;
    }

    kit::VoiceOtoEntry VoiceBankEntryModel::entryOf(int row) const {
        stdc_impl_t;
        return impl.rows.value(row).entry;
    }

    bool VoiceBankEntryModel::setEntry(int row, const kit::VoiceOtoEntry &value,
                                       kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        if (row < 0 || row >= impl.rows.size()) {
            Impl::fail(diagnostics, tr("The table has no row %1.").arg(row));
            return false;
        }
        const auto shown = impl.rows.at(row);
        if (shown.kind != UnlistedAudioRow) {
            const auto entry = impl.entryRefOf(shown);
            if (!entry) {
                Impl::fail(diagnostics, tr("The entry was changed meanwhile."));
                return false;
            }
            return kit::VoiceBankEdits::setEntry(*entry, value, diagnostics);
        }

        const auto directories = kit::VoiceBankRef(impl.session).directories();
        if (shown.directoryIndex >= directories.size() ||
            directories.at(shown.directoryIndex).path() != shown.directory) {
            Impl::fail(diagnostics, tr("The folder was changed meanwhile."));
            return false;
        }
        const auto directory = directories.at(shown.directoryIndex);
        auto transaction = impl.session->transaction(tr("Include Audio File"));
        if (!kit::VoiceBankEdits::includeAudio(directory, {shown.entry.fileName}, diagnostics)) {
            return false;
        }
        const auto list = directory.otoEntries();
        for (int i = 0; i < list.size(); ++i) {
            if (list.at(i).fileName() == shown.entry.fileName) {
                if (!kit::VoiceBankEdits::setEntry(list.at(i), value, diagnostics)) {
                    return false;
                }
                break;
            }
        }
        return transaction.commit(diagnostics);
    }

    int VoiceBankEntryModel::rowCount(const QModelIndex &parent) const {
        stdc_impl_t;
        return parent.isValid() ? 0 : int(impl.rows.size());
    }

    int VoiceBankEntryModel::columnCount(const QModelIndex &parent) const {
        stdc_impl_t;
        return parent.isValid() ? 0 : CacheColumn + int(impl.caches.size());
    }

    QVariant VoiceBankEntryModel::data(const QModelIndex &index, int role) const {
        stdc_impl_t;
        if (!index.isValid() || index.row() >= impl.rows.size()) {
            return {};
        }
        const auto &row = impl.rows.at(index.row());
        const int cache = index.column() - CacheColumn;
        if (cache >= 0) {
            switch (role) {
                case Qt::DisplayRole:
                case Qt::EditRole:
                case SortRole:
                    // A check mark for a cache file, nothing for none
                    if (row.caches.value(cache)) {
                        return QString(QChar(0x2713));
                    }
                    return {};
                case Qt::TextAlignmentRole:
                    return QVariant::fromValue(Qt::AlignCenter);
                case Qt::ToolTipRole:
                    if (cache < row.caches.size() && !row.caches.at(cache)) {
                        return tr("The audio file has no analysis cache %1 of the format %2.")
                            .arg(impl.caches.value(cache).name, impl.format->name());
                    }
                    break;
                default:
                    break;
            }
        }
        switch (role) {
            case Qt::DisplayRole:
            case Qt::EditRole:
                switch (index.column()) {
                    case DirectoryColumn:
                        return QString::fromStdU16String(
                            impl.session->logicalPathOf(row.directory).generic_u16string());
                    case FileColumn:
                        return row.entry.fileName;
                    case AliasColumn:
                        return row.entry.alias;
                    case FrequencyMapColumn:
                        // A check mark for a map, nothing for none
                        if (row.map == true) {
                            return QString(QChar(0x2713));
                        }
                        return {};
                    default:
                        return row.kind == UnlistedAudioRow
                                   ? QVariant()
                                   : Impl::valueText(row.entry, index.column());
                }
            case Qt::TextAlignmentRole:
                if (index.column() >= OffsetColumn && index.column() <= OverlapColumn) {
                    return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
                }
                if (index.column() == FrequencyMapColumn) {
                    return QVariant::fromValue(Qt::AlignCenter);
                }
                return {};
            case Qt::ForegroundRole:
                if (row.kind == UnlistedAudioRow) {
                    return QGuiApplication::palette().brush(QPalette::Disabled, QPalette::Text);
                }
                if (row.duplicate && index.column() == AliasColumn) {
                    return QBrush(QColor(0xd0, 0x30, 0x30));
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
                if (index.column() == FrequencyMapColumn && row.map == false) {
                    return tr("The audio file has no frequency map of the format %1.")
                        .arg(impl.format->name());
                }
                if (row.duplicate) {
                    return tr("Another entry of this audio file has the same alias. UTAU uses "
                              "only one of them, and an edit that adds such an alias is "
                              "rejected.");
                }
                switch (row.kind) {
                    case MissingAudioRow:
                        return tr("The audio file of this entry does not exist.");
                    case UnlistedAudioRow:
                        return tr("This audio file has no entry. Editing its alias or a value "
                                  "includes it.");
                    default:
                        return {};
                }
            case RowKindRole:
                return row.kind;
            case DuplicateAliasRole:
                return row.duplicate;
            case SortRole: {
                const double values[] = {row.entry.offset, row.entry.consonant, row.entry.cutoff,
                                         row.entry.preUtterance, row.entry.voiceOverlap};
                if (index.column() >= OffsetColumn && index.column() <= OverlapColumn) {
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
        stdc_impl_t;
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
            return QAbstractTableModel::headerData(section, orientation, role);
        }
        if (section >= CacheColumn) {
            return impl.caches.value(section - CacheColumn).name;
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
            case FrequencyMapColumn:
                return tr("Frequency Map");
            default:
                return {};
        }
    }

    Qt::ItemFlags VoiceBankEntryModel::flags(const QModelIndex &index) const {
        stdc_impl_t;
        auto flags = QAbstractTableModel::flags(index);
        if (!index.isValid() || index.row() >= impl.rows.size()) {
            return flags;
        }
        // The folder is where the file is, and an unlisted row is the file itself. The map and
        // the caches cannot be edited.
        const auto kind = impl.rows.at(index.row()).kind;
        if (index.column() == DirectoryColumn || index.column() == FileColumn ||
            index.column() >= FrequencyMapColumn) {
            return flags;
        }
        return flags | Qt::ItemIsEditable;
    }

    bool VoiceBankEntryModel::setData(const QModelIndex &index, const QVariant &value, int role) {
        if (role != Qt::EditRole || !(flags(index) & Qt::ItemIsEditable)) {
            return false;
        }
        const int row = index.row();
        const auto old = entryOf(row);
        auto entry = old;
        const auto text = value.toString();
        kit::DiagnosticList diagnostics;
        switch (index.column()) {
            case FileColumn:
                entry.fileName = text;
                break;
            case AliasColumn:
                entry.alias = text;
                break;
            default: {
                // The empty cell of an unlisted row, left as it was
                if (text.trimmed().isEmpty() && !index.data().isValid()) {
                    return false;
                }
                bool ok = false;
                const double number = QLocale::c().toDouble(text.trimmed(), &ok);
                if (!ok) {
                    Impl::fail(diagnostics, tr("\"%1\" is not a number.").arg(text));
                    Q_EMIT editRejected(diagnostics);
                    return false;
                }
                valueOf(entry, index.column()) = number;
                break;
            }
        }
        if (entry == old) {
            return true;
        }
        if (!setEntry(row, entry, diagnostics)) {
            Q_EMIT editRejected(diagnostics);
            return false;
        }
        refresh();
        return true;
    }

}
