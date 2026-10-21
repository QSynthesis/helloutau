#include "ClassicPluginPreview.h"

#include <optional>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QPointer>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#ifdef Q_OS_WINDOWS
#  include <QtCore/qt_windows.h>
#endif

#include <hellokit/Document/Project.h>
#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Support/TemporaryStorage.h>

#include <helloutau/Editor/AppSettings.h>
#include <helloutau/Editor/Dialogs/DiagnosticBox.h>
#include <helloutau/Editor/Editor.h>
#include <helloutau/Editor/Playback.h>
#include <helloutau/Editor/ProjectWindow.h>
#include <helloutau/Editor/SynthToolTrust.h>

#include <ClassicPluginHost/ClassicPlugin.h>
#include <ClassicPluginHost/ClassicPluginPreviewServer.h>

namespace hello::daw {

    namespace {

        // Brings window to the front. On Windows the host may set the foreground window while
        // the plugin is the foreground process, because hello-plugin-play passes it the right.
        void bringToFront(QWidget *window) {
            window->raise();
            window->activateWindow();
#ifdef Q_OS_WINDOWS
            SetForegroundWindow(reinterpret_cast<HWND>(window->winId()));
#endif
        }

        // The translation context of the texts of a preview
        class ClassicPluginPreviewText {
            Q_DECLARE_TR_FUNCTIONS(hello::daw::ClassicPluginPreview)
        };
        using Server = ClassicPluginPreviewServer;

        QString textOf(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        QString messagesOf(const kit::DiagnosticList &diagnostics) {
            QStringList messages;
            for (const auto &diagnostic : diagnostics) {
                messages.push_back(diagnostic.message);
            }
            return messages.join(QLatin1Char('\n'));
        }

        // Removes the file of /o: after the reply unless /nodeletewav keeps it, as the original
        // playinplugin does.
        void removeOutput(const Server::Request &request) {
            if (request.output && !request.keepOutput) {
                QFile::remove(textOf(*request.output));
            }
        }

    }

    class ClassicPluginPreview::Impl {
    public:
        Impl(ProjectWindow *window, const ClassicPlugin &plugin, const kit::Project &project,
             int first, int count)
            : window(window), plugin(plugin), server(plugin, project, first, count) {
        }

        ProjectWindow *window;
        ClassicPlugin plugin;
        Server server;
        std::optional<QTemporaryDir> cache;

        // Whether a preview has used the playback of the window
        bool used = false;

        // The receiver of the connections of the request in progress, which are removed with it
        std::unique_ptr<QObject> requestContext;

        // The dialog of the request in progress, closed if the request is abandoned
        QPointer<QDialog> dialog;
        bool abandoned = false;

#ifdef Q_OS_WINDOWS
        // The foreground window when the request arrived, normally the window of the plugin,
        // which becomes the foreground window again after the dialogs of the request close
        HWND previousForeground = nullptr;
#endif

        void rememberForeground() {
#ifdef Q_OS_WINDOWS
            previousForeground = GetForegroundWindow();
#endif
        }

        void restoreForeground() {
#ifdef Q_OS_WINDOWS
            if (previousForeground && IsWindow(previousForeground)) {
                SetForegroundWindow(previousForeground);
            }
            previousForeground = nullptr;
#endif
        }

        std::filesystem::path cacheDirectory() const {
            return std::filesystem::path(cache->path().toStdU16String());
        }

        bool start(qint64 processId) {
            const auto directory = window->playback()->temporaryDirectory();
            cache.emplace(
                directory.empty()
                    ? kit::TemporaryStorage::templatePath(QStringLiteral("plugin-preview"))
                    : QDir(QString::fromStdU16String(directory.u16string()))
                          .filePath(QStringLiteral("plugin-preview-XXXXXX")));
            if (!cache->isValid()) {
                return false;
            }
            QString error;
            if (!server.listen(processId, &error)) {
                return false;
            }
            QObject::connect(&server, &Server::requested, &server,
                             [this](const Server::Request &request) { handle(request); });
            QObject::connect(&server, &Server::abandoned, &server, [this] { abandon(); });
            return true;
        }

        // Returns the synth tools of the project, those with which the window plays, if both
        // exist and may run, or std::nullopt with the reason in message.
        std::optional<kit::SynthTools> synthTools(const kit::Project &project,
                                                  QString *message) const {
            const auto &settings = window->editor()->settings();
            const auto utau = settings.utau().directory();
            const QStringList values = {project.settings.wavtool, project.settings.resampler};
            for (const auto &value : values) {
                if (!SynthToolTrust::exists(value, utau)) {
                    *message = ClassicPluginPreviewText::tr(
                        "Set a valid wavtool and resampler in the project "
                        "properties first.");
                    return std::nullopt;
                }
            }
            for (const auto &value : values) {
                if (!SynthToolTrust::isAllowed(settings, value, utau)) {
                    *message = ClassicPluginPreviewText::tr(
                        "The synth tools of the project are not trusted. Play "
                        "the project once and trust them, then preview again.");
                    return std::nullopt;
                }
            }
            kit::SynthTools tools;
            tools.wavtool = SynthToolTrust::resolved(project.settings.wavtool, utau);
            tools.resampler = SynthToolTrust::resolved(project.settings.resampler, utau);
            // The synth tools that run because the user trusted them stay unchanged while they
            // run, as in the project window. Both roles may name one program.
            for (const auto &value : values) {
                const auto file = SynthToolTrust::trustedFile(settings, value, utau);
                if (file && std::none_of(tools.trusted.cbegin(), tools.trusted.cend(),
                                         [&](const kit::FileGuard::File &other) {
                                             return other.path == file->path;
                                         })) {
                    tools.trusted.push_back(*file);
                }
            }
            return tools;
        }

        void handle(const Server::Request &request) {
            const auto modal = QApplication::activeModalWidget();
            const auto parent = modal ? modal : static_cast<QWidget *>(window);
            rememberForeground();
            QString message;
            const auto tools = synthTools(request.preview.project, &message);
            if (!tools) {
                QMessageBox box(QMessageBox::Critical, plugin.name, message, QMessageBox::Ok,
                                parent);
                box.show();
                bringToFront(&box);
                box.exec();
                restoreForeground();
                server.reply(Server::Failed, message);
                return;
            }
            const auto &preview = request.preview;
            if (preview.count <= 0) {
                server.reply(Server::Done);
                return;
            }
            const auto range = std::make_pair(preview.first, preview.first + preview.count - 1);
            used = true;
            abandoned = false;
            requestContext = std::make_unique<QObject>();
            if (request.play) {
                play(request, *tools, range, parent);
            } else {
                render(request, *tools, range, parent);
            }
        }

        // Renders the notes without playing them, for /noplay, and replies once the render
        // ends.
        void render(const Server::Request &request, const kit::SynthTools &tools,
                    std::pair<int, int> range, QWidget *parent) {
            const auto playback = window->playback();
            const auto file = request.output ? *request.output : cacheDirectory() / "preview.wav";
            const auto context = requestContext.get();
            QObject::connect(playback, &Playback::trackRendered, context,
                             [this, request] { finish(request, Server::Done); });
            QObject::connect(playback, &Playback::failed, context,
                             [this, request] { finish(request, Server::Failed); });
            // A render stopped from the status bar ends without trackRendered() or failed(). The
            // check comes after those signals, which may arrive after the change of the state.
            QObject::connect(playback, &Playback::stateChanged, context,
                             [this, request, context](Playback::State state) {
                                 if (state == Playback::Stopped) {
                                     QTimer::singleShot(0, context, [this, request] {
                                         finish(request, Server::Canceled);
                                     });
                                 }
                             });
            kit::DiagnosticList diagnostics;
            if (!playback->renderTrack(request.preview.project, *window->document(),
                                       cacheDirectory(), range, file, tools, diagnostics)) {
                requestContext.reset();
                DiagnosticBox::report(parent, plugin.name, diagnostics);
                server.reply(Server::Failed, messagesOf(diagnostics));
            }
        }

        // Ends a render of render(). Called from a connection of the request, whose receiver is
        // therefore deleted later.
        void finish(const Server::Request &request, Server::Result result) {
            if (!requestContext) {
                return;
            }
            requestContext.release()->deleteLater();
            server.reply(result);
            removeOutput(request);
        }

        // Shows the preview dialog, which renders the notes and plays them, until the user
        // closes it, and replies then.
        void play(const Server::Request &request, const kit::SynthTools &tools,
                  std::pair<int, int> range, QWidget *parent) {
            const auto playback = window->playback();
            const auto document = window->document();
            const auto context = requestContext.get();

            QDialog dialog(parent);
            this->dialog = &dialog;
            dialog.setWindowTitle(ClassicPluginPreviewText::tr("Preview of %1").arg(plugin.name));
            const auto layout = new QVBoxLayout(&dialog);
            const auto status = new QLabel();
            const auto progress = new QProgressBar();
            progress->setTextVisible(false);
            const auto buttons = new QDialogButtonBox(QDialogButtonBox::Close);
            const auto again = buttons->addButton(ClassicPluginPreviewText::tr("Play &Again"),
                                                  QDialogButtonBox::ActionRole);
            const auto stop = buttons->addButton(ClassicPluginPreviewText::tr("&Stop"),
                                                 QDialogButtonBox::ActionRole);
            layout->addWidget(status);
            layout->addWidget(progress);
            layout->addWidget(buttons);
            dialog.setMinimumWidth(360);

            bool rendered = false;
            bool failed = false;
            const auto update = [&] {
                const auto state = playback->state();
                const bool rendering = state == Playback::Rendering;
                const bool playing = state == Playback::Playing;
                if (rendering) {
                    status->setText(ClassicPluginPreviewText::tr("Rendering..."));
                } else if (playing) {
                    status->setText(ClassicPluginPreviewText::tr("Playing..."));
                } else if (failed) {
                    status->setText(
                        ClassicPluginPreviewText::tr("The notes could not be rendered."));
                } else {
                    status->setText(rendered
                                        ? ClassicPluginPreviewText::tr("Stopped.")
                                        : ClassicPluginPreviewText::tr("The render was stopped."));
                }
                progress->setVisible(rendering);
                again->setEnabled(!rendering && !playing);
                stop->setEnabled(rendering || playing);
            };
            const auto start = [&] {
                failed = false;
                progress->setRange(0, 0);
                kit::DiagnosticList diagnostics;
                if (!playback->play(request.preview.project, *document, cacheDirectory(), range,
                                    tools, diagnostics)) {
                    failed = true;
                    DiagnosticBox::report(&dialog, plugin.name, diagnostics);
                }
                update();
            };

            QObject::connect(playback, &Playback::progressed, context, [&](int done, int total) {
                progress->setRange(0, total);
                progress->setValue(done);
            });
            QObject::connect(playback, &Playback::stateChanged, context,
                             [&](Playback::State state) {
                                 // The render has ended once playback starts. The track file of
                                 // /o: is its copy.
                                 if (state == Playback::Playing && !rendered) {
                                     rendered = true;
                                     if (request.output) {
                                         const auto output = textOf(*request.output);
                                         QFile::remove(output);
                                         QFile::copy(textOf(playback->lastRenderFile()), output);
                                     }
                                 }
                                 update();
                             });
            QObject::connect(playback, &Playback::failed, context, [&] {
                failed = true;
                update();
            });
            QObject::connect(again, &QPushButton::clicked, context, start);
            QObject::connect(stop, &QPushButton::clicked, playback, &Playback::stop);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

            dialog.show();
            bringToFront(&dialog);
            start();
            dialog.exec();

            requestContext.reset();
            this->dialog = nullptr;
            restoreForeground();
            if (playback->state() != Playback::Stopped) {
                playback->stop();
            }
            if (abandoned) {
                removeOutput(request);
                return;
            }
            if (failed) {
                server.reply(Server::Failed,
                             ClassicPluginPreviewText::tr("The notes could not be rendered."));
            } else {
                server.reply(rendered ? Server::Done : Server::Canceled);
            }
            removeOutput(request);
        }

        // Stops the request in progress, whose hello-plugin-play has ended.
        void abandon() {
            abandoned = true;
            window->playback()->stop();
            if (dialog) {
                dialog->reject();
            } else {
                requestContext.reset();
            }
        }
    };

    ClassicPluginPreview::ClassicPluginPreview(ProjectWindow *window, const ClassicPlugin &plugin,
                                               const kit::Project &project, int first, int count)
        : _impl(std::make_unique<Impl>(window, plugin, project, first, count)) {
    }

    ClassicPluginPreview::~ClassicPluginPreview() {
        // The render must not write into the cache, which is removed with the object.
        if (_impl->used) {
            _impl->window->playback()->stopAndWait();
        }
    }

    bool ClassicPluginPreview::start(qint64 processId) {
        return _impl->start(processId);
    }

}
