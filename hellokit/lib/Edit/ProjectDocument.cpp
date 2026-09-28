#include "ProjectDocument.h"

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

}
