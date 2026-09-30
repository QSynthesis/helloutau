#include "VoiceBankDocument.h"

#include <stdcorelib/pimpl.h>

#include <hellokit/VoiceBank/VoiceBankFileSystemState.h>

#include "RecordingSelector_p.h"

namespace hello::kit {

    class VoiceBankDocument::Impl {
    public:
        using Decl = VoiceBankDocument;

        Impl(Decl *decl, std::unique_ptr<VoiceBankSession> session)
            : _decl(decl), session(std::move(session)) {
        }

        Decl *_decl;
        std::unique_ptr<VoiceBankSession> session;
        // The step at which the files were last saved, or -1 if at none
        int savedStep = 0;
        bool modified = false;

        void updateModified() {
            stdc_decl_t;
            if (session->isIncomplete()) {
                savedStep = -1;
            }
            const bool now = session->currentStep() != savedStep;
            if (now != modified) {
                modified = now;
                Q_EMIT decl.modifiedChanged(now);
            }
        }

        void saved() {
            savedStep = session->currentStep();
            updateModified();
        }

        VoiceBankChanges checked(VoiceBankChanges changes) {
            if (!changes.changed.isEmpty() || !changes.removed.isEmpty() ||
                !changes.config.isEmpty() || changes.rootNotFound) {
                savedStep = -1;
            }
            updateModified();
            return changes;
        }
    };

    VoiceBankDocument::VoiceBankDocument(std::unique_ptr<VoiceBankSession> session, QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this, std::move(session))) {
        stdc_impl_t;
        connect(impl.session.get(), &VoiceBankSession::stepChanged, this, [this] {
            stdc_impl_t;
            impl.updateModified();
        });
        connect(impl.session.get(), &VoiceBankSession::stepsDiscarded, this,
                [this](int first, int last) {
                    stdc_impl_t;
                    if (impl.savedStep >= first && impl.savedStep <= last) {
                        impl.savedStep = -1;
                    }
                });
        impl.savedStep = impl.session->hasUnrecordedCharsets() ? -1 : impl.session->currentStep();
        impl.updateModified();
    }

    VoiceBankDocument::~VoiceBankDocument() = default;

    std::unique_ptr<VoiceBankDocument> VoiceBankDocument::open(const std::filesystem::path &root,
                                                               VoiceBankCharsetSelector *selector,
                                                               DiagnosticList &diagnostics,
                                                               QObject *parent) {
        RecordingSelector recording(selector);
        auto opened =
            VoiceBankFileSystemState::open(root, selector ? &recording : nullptr, diagnostics);
        if (!opened) {
            return nullptr;
        }
        auto session = VoiceBankSession::create(std::move(*opened), diagnostics);
        if (!session) {
            return nullptr;
        }
        for (const auto &directory : std::as_const(recording.chosen)) {
            session->rememberCharset(directory);
        }
        return std::unique_ptr<VoiceBankDocument>(
            new VoiceBankDocument(std::move(session), parent));
    }

    VoiceBankSession *VoiceBankDocument::session() {
        stdc_impl_t;
        return impl.session.get();
    }

    const VoiceBankSession *VoiceBankDocument::session() const {
        stdc_impl_t;
        return impl.session.get();
    }

    std::filesystem::path VoiceBankDocument::rootPath() const {
        stdc_impl_t;
        return impl.session->rootPath();
    }

    QString VoiceBankDocument::displayName() const {
        stdc_impl_t;
        auto root = impl.session->rootPath();
        if (!root.has_filename()) {
            root = root.parent_path();
        }
        return QString::fromStdU16String(root.filename().u16string());
    }

    bool VoiceBankDocument::isModified() const {
        stdc_impl_t;
        return impl.modified;
    }

    bool VoiceBankDocument::save(DiagnosticList &diagnostics) {
        stdc_impl_t;
        if (!impl.session->save(diagnostics)) {
            return false;
        }
        impl.saved();
        Q_EMIT saved();
        return true;
    }

    bool VoiceBankDocument::saveAs(const std::filesystem::path &folder,
                                   VoiceBankSession::SaveAsFiles files,
                                   DiagnosticList &diagnostics) {
        stdc_impl_t;
        if (!impl.session->saveAs(folder, files, diagnostics)) {
            return false;
        }
        impl.saved();
        Q_EMIT rootPathChanged();
        Q_EMIT saved();
        return true;
    }

    VoiceBankChanges VoiceBankDocument::checkDisk(const QList<std::filesystem::path> &places) {
        stdc_impl_t;
        return impl.checked(impl.session->checkDisk(places));
    }

    VoiceBankChanges VoiceBankDocument::checkDisk() {
        stdc_impl_t;
        return impl.checked(impl.session->checkDisk());
    }

    VoiceBankChanges VoiceBankDocument::reloadFromDisk(const VoiceBankChanges &changes,
                                                       VoiceBankCharsetSelector *selector,
                                                       DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto applied = impl.session->reloadFromDisk(changes, selector, diagnostics);
        impl.updateModified();
        return applied;
    }

    VoiceBankChanges VoiceBankDocument::reloadAllFromDisk(VoiceBankCharsetSelector *selector,
                                                          DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto applied = impl.session->reloadAllFromDisk(selector, diagnostics);
        impl.updateModified();
        return applied;
    }

}
