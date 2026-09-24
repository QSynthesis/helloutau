#include "VoiceBankSession.h"

#include <algorithm>

#include <hellokit/EditBase/private/ChangeLog_p.h>
#include <hellokit/EditBase/private/EditSession_p.h>

#include "VoiceBankFields_p.h"
#include "VoiceBankTree_p.h"
#include "VoiceBankValidation_p.h"

namespace hello::kit {

    std::unique_ptr<VoiceBankSession> VoiceBankSession::create(VoiceBankDiskState::Opened opened,
                                                               DiagnosticList &diagnostics,
                                                               QObject *parent) {
        const auto root = opened.bank.indexOf({});
        if (root < 0 || !isEditable(opened.bank.directories().at(root))) {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Error;
            diagnostic.message =
                root < 0 || opened.bank.directories().at(root).leftOut
                    ? tr("The voice bank cannot be edited, because no encoding was specified for "
                         "its folder.")
                    : tr("The voice bank cannot be edited, because part of the text in its folder "
                         "is not valid in its encoding. Open it in another encoding.");
            diagnostics.push_back(diagnostic);
            return nullptr;
        }
        return std::unique_ptr<VoiceBankSession>(new VoiceBankSession(std::move(opened), parent));
    }

    VoiceBankSession::VoiceBankSession(VoiceBankDiskState::Opened opened, QObject *parent)
        : edit::EditSession(parent), m_disk(std::move(opened.disk)) {
        for (const auto &directory : opened.bank.directories()) {
            if (!isEditable(directory)) {
                m_excluded.push_back(directory);
            }
        }
        edit::EditSessionPrivate::setRoot(*this, treeOf(opened.bank));
        registerVoiceBankValidators(*this);
    }

    VoiceBankSession::~VoiceBankSession() = default;

    const std::filesystem::path &VoiceBankSession::rootPath() const {
        return m_disk.root();
    }

    VoiceBank VoiceBankSession::snapshot() const {
        return voiceBankOf(edit::EditSessionPrivate::find(this, root()), m_disk);
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

    bool VoiceBankSession::isModified() const {
        const auto bank = snapshot();
        for (const auto &directory : bank.directories()) {
            if (m_disk.isModified(bank, directory.path)) {
                return true;
            }
        }
        return false;
    }

    bool VoiceBankSession::save(DiagnosticList &diagnostics) {
        return m_disk.save(snapshot(), diagnostics);
    }

    void VoiceBankSession::rememberCharset(const std::filesystem::path &directory) {
        m_disk.rememberCharset(directory);
    }

    VoiceBankChanges VoiceBankSession::checkDisk(const QList<std::filesystem::path> &places) {
        return withUntaken(m_disk.checkDisk(places));
    }

    VoiceBankChanges VoiceBankSession::checkDisk() {
        return withUntaken(m_disk.checkDisk());
    }

    VoiceBankChanges VoiceBankSession::withUntaken(VoiceBankChanges changes) const {
        if (changes.rootNotFound) {
            return changes;
        }
        const auto &directories = directoriesOf(*this);
        const auto excluded = excludedDirectories();
        for (const auto &path : m_disk.directories()) {
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
        const auto done = m_disk.reloadFromDisk(bank, changes, selector, diagnostics);
        takeFrom(bank, done.changed, tr("Reload from Disk"), diagnostics);
        return done;
    }

    VoiceBankChanges VoiceBankSession::reloadAllFromDisk(VoiceBankCharsetSelector *selector,
                                                         DiagnosticList &diagnostics) {
        auto bank = fullBank();
        const auto done = m_disk.reloadAllFromDisk(bank, selector, diagnostics);
        takeFrom(bank, done.changed, tr("Reload from Disk"), diagnostics);
        return done;
    }

    bool VoiceBankSession::reread(const std::filesystem::path &directory, const QString &charset,
                                  DiagnosticList &diagnostics) {
        auto bank = fullBank();
        if (!m_disk.reread(bank, directory, charset, diagnostics)) {
            return false;
        }
        return takeFrom(bank, {directory}, tr("Read Again in %1").arg(charset), diagnostics);
    }

    bool VoiceBankSession::takeFrom(const VoiceBank &bank,
                                    const QList<std::filesystem::path> &replaced,
                                    const QString &message, DiagnosticList &diagnostics) {
        auto transaction = this->transaction(message);
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
                diagnostic.message = tr("The folder of the voice bank no longer reads in its "
                                        "encoding, so the contents read before are kept.");
                diagnostics.push_back(diagnostic);
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
            } else if (replaced.contains(directory.path)) {
                directories->remove(at, 1);
                std::vector<std::unique_ptr<ss::Node>> nodes;
                nodes.push_back(directoryTreeOf(bank, i));
                directories->insert(at, std::move(nodes));
            }
            if (directory.path.empty() && replaced.contains(directory.path)) {
                setRootFiles(*root, directory);
            }
        }

        m_excluded.clear();
        for (const auto &directory : bank.directories()) {
            if (!isEditable(directory) && !directory.path.empty()) {
                m_excluded.push_back(directory);
            }
        }
        return transaction.commit(diagnostics);
    }

}
