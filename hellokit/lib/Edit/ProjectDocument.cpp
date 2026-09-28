#include "ProjectDocument.h"

#include <QtCore/QDir>

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
        Impl(ProjectDocument *decl, const Project &project, const std::filesystem::path &sourcePath,
             bool native)
            : _decl(decl), session(project), sourcePath(sourcePath),
              filePath(native ? sourcePath : std::filesystem::path()),
              savedStep(session.currentStep()) {
        }

        ProjectDocument *_decl;
        ProjectSession session;
        std::filesystem::path sourcePath;
        std::filesystem::path filePath;
        int savedStep;
        bool modified = false;
        std::shared_ptr<const VoiceBank> voiceBank;

        void updateModified() {
            const bool now = session.currentStep() != savedStep;
            if (now != modified) {
                modified = now;
                Q_EMIT _decl->modifiedChanged(now);
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
        connect(&_impl->session, &ProjectSession::stepChanged, this,
                [this] { _impl->updateModified(); });
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
                new ProjectDocument(*project, path, true, parent));
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
                new ProjectDocument(*project, path, false, parent));
        }

        fail(diagnostics, tr("This file is neither a HelloUtau project nor a UST."));
        return nullptr;
    }

    ProjectSession *ProjectDocument::session() const {
        return &_impl->session;
    }

    std::filesystem::path ProjectDocument::filePath() const {
        return _impl->filePath;
    }

    std::filesystem::path ProjectDocument::sourcePath() const {
        return _impl->sourcePath;
    }

    QString ProjectDocument::displayName() const {
        return QString::fromStdU16String(_impl->sourcePath.filename().u16string());
    }

    bool ProjectDocument::isModified() const {
        return _impl->modified;
    }

    bool ProjectDocument::save(DiagnosticList &diagnostics) {
        Q_ASSERT(!_impl->filePath.empty());
        if (!_impl->write(_impl->filePath, diagnostics)) {
            return false;
        }
        _impl->savedStep = _impl->session.currentStep();
        _impl->updateModified();
        return true;
    }

    bool ProjectDocument::saveAs(const std::filesystem::path &path, DiagnosticList &diagnostics) {
        if (!_impl->write(path, diagnostics)) {
            return false;
        }
        _impl->savedStep = _impl->session.currentStep();
        _impl->updateModified();
        _impl->filePath = path;
        _impl->sourcePath = path;
        Q_EMIT filePathChanged();
        return true;
    }

    bool ProjectDocument::exportUst(const std::filesystem::path &path,
                                    UstDocument::ExportOptions options,
                                    DiagnosticList &diagnostics) const {
        options.file = path;
        const auto ust = UstDocument::fromProject(_impl->session.snapshot(), options, diagnostics);
        return ust && ust->save(path, diagnostics);
    }

    std::shared_ptr<const VoiceBank> ProjectDocument::voiceBank() const {
        return _impl->voiceBank;
    }

    bool ProjectDocument::loadVoiceBank(const std::filesystem::path &utauDirectory,
                                        VoiceBankCharsetSelector *selector,
                                        DiagnosticList &diagnostics) {
        const auto set = [this](std::shared_ptr<const VoiceBank> bank) {
            if (bank != _impl->voiceBank) {
                _impl->voiceBank = std::move(bank);
                Q_EMIT voiceBankChanged();
            }
        };

        const auto track = _impl->session.snapshot().tracks.value(0);
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
