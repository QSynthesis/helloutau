#include "ClassicPluginPreviewServer.h"

#include <utility>

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

#include <stdutau/pluginfile.h>

#include "ClassicPlugin.h"

namespace hello::daw {

    namespace {

        // The longest request line accepted. A request holds three paths. The limit only bounds
        // the memory that a stray client can occupy.
        constexpr qsizetype maximumLine = 1 << 20;

        QByteArray replyLine(ClassicPluginPreviewServer::Result result, const QString &message) {
            QJsonObject object;
            switch (result) {
                case ClassicPluginPreviewServer::Done:
                    object.insert(QLatin1String("result"), QLatin1String("done"));
                    break;
                case ClassicPluginPreviewServer::Canceled:
                    object.insert(QLatin1String("result"), QLatin1String("canceled"));
                    break;
                case ClassicPluginPreviewServer::Failed:
                    object.insert(QLatin1String("result"), QLatin1String("failed"));
                    object.insert(QLatin1String("message"), message);
                    break;
            }
            return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
        }

        // Writes the reply and closes the connection once the reply is written.
        void send(QLocalSocket *socket, ClassicPluginPreviewServer::Result result,
                  const QString &message = {}) {
            socket->write(replyLine(result, message));
            socket->disconnectFromServer();
        }

        std::filesystem::path pathOf(const QString &text) {
            return std::filesystem::path(text.toStdU16String());
        }

    }

    class ClassicPluginPreviewServer::Impl {
    public:
        Impl(ClassicPluginPreviewServer *q, const ClassicPlugin &plugin,
             const kit::Project &project, int first, int count)
            : q(q), plugin(plugin), project(project), first(first), count(count) {
        }

        ClassicPluginPreviewServer *q;
        ClassicPlugin plugin;
        kit::Project project;
        int first;
        int count;
        QLocalServer server;

        // The connection of the request that awaits reply()
        QLocalSocket *pending = nullptr;

        void accept() {
            while (const auto socket = server.nextPendingConnection()) {
                QObject::connect(socket, &QLocalSocket::readyRead, q,
                                 [this, socket] { read(socket); });
                QObject::connect(socket, &QLocalSocket::disconnected, q,
                                 [this, socket] { closed(socket); });
            }
        }

        // Handles the request line of socket, the only line of a connection.
        void read(QLocalSocket *socket) {
            if (!socket->canReadLine()) {
                if (socket->bytesAvailable() > maximumLine) {
                    socket->abort();
                }
                return;
            }
            const auto line = socket->readLine(maximumLine);
            QObject::disconnect(socket, &QLocalSocket::readyRead, q, nullptr);
            if (pending) {
                send(socket, Failed, tr("Another preview of the plugin is in progress."));
                return;
            }
            kit::DiagnosticList diagnostics;
            auto request = parse(line, diagnostics);
            if (!request) {
                send(socket, Failed, messageOf(diagnostics));
                Q_EMIT q->rejected(diagnostics);
                return;
            }
            pending = socket;
            Q_EMIT q->requested(*request);
        }

        void closed(QLocalSocket *socket) {
            socket->deleteLater();
            if (socket == pending) {
                pending = nullptr;
                Q_EMIT q->abandoned();
            }
        }

        static QString messageOf(const kit::DiagnosticList &diagnostics) {
            QStringList messages;
            for (const auto &diagnostic : diagnostics) {
                messages.push_back(diagnostic.message);
            }
            return messages.join(QLatin1Char('\n'));
        }

        static void fail(kit::DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({kit::DiagnosticSeverity::Error, message, std::nullopt});
        }

        std::optional<Request> parse(const QByteArray &line, kit::DiagnosticList &diagnostics) {
            const auto document = QJsonDocument::fromJson(line);
            const auto object = document.object();
            const auto input = object.value(QLatin1String("input")).toString();
            if (!document.isObject() || input.isEmpty()) {
                fail(diagnostics, tr("The request of the preview cannot be read."));
                return std::nullopt;
            }

            // The input file is the temporary file of the run, or a copy of it that PitTrace
            // makes, and its notes are those passed to the plugin.
            QFile in(input);
            if (!in.open(QIODevice::ReadOnly)) {
                fail(diagnostics, tr("The file \"%1\" of the preview cannot be read.").arg(input));
                return std::nullopt;
            }
            const auto bytes = in.readAll();
            utau::PluginInput file;
            file.read(std::string_view(bytes.data(), size_t(bytes.size())));
            const int expectedFirst = plugin.wholeTrack ? 0 : first;
            const int expectedCount =
                plugin.wholeTrack ? int(project.tracks.first().notes.size()) : count;
            if (file.startIndex != expectedFirst || int(file.notes.size()) != expectedCount) {
                fail(diagnostics, tr("The notes of the preview are not the notes passed to the "
                                     "plugin."));
                return std::nullopt;
            }

            // A file that cannot be read is no modification, as in the original playinplugin.
            QByteArray modification;
            if (const auto modify = object.value(QLatin1String("modify")); modify.isString()) {
                QFile result(modify.toString());
                if (result.open(QIODevice::ReadOnly)) {
                    modification = result.readAll();
                }
            }
            // The warnings of the modifications, such as ignored sections, concern the
            // application to the document and are not shown for a preview.
            kit::DiagnosticList modifications;
            auto preview = ClassicPluginExchange::preview(plugin, project, first, count,
                                                          modification, modifications);
            if (!preview) {
                diagnostics += modifications;
                return std::nullopt;
            }

            Request request;
            request.preview = std::move(*preview);
            if (const auto output = object.value(QLatin1String("output")).toString();
                !output.isEmpty()) {
                request.output = pathOf(output);
            }
            request.play = object.value(QLatin1String("play")).toBool(true);
            request.keepOutput = object.value(QLatin1String("keepOutput")).toBool(false);
            return request;
        }
    };

    ClassicPluginPreviewServer::ClassicPluginPreviewServer(const ClassicPlugin &plugin,
                                                           const kit::Project &project, int first,
                                                           int count, QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this, plugin, project, first, count)) {
        QObject::connect(&_impl->server, &QLocalServer::newConnection, this,
                         [this] { _impl->accept(); });
    }

    ClassicPluginPreviewServer::~ClassicPluginPreviewServer() {
        _impl->server.close();
    }

    QString ClassicPluginPreviewServer::pipeName(qint64 processId) {
        return QStringLiteral("helloutau-plugin-play-%1").arg(processId);
    }

    bool ClassicPluginPreviewServer::listen(qint64 processId, QString *error) {
        _impl->server.setSocketOptions(QLocalServer::UserAccessOption);
        if (!_impl->server.listen(pipeName(processId))) {
            *error = _impl->server.errorString();
            return false;
        }
        return true;
    }

    bool ClassicPluginPreviewServer::isBusy() const {
        return _impl->pending != nullptr;
    }

    void ClassicPluginPreviewServer::reply(Result result, const QString &message) {
        if (!_impl->pending) {
            return;
        }
        // The request is cleared first, so that the closing of its connection does not count as
        // an abandonment.
        send(std::exchange(_impl->pending, nullptr), result, message);
    }

}
