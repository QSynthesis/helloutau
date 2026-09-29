#include "ProjectDocument.h"

#include <QtCore/QDir>

#include <stdcorelib/pimpl.h>

#include <hellokit/VoiceBank/VoiceBankFileSystemState.h>

namespace hello::kit {

    namespace {

        Project emptyProject() {
            Project project;
            project.tracks.push_back(Track{});
            return project;
        }

        // Extensions are ASCII, so a case-insensitive comparison of the whole string suffices.
        bool hasExtension(const std::filesystem::path &path, QStringView extension) {
            return QString::fromStdU16String(path.extension().u16string())
                       .compare(extension, Qt::CaseInsensitive) == 0;
        }

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message, std::nullopt});
        }

        // The project with the last Mode2 point of every note at the pitch of the note, as the
        // editor writes points (see step 2 in docs/Tuning.md). The editor of UTAU moves that point
        // only horizontally and keeps any height it was given, which is not meant to be kept.
        // Done before the session exists, so the document is not modified by it.
        Project endingAtPitch(Project project) {
            for (auto &track : project.tracks) {
                for (auto &note : track.notes) {
                    if (!note.portamento.isEmpty()) {
                        note.portamento.last().y = 0;
                    }
                }
            }
            return project;
        }

        void warn(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Warning, message, std::nullopt});
        }

        QString displayed(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        // Passes each question on, and records the directories for which the user chose an
        // encoding.
        class RecordingSelector : public VoiceBankCharsetSelector {
        public:
            explicit RecordingSelector(VoiceBankCharsetSelector *selector) : m_selector(selector) {
            }

            std::optional<QString> selectCharset(const VoiceBankDirectorySource &directory,
                                                 DiagnosticList &diagnostics) override {
                auto charset = m_selector->selectCharset(directory, diagnostics);
                if (charset) {
                    chosen.push_back(directory.path);
                }
                return charset;
            }

            QList<std::filesystem::path> chosen;

        private:
            VoiceBankCharsetSelector *m_selector;
        };
    }

    UstCharsetSelector::~UstCharsetSelector() = default;

    class ProjectDocument::Impl {
    public:
        using Decl = ProjectDocument;

        Impl(Decl *decl, const Project &project, const std::filesystem::path &sourcePath,
             bool native)
            : _decl(decl), session(project), sourcePath(sourcePath),
              filePath(native ? sourcePath : std::filesystem::path()),
              savedStep(session.currentStep()) {
        }

        Decl *_decl;
        ProjectSession session;
        std::filesystem::path sourcePath;
        std::filesystem::path filePath;
        int savedStep;
        bool modified = false;
        std::shared_ptr<const VoiceBank> voiceBank;

        void updateModified() {
            stdc_decl_t;
            const bool now = session.currentStep() != savedStep;
            if (now != modified) {
                modified = now;
                Q_EMIT decl.modifiedChanged(now);
            }
        }

        bool write(const std::filesystem::path &path, DiagnosticList &diagnostics) const {
            return session.snapshot().save(path, diagnostics);
        }
    };

    ProjectDocument::ProjectDocument(QObject *parent)
        : ProjectDocument(emptyProject(), {}, false, parent) {
    }

    ProjectDocument::ProjectDocument(const Project &project,
                                     const std::filesystem::path &sourcePath, bool native,
                                     QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this, project, sourcePath, native)) {
        stdc_impl_t;
        connect(&impl.session, &ProjectSession::stepChanged, this, [this] {
            stdc_impl_t;
            impl.updateModified();
        });
    }

    ProjectDocument::~ProjectDocument() = default;

    std::unique_ptr<ProjectDocument> ProjectDocument::open(const std::filesystem::path &path,
                                                           UstCharsetSelector *selector,
                                                           DiagnosticList &diagnostics,
                                                           QObject *parent) {
        if (hasExtension(path, u".usth")) {
            const auto project = Project::open(path, diagnostics);
            if (!project) {
                return nullptr;
            }
            return std::unique_ptr<ProjectDocument>(
                new ProjectDocument(endingAtPitch(*project), path, true, parent));
        }

        if (hasExtension(path, u".ust")) {
            const auto ust = UstDocument::open(path, diagnostics);
            if (!ust) {
                return nullptr;
            }
            auto charset = ust->settledCharset();
            if (!charset) {
                if (!selector) {
                    fail(diagnostics, tr("This file does not state its encoding."));
                    return nullptr;
                }
                charset = selector->selectCharset(*ust, path);
                if (!charset) {
                    return nullptr;
                }
            }
            const auto project = ust->toProject(*charset, diagnostics);
            if (!project) {
                return nullptr;
            }
            return std::unique_ptr<ProjectDocument>(
                new ProjectDocument(endingAtPitch(*project), path, false, parent));
        }

        fail(diagnostics, tr("This file is neither a HelloUtau project nor a UST."));
        return nullptr;
    }

    ProjectSession *ProjectDocument::session() {
        stdc_impl_t;
        return &impl.session;
    }

    const ProjectSession *ProjectDocument::session() const {
        stdc_impl_t;
        return &impl.session;
    }

    std::filesystem::path ProjectDocument::filePath() const {
        stdc_impl_t;
        return impl.filePath;
    }

    std::filesystem::path ProjectDocument::sourcePath() const {
        stdc_impl_t;
        return impl.sourcePath;
    }

    QString ProjectDocument::displayName() const {
        stdc_impl_t;
        return QString::fromStdU16String(impl.sourcePath.filename().u16string());
    }

    bool ProjectDocument::isModified() const {
        stdc_impl_t;
        return impl.modified;
    }

    bool ProjectDocument::save(DiagnosticList &diagnostics) {
        stdc_impl_t;
        Q_ASSERT(!impl.filePath.empty());
        if (!impl.write(impl.filePath, diagnostics)) {
            return false;
        }
        impl.savedStep = impl.session.currentStep();
        impl.updateModified();
        return true;
    }

    bool ProjectDocument::saveAs(const std::filesystem::path &path, DiagnosticList &diagnostics) {
        stdc_impl_t;
        if (!impl.write(path, diagnostics)) {
            return false;
        }
        impl.savedStep = impl.session.currentStep();
        impl.updateModified();
        impl.filePath = path;
        impl.sourcePath = path;
        Q_EMIT filePathChanged();
        return true;
    }

    bool ProjectDocument::exportUst(const std::filesystem::path &path,
                                    UstDocument::ExportOptions options,
                                    DiagnosticList &diagnostics) const {
        stdc_impl_t;
        options.file = path;
        const auto ust = UstDocument::fromProject(impl.session.snapshot(), options, diagnostics);
        return ust && ust->save(path, diagnostics);
    }

    std::shared_ptr<const VoiceBank> ProjectDocument::voiceBank() const {
        stdc_impl_t;
        return impl.voiceBank;
    }

    bool ProjectDocument::loadVoiceBank(const std::filesystem::path &utauDirectory,
                                        VoiceBankCharsetSelector *selector,
                                        DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto set = [this](std::shared_ptr<const VoiceBank> bank) {
            stdc_impl_t;
            if (bank != impl.voiceBank) {
                impl.voiceBank = std::move(bank);
                Q_EMIT voiceBankChanged();
            }
        };

        const auto track = impl.session.snapshot().tracks.value(0);
        if (track.voiceDir.isEmpty()) {
            set(nullptr);
            return false;
        }
        const auto root = track.voiceDirectory(utauDirectory);
        if (root.empty()) {
            warn(diagnostics, tr("The voice bank \"%1\" is in the UTAU folder, which is not set.")
                                  .arg(track.voiceDir));
            set(nullptr);
            return false;
        }

        RecordingSelector recording(selector);
        auto opened =
            VoiceBankFileSystemState::open(root, selector ? &recording : nullptr, diagnostics);
        if (!opened) {
            set(nullptr);
            return false;
        }

        // Saving writes only the configuration of each remembered directory, since nothing else
        // changed since it was read.
        if (!recording.chosen.isEmpty()) {
            for (const auto &directory : std::as_const(recording.chosen)) {
                opened->files.rememberCharset(directory);
            }
            DiagnosticList saving;
            if (!opened->files.save(opened->bank, saving)) {
                warn(diagnostics, tr("The chosen encoding could not be recorded in \"%1\", so it "
                                     "will be asked again next time.")
                                      .arg(displayed(root)));
                for (auto &diagnostic : saving) {
                    if (diagnostic.severity == DiagnosticSeverity::Error) {
                        diagnostic.severity = DiagnosticSeverity::Warning;
                    }
                    diagnostics.push_back(std::move(diagnostic));
                }
            }
        }

        set(std::make_shared<const VoiceBank>(std::move(opened->bank)));
        return true;
    }
}
