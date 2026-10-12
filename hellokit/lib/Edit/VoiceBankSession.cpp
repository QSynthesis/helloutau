#include "VoiceBankSession.h"

#include <algorithm>
#include <utility>

#include <hellokit/EditBase/private/ChangeLog_p.h>
#include <hellokit/EditBase/private/EditSession_p.h>

#include "VoiceBankFields_p.h"
#include "VoiceBankTree_p.h"
#include "VoiceBankValidation_p.h"

namespace hello::kit {

    std::unique_ptr<VoiceBankSession>
        VoiceBankSession::create(VoiceBankFileSystemState::Opened opened,
                                 DiagnosticList &diagnostics, QObject *parent) {
        const auto root = opened.bank.indexOf({});
        if (root < 0 || !isEditable(opened.bank.directories().at(root))) {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Error;
            diagnostic.message = tr("The voice bank cannot be edited, because no encoding was "
                                    "specified for its folder.");
            diagnostics.push_back(diagnostic);
            return nullptr;
        }
        return std::unique_ptr<VoiceBankSession>(new VoiceBankSession(std::move(opened), parent));
    }

    VoiceBankSession::VoiceBankSession(VoiceBankFileSystemState::Opened opened, QObject *parent)
        : edit::EditSession(parent), m_files(std::move(opened.files)) {
        for (const auto &directory : opened.bank.directories()) {
            if (!isEditable(directory)) {
                m_excluded.push_back(directory);
            }
        }
        edit::EditSessionPrivate::setRoot(*this, treeOf(opened.bank));
        registerVoiceBankValidators(*this);

        m_baseStep = currentStep();
        edit::EditSessionPrivate::setCommitHandler(
            *this, [this](int step, const std::vector<edit::ChangePtr> &changes) {
                takeCommit(step, changes);
            });
        connect(this, &edit::EditSession::stepsDiscarded, this,
                [this](int first, int last) { discardSteps(first, last); });
    }

    VoiceBankSession::~VoiceBankSession() = default;

    const std::filesystem::path &VoiceBankSession::rootPath() const {
        return m_files.root();
    }

    VoiceBank VoiceBankSession::snapshot() const {
        return voiceBankOf(edit::EditSessionPrivate::find(this, root()), m_files);
    }

    std::optional<QJsonObject> VoiceBankSession::logEntry(const edit::Change &change) const {
        return edit::ChangeLog::entryOf(*this, change, voiceBankRecordOf);
    }

    namespace {

        // Returns the list of directories of the tree of session.
        const ss::VectorNode &directoriesOf(const edit::EditSession &session) {
            const auto root = edit::EditSessionPrivate::find<VoiceBankNode>(
                &session, session.root(), VoiceBankType);
            return static_cast<const ss::VectorNode &>(
                *root->child(VoiceBankSlots::Directories.index));
        }

        // Returns the index of the directory at path in directories, or -1.
        int indexIn(const ss::VectorNode &directories, const std::filesystem::path &path) {
            for (int i = 0; i < directories.size(); ++i) {
                if (directoryPathOf(directories.at(i)) == path) {
                    return i;
                }
            }
            return -1;
        }

        // Returns the text files that the directory at directory may have.
        std::vector<VoiceBankDirectorySource::File>
            textFilesOf(const std::filesystem::path &directory) {
            if (directory.empty()) {
                return {VoiceBankDirectorySource::Oto, VoiceBankDirectorySource::PrefixMap,
                        VoiceBankDirectorySource::Character, VoiceBankDirectorySource::Readme};
            }
            return {VoiceBankDirectorySource::Oto};
        }

        // Returns the files of the directory at directory that a save writes, the configuration
        // included.
        std::vector<VoiceBankDirectorySource::File>
            filesOf(const std::filesystem::path &directory) {
            auto files = textFilesOf(directory);
            files.push_back(VoiceBankDirectorySource::Config);
            return files;
        }

    }

    std::optional<VoiceBankConfiguration::FileNameEncoding>
        VoiceBankSession::fileNameEncodingOf(const std::filesystem::path &directory) const {
        const auto &directories = directoriesOf(*this);
        const int index = indexIn(directories, directory);
        if (index >= 0) {
            return kit::fileNameEncodingOf(directories.at(index));
        }
        for (const auto &excluded : m_excluded) {
            if (excluded.path == directory) {
                return excluded.fileNameEncoding;
            }
        }
        return std::nullopt;
    }

    QStringList VoiceBankSession::audioFiles(const std::filesystem::path &directory) const {
        return m_files.audioFiles(directory, fileNameEncodingOf(directory));
    }

    std::filesystem::path VoiceBankSession::audioPathOf(const std::filesystem::path &directory,
                                                        const QString &fileName) const {
        return m_files.audioPathOf(directory, fileNameEncodingOf(directory), fileName);
    }

    QStringList VoiceBankSession::undeterminedNames(const std::filesystem::path &directory) const {
        return m_files.undeterminedNames(directory, fileNameEncodingOf(directory));
    }

    std::filesystem::path
        VoiceBankSession::logicalPathOf(const std::filesystem::path &directory) const {
        std::filesystem::path logical;
        std::filesystem::path parent;
        for (const auto &component : directory) {
            auto name = QString::fromStdU16String(component.u16string());
            if (const auto record = fileNameEncodingOf(parent)) {
                name = record->logicalNameOf(name);
            }
            logical /= std::filesystem::path(name.toStdU16String());
            parent /= component;
        }
        return logical;
    }

    QList<MisdecodedNameConversion::Listing> VoiceBankSession::nameListings() const {
        const auto bank = fullBank();
        const auto &directories = bank.directories();
        QList<MisdecodedNameConversion::Listing> listings;
        for (const auto &directory : directories) {
            auto listing = m_files.nameListingOf(directory.path);
            listing.charset = directory.charset;
            listing.record = directory.fileNameEncoding;
            listings.push_back(listing);
        }
        for (const auto &sample : bank.samples()) {
            if (sample.hasEntry && sample.directory >= 0 && sample.directory < listings.size()) {
                listings[sample.directory].entries.push_back(sample.fileName);
            }
        }
        std::sort(listings.begin(), listings.end(),
                  [](const auto &a, const auto &b) { return a.directory < b.directory; });
        return listings;
    }

    QList<VoiceBankDirectory> VoiceBankSession::excludedDirectories() const {
        // An undo can restore a directory that a later reading excluded, which the tree then
        // holds again.
        const auto &directories = directoriesOf(*this);
        QList<VoiceBankDirectory> excluded;
        for (const auto &directory : m_excluded) {
            if (indexIn(directories, directory.path) < 0) {
                excluded.push_back(directory);
            }
        }
        return excluded;
    }

    VoiceBank VoiceBankSession::fullBank() const {
        const auto bank = snapshot();
        auto directories = bank.directories();
        directories += excludedDirectories();
        return VoiceBank(bank.root(), directories, bank.samples());
    }

    bool VoiceBankSession::isIncomplete() const {
        return m_rootMissing || m_rootUnreadable;
    }

    bool VoiceBankSession::save(DiagnosticList &diagnostics) {
        const auto files = unsavedFiles();
        if (!m_files.save(snapshot(), files, diagnostics)) {
            return false;
        }
        const int step = currentStep();
        if (m_rootMissing) {
            // Without the root, every text file of the tree was written.
            m_synced.clear();
            m_unsynced.clear();
            m_baseStep = step;
        } else {
            for (const auto &[directory, kinds] : files) {
                for (const auto file : kinds) {
                    const FileKey key{directory, file};
                    m_unsynced.erase(key);
                    m_synced[key] = step;
                }
            }
        }
        // The files on disk are those of the tree again.
        m_rootMissing = false;
        m_rootUnreadable = false;
        return true;
    }

    bool VoiceBankSession::saveAs(const std::filesystem::path &folder, SaveAsFiles files,
                                  DiagnosticList &diagnostics) {
        auto saved =
            VoiceBankFileSystemState::saveAs(snapshot(), folder, files == AllFiles, diagnostics);
        if (!saved) {
            return false;
        }
        m_files = std::move(saved->files);
        m_excluded.clear();
        for (const auto &directory : saved->bank.directories()) {
            if (!isEditable(directory) && !directory.path.empty()) {
                m_excluded.push_back(directory);
            }
        }
        m_rootMissing = false;
        m_rootUnreadable = false;
        // Every file of the new folder holds the tree.
        m_synced.clear();
        m_unsynced.clear();
        m_baseStep = currentStep();
        return true;
    }

    bool VoiceBankSession::renameOnDisk(const QList<NameConversion> &conversions,
                                        const FrequencyFormats *formats,
                                        DiagnosticList &diagnostics) {
        namespace fs = std::filesystem;
        using FileNameEncoding = VoiceBankConfiguration::FileNameEncoding;
        const auto bank = snapshot();
        const auto normalized = [](const QString &name) {
            return name.normalized(QString::NormalizationForm_C);
        };
        const auto lists = [&normalized](const QStringList &list, const QString &name) {
            return std::any_of(list.begin(), list.end(), [&](const QString &item) {
                return normalized(item) == normalized(name);
            });
        };

        QList<VoiceBankFileSystemState::NameChanges> changes;
        bool valid = true;
        for (const auto &conversion : conversions) {
            const auto &record = conversion.record;
            const auto directory = bank.root() / conversion.directory;
            VoiceBankFileSystemState::NameChanges change{conversion.directory, {}, {}};
            QList<FrequencyFormat::AudioRename> audio;
            QStringList restoredNames;
            for (const auto &name : conversion.renamed) {
                const auto restored = record.restore(name);
                if (!restored) {
                    diagnostics.push_back(
                        {DiagnosticSeverity::Error,
                         tr("The name \"%1\" does not restore, so it cannot be renamed.").arg(name)});
                    valid = false;
                    continue;
                }
                const fs::path from(name.toStdU16String());
                const fs::path to(restored->toStdU16String());
                change.renames.push_back({from, to});
                restoredNames.push_back(*restored);
                std::error_code error;
                if (!fs::is_directory(directory / from, error)) {
                    audio.push_back({from, to});
                }
            }

            // The frequency maps that follow the renamed audio files. A format that cannot follow
            // them is reported only if one of them has a map of it.
            if (formats && !audio.isEmpty()) {
                QList<fs::path> renamedAudio;
                for (const auto &rename : std::as_const(audio)) {
                    renamedAudio.push_back(rename.from);
                }
                for (const auto format : formats->formats()) {
                    DiagnosticList reasons;
                    if (const auto files = format->changesRenaming(directory, audio, reasons)) {
                        change.files += *files;
                    } else if (format->mapsExist(directory, renamedAudio).contains(true)) {
                        diagnostics += reasons;
                    }
                }
            }

            // The record with the renamed names as correct. Every name that is not ASCII is
            // determined, a name in neither list as correct.
            FileNameEncoding all = record;
            all.files = FileNameEncoding::Files();
            QStringList misdecoded;
            QStringList correct = restoredNames;
            for (const auto &name : m_files.undeterminedNames(conversion.directory, all)) {
                if (lists(conversion.renamed, name)) {
                    continue;
                }
                const bool isMisdecoded =
                    record.files ? lists(record.files->misdecoded, name) : !record.isAsIs();
                (isMisdecoded ? misdecoded : correct).push_back(name);
            }
            auto config = m_files.configurationFileOf(bank, conversion.directory);
            config.configuration.fileNameEncoding = FileNameEncoding::normalized(
                record.original, record.misdecodedAs, misdecoded, correct);
            change.files.push_back(
                FrequencyFormat::FileChange::rewritten(config.name, config.configuration.toJson()));
            changes.push_back(change);
        }
        if (!valid) {
            return false;
        }
        return VoiceBankFileSystemState::renameOnDisk(bank.root(), changes, diagnostics);
    }

    void VoiceBankSession::rememberCharset(const std::filesystem::path &directory) {
        m_unsynced.insert({directory, VoiceBankDirectorySource::Config});
    }

    bool VoiceBankSession::hasUnrecordedCharsets() const {
        return std::any_of(m_unsynced.begin(), m_unsynced.end(), [](const FileKey &key) {
            return key.second == VoiceBankDirectorySource::Config;
        });
    }

    void VoiceBankSession::takeCommit(int step, const std::vector<edit::ChangePtr> &changes) {
        auto &touched = m_touched[step];
        for (const auto &change : changes) {
            addTouched(*change, touched);
        }
        // A directory read from disk replaces or adds a whole subtree. Its encoding is kept, or
        // chosen by the user, which takeFrom() records as an invalid synchronization point.
        for (const auto &directory : m_reading) {
            for (const auto file : textFilesOf(directory)) {
                touched.insert({directory, file});
            }
        }
    }

    void VoiceBankSession::addTouched(const edit::Change &change,
                                      std::set<FileKey> &touched) const {
        // A node that a later change of the same transaction removed is no longer in the tree,
        // and that removal from its list touches the same file.
        const ss::Node *node = edit::EditSessionPrivate::find(this, change.node());
        const ss::Node *below = nullptr;
        const auto value = change.as<edit::ValueChange>();
        for (; node; below = node, node = node->parent()) {
            if (node->type() == VoiceDirectoryType) {
                const auto path = directoryPathOf(node);
                if (below || !value || value->slot() == VoiceDirectorySlots::OtoEntries.index) {
                    touched.insert({path, VoiceBankDirectorySource::Oto});
                } else if (value->slot() == VoiceDirectorySlots::Charset.index) {
                    for (const auto file : filesOf(path)) {
                        touched.insert({path, file});
                    }
                } else if (value->slot() == VoiceDirectorySlots::FileNameEncoding.index) {
                    touched.insert({path, VoiceBankDirectorySource::Config});
                }
                return;
            }
            if (node->type() == VoiceBankType) {
                // The files of the root belong to the voice bank as a whole. The list of the
                // directories changes by reading only, which takeCommit() covers.
                const auto &root = static_cast<const VoiceBankNode &>(*node);
                int slot = value && !below ? value->slot() : -1;
                if (below && below == root.child(VoiceBankSlots::Character.index)) {
                    slot = VoiceBankSlots::Character.index;
                } else if (below && below == root.child(VoiceBankSlots::PrefixMap.index)) {
                    slot = VoiceBankSlots::PrefixMap.index;
                }
                if (slot == VoiceBankSlots::Character.index) {
                    touched.insert({{}, VoiceBankDirectorySource::Character});
                } else if (slot == VoiceBankSlots::PrefixMap.index) {
                    touched.insert({{}, VoiceBankDirectorySource::PrefixMap});
                } else if (slot == VoiceBankSlots::Readme.index) {
                    touched.insert({{}, VoiceBankDirectorySource::Readme});
                }
                return;
            }
        }
    }

    void VoiceBankSession::discardSteps(int first, int last) {
        const auto within = [first, last](int step) { return step >= first && step <= last; };
        for (const auto &[key, step] : m_synced) {
            if (within(step)) {
                m_unsynced.insert(key);
            }
        }
        if (within(m_baseStep)) {
            const auto &directories = directoriesOf(*this);
            for (int i = 0; i < directories.size(); ++i) {
                const auto path = directoryPathOf(directories.at(i));
                for (const auto file : filesOf(path)) {
                    if (m_synced.count({path, file}) == 0) {
                        m_unsynced.insert({path, file});
                    }
                }
            }
        }
        m_touched.erase(m_touched.lower_bound(first), m_touched.upper_bound(last));
    }

    int VoiceBankSession::syncedStep(const FileKey &key) const {
        const auto found = m_synced.find(key);
        return found != m_synced.end() ? found->second : m_baseStep;
    }

    VoiceBankFileSystemState::Files VoiceBankSession::unsavedFiles() const {
        VoiceBankFileSystemState::Files files;
        for (const auto &[directory, file] : m_unsynced) {
            files[directory].insert(file);
        }
        // A step lies between the synchronization point and the current step if it was applied
        // since the file was synchronized, or undone since then.
        const int current = currentStep();
        for (const auto &[step, keys] : m_touched) {
            for (const auto &key : keys) {
                const int synced = syncedStep(key);
                if ((synced < step && step <= current) || (current < step && step <= synced)) {
                    files[key.first].insert(key.second);
                }
            }
        }
        return files;
    }

    VoiceBankChanges VoiceBankSession::checkDisk(const QList<std::filesystem::path> &places) {
        return withUntaken(m_files.checkDisk(places));
    }

    VoiceBankChanges VoiceBankSession::checkDisk() {
        return withUntaken(m_files.checkDisk());
    }

    VoiceBankChanges VoiceBankSession::withUntaken(VoiceBankChanges changes) {
        // A root restored on disk makes the voice bank complete again, unless it no longer
        // reads, which only reading it again or writing it resolves.
        m_rootMissing = changes.rootNotFound;
        if (changes.rootNotFound) {
            return changes;
        }
        const auto &directories = directoriesOf(*this);
        const auto excluded = excludedDirectories();
        for (const auto &path : m_files.directories()) {
            const bool known = indexIn(directories, path) >= 0 ||
                               std::any_of(excluded.begin(), excluded.end(),
                                           [&path](const VoiceBankDirectory &directory) {
                                               return directory.path == path;
                                           });
            if (!known && !changes.removed.contains(path)) {
                changes.added.push_back(path);
            }
        }
        return changes;
    }

    VoiceBankChanges VoiceBankSession::reloadFromDisk(const VoiceBankChanges &changes,
                                                      VoiceBankCharsetSelector *selector,
                                                      DiagnosticList &diagnostics) {
        auto bank = fullBank();
        const auto done = m_files.reloadFromDisk(bank, changes, selector, diagnostics);
        takeFrom(bank, done.changed, tr("Reload from Disk"), false, diagnostics);
        return done;
    }

    VoiceBankChanges VoiceBankSession::reloadAllFromDisk(VoiceBankCharsetSelector *selector,
                                                         DiagnosticList &diagnostics) {
        auto bank = fullBank();
        const auto done = m_files.reloadAllFromDisk(bank, selector, diagnostics);
        takeFrom(bank, done.changed, tr("Reload from Disk"), false, diagnostics);
        return done;
    }

    bool VoiceBankSession::reread(const std::filesystem::path &directory, const QString &charset,
                                  DiagnosticList &diagnostics) {
        return reread(
            {
                {directory, charset}
        },
            diagnostics);
    }

    bool VoiceBankSession::reread(const QList<std::pair<std::filesystem::path, QString>> &charsets,
                                  DiagnosticList &diagnostics) {
        auto bank = fullBank();
        QList<std::filesystem::path> replaced;
        for (const auto &[directory, charset] : charsets) {
            if (m_files.reread(bank, directory, charset, diagnostics)) {
                replaced.push_back(directory);
            }
        }
        if (replaced.isEmpty()) {
            return false;
        }
        const auto message = charsets.size() == 1
                                 ? tr("Read Again in %1").arg(charsets.first().second)
                                 : tr("Read Again in Encodings");
        return takeFrom(bank, replaced, message, true, diagnostics) &&
               replaced.size() == charsets.size();
    }

    bool VoiceBankSession::takeFrom(const VoiceBank &bank,
                                    const QList<std::filesystem::path> &replaced,
                                    const QString &message, bool recordCharsets,
                                    DiagnosticList &diagnostics) {
        auto transaction = this->transaction(message);
        bool rootKept = false;
        edit::EditSessionPrivate::markAsRead(*this);
        const auto root = edit::EditSessionPrivate::findEditable<VoiceBankNode>(this, this->root(),
                                                                                VoiceBankType);
        const auto directories = edit::EditSessionPrivate::findEditable<ss::VectorNode>(
            this, root->child(VoiceBankSlots::Directories.index)->id(), ss::Node::Vector);

        // The directories gone from the voice bank or no longer editable, from the last. The
        // root stays, as a session requires it: if it no longer reads, the tree keeps what was
        // read before, which saving writes over the files.
        for (int i = directories->size() - 1; i >= 0; --i) {
            const auto path = directoryPathOf(directories->at(i));
            const auto index = bank.indexOf(path);
            if (index >= 0 && isEditable(bank.directories().at(index))) {
                continue;
            }
            if (path.empty()) {
                Diagnostic diagnostic;
                diagnostic.severity = DiagnosticSeverity::Warning;
                diagnostic.message = tr("The folder of the voice bank was not read again, because "
                                        "no encoding was selected for a text file in it, so the "
                                        "voice bank on disk is incomplete. The contents read "
                                        "before are kept, and saving writes them back.");
                diagnostics.push_back(diagnostic);
                m_rootUnreadable = true;
                rootKept = true;
                continue;
            }
            directories->remove(i, 1);
        }

        // The directories read again take the place of their former trees, and the new ones
        // follow the others.
        for (int i = 0; i < bank.directories().size(); ++i) {
            const auto &directory = bank.directories().at(i);
            if (!isEditable(directory)) {
                continue;
            }
            const auto at = indexIn(*directories, directory.path);
            if (at < 0) {
                std::vector<std::unique_ptr<ss::Node>> nodes;
                nodes.push_back(directoryTreeOf(bank, i));
                directories->insert(directories->size(), std::move(nodes));
                m_reading.insert(directory.path);
            } else if (replaced.contains(directory.path)) {
                directories->remove(at, 1);
                std::vector<std::unique_ptr<ss::Node>> nodes;
                nodes.push_back(directoryTreeOf(bank, i));
                directories->insert(at, std::move(nodes));
                m_reading.insert(directory.path);
            }
            if (directory.path.empty() && replaced.contains(directory.path)) {
                setRootFiles(*root, directory);
                m_rootUnreadable = false;
            }
        }

        m_excluded.clear();
        for (const auto &directory : bank.directories()) {
            if (!isEditable(directory) && !directory.path.empty()) {
                m_excluded.push_back(directory);
            }
        }
        const bool committed = transaction.commit(diagnostics);
        const auto read = std::exchange(m_reading, {});
        if (!committed) {
            return false;
        }

        // The directories read hold what is on disk now, except a configuration that is to
        // record an encoding the user chose, and a root kept because it no longer reads.
        const int step = currentStep();
        for (const auto &directory : read) {
            for (const auto file : filesOf(directory)) {
                const FileKey key{directory, file};
                if (recordCharsets && file == VoiceBankDirectorySource::Config) {
                    m_unsynced.insert(key);
                } else {
                    m_unsynced.erase(key);
                    m_synced[key] = step;
                }
            }
        }
        if (rootKept) {
            for (const auto file : textFilesOf({})) {
                m_unsynced.insert({{}, file});
            }
        }
        return true;
    }

}
