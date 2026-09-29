#ifndef HELLOKIT_EDIT_VOICEBANKDOCUMENT_H
#define HELLOKIT_EDIT_VOICEBANKDOCUMENT_H

#include <filesystem>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>

#include <hellokit/Support/Diagnostic.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/VoiceBankSession.h>

namespace hello::kit {

    class VoiceBankCharsetSelector;

    /// A voice bank opened for editing: its session, and whether it differs from its files as
    /// last saved, by the rules of the section on the saved state in docs/Editing.md.
    ///
    /// The saved state is a step of the session: the document is modified at any other step,
    /// or at every step once the saved one is lost. It is lost when a commit discards it, when
    /// the encodings chosen on opening are still to be recorded, and when the disk is found
    /// changed outside the tree.
    class HELLOKIT_EDIT_EXPORT VoiceBankDocument : public QObject {
        Q_OBJECT
    public:
        /// Opens the voice bank in \a root, querying \a selector for each directory whose
        /// encoding is not recorded. The chosen encodings are remembered, to be recorded by the
        /// next save, and the document is modified until then.
        ///
        /// \return the document, or null if the voice bank could not be read, with the reason in
        ///         \a diagnostics
        static std::unique_ptr<VoiceBankDocument> open(const std::filesystem::path &root,
                                                       VoiceBankCharsetSelector *selector,
                                                       DiagnosticList &diagnostics,
                                                       QObject *parent = nullptr);

        ~VoiceBankDocument();

        VoiceBankSession *session();
        const VoiceBankSession *session() const;

        /// The folder of the voice bank, which saveAs() changes.
        std::filesystem::path rootPath() const;

        /// The name of the folder, for the title of a window.
        QString displayName() const;

        bool isModified() const;

        /// Saves the voice bank into its folder.
        bool save(DiagnosticList &diagnostics);

        /// Saves the voice bank into \a folder, which must not exist or be empty, and edits it
        /// there from now on.
        bool saveAs(const std::filesystem::path &folder, VoiceBankSession::SaveAsFiles files,
                    DiagnosticList &diagnostics);

        /// Checks the disk for changes, as VoiceBankSession::checkDisk() does. A change that the
        /// tree does not hold, of a text file, a directory, a configuration or the root, makes
        /// the document modified, since saving is needed to bring the disk back to the tree
        /// even after it is read again.
        VoiceBankChanges checkDisk(const QList<std::filesystem::path> &places);

        /// \overload for the entire voice bank.
        VoiceBankChanges checkDisk();

        /// Applies \a changes as VoiceBankSession::reloadFromDisk() does. A root that no longer
        /// reads makes the document modified, since the tree is then the only intact copy.
        VoiceBankChanges reloadFromDisk(const VoiceBankChanges &changes,
                                        VoiceBankCharsetSelector *selector,
                                        DiagnosticList &diagnostics);

        /// Reads every directory again, as VoiceBankSession::reloadAllFromDisk() does.
        VoiceBankChanges reloadAllFromDisk(VoiceBankCharsetSelector *selector,
                                           DiagnosticList &diagnostics);

    Q_SIGNALS:
        void modifiedChanged(bool modified);

        /// rootPath() changed, by saveAs().
        void rootPathChanged();

    private:
        explicit VoiceBankDocument(std::unique_ptr<VoiceBankSession> session, QObject *parent);

        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOKIT_EDIT_VOICEBANKDOCUMENT_H
