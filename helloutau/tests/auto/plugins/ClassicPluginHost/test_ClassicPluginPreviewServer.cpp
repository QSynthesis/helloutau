#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTemporaryDir>
#include <QtNetwork/QLocalSocket>
#include <QtTest/QTest>

#include <hellokit/Document/Project.h>

#include <ClassicPluginHost/ClassicPlugin.h>
#include <ClassicPluginHost/ClassicPluginExchange.h>
#include <ClassicPluginHost/ClassicPluginPreviewServer.h>

using namespace hello;
using namespace hello::daw;

namespace fs = std::filesystem;

namespace {

    // The selection of the run in every test: the three notes from index 1
    constexpr int first = 1;
    constexpr int count = 3;

    kit::Note noteOf(const QString &lyric) {
        kit::Note note;
        note.lyric = lyric;
        note.length = 480;
        note.noteNum = 60;
        return note;
    }

    kit::Project project() {
        kit::Track track;
        for (const auto lyric : {"R", "a", "i", "u", "e"}) {
            track.notes.push_back(noteOf(QString::fromLatin1(lyric)));
        }
        kit::Project project;
        project.settings.mode2 = true;
        project.tracks = {track};
        return project;
    }

    ClassicPlugin plugin() {
        ClassicPlugin plugin;
        plugin.name = QStringLiteral("Probe");
        plugin.charset = QStringLiteral("UTF-8");
        return plugin;
    }

    QStringList lyricsOf(const kit::Project &project) {
        QStringList lyrics;
        for (const auto &note : project.tracks.first().notes) {
            lyrics.push_back(note.lyric);
        }
        return lyrics;
    }

    QString write(const QTemporaryDir &dir, const QString &name, const QByteArray &bytes) {
        const auto path = dir.filePath(name);
        QFile file(path);
        file.open(QIODevice::WriteOnly);
        file.write(bytes);
        return path;
    }

    // Writes the temporary file of the notes from index \a from , as PitTrace copies it for
    // playinplugin.
    QString inputOf(const QTemporaryDir &dir, int from, int notes) {
        return write(dir, QStringLiteral("Base.ust"),
                     ClassicPluginExchange::input(plugin(), project(), from, notes, {}, nullptr));
    }

    // Sends a request on the pipe of this process, as hello-plugin-play does.
    std::unique_ptr<QLocalSocket> send(const QByteArray &line) {
        auto socket = std::make_unique<QLocalSocket>();
        socket->connectToServer(
            ClassicPluginPreviewServer::pipeName(QCoreApplication::applicationPid()));
        if (!socket->waitForConnected(5000)) {
            return nullptr;
        }
        socket->write(line);
        socket->flush();
        return socket;
    }

    std::unique_ptr<QLocalSocket> send(const QJsonObject &request) {
        return send(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    }

    // Returns the reply that socket receives.
    QJsonObject replyOf(QLocalSocket &socket) {
        if (!QTest::qWaitFor([&] { return socket.canReadLine(); }, 5000)) {
            return {};
        }
        return QJsonDocument::fromJson(socket.readLine()).object();
    }

    // A server of the run, with the last request and rejection that it reported
    struct Server {
        Server() : server(plugin(), project(), first, count) {
            QObject::connect(
                &server, &ClassicPluginPreviewServer::requested, &server,
                [this](const ClassicPluginPreviewServer::Request &value) { request = value; });
            QObject::connect(
                &server, &ClassicPluginPreviewServer::rejected, &server,
                [this](const kit::DiagnosticList &diagnostics) { rejection = diagnostics; });
            QObject::connect(&server, &ClassicPluginPreviewServer::abandoned, &server,
                             [this] { abandoned = true; });
            QString error;
            listening = server.listen(QCoreApplication::applicationPid(), &error);
        }

        ClassicPluginPreviewServer server;
        bool listening = false;
        std::optional<ClassicPluginPreviewServer::Request> request;
        std::optional<kit::DiagnosticList> rejection;
        bool abandoned = false;
    };

}

class test_ClassicPluginPreviewServer : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void the_pipe_is_named_after_the_plugin_process() {
        QCOMPARE(ClassicPluginPreviewServer::pipeName(42),
                 QStringLiteral("helloutau-plugin-play-42"));
    }

    // A request whose input file holds the notes passed to the plugin is previewed with the
    // modifications applied to a copy of the project, and the reply reaches hello-plugin-play.
    void a_matching_request_is_previewed() {
        QTemporaryDir dir;
        Server server;
        QVERIFY(server.listening);
        QJsonObject request;
        request.insert(QLatin1String("input"), inputOf(dir, first, count));
        request.insert(QLatin1String("modify"),
                       write(dir, QStringLiteral("testplay.txt"),
                             "[#SETTING]\r\nMode2=True\r\n[#0001]\r\nLyric=X\r\n[#0002]\r\n"
                             "[#0003]\r\n"));
        request.insert(QLatin1String("output"), dir.filePath(QStringLiteral("out.wav")));
        request.insert(QLatin1String("play"), false);
        request.insert(QLatin1String("keepOutput"), true);
        const auto socket = send(request);
        QVERIFY(socket);

        QTRY_VERIFY_WITH_TIMEOUT(server.request.has_value(), 5000);
        const auto &received = *server.request;
        QCOMPARE(lyricsOf(received.preview.project), QStringList({"R", "X", "i", "u", "e"}));
        QCOMPARE(received.preview.first, first);
        QCOMPARE(received.preview.count, count);
        QVERIFY(received.output);
        QCOMPARE(*received.output,
                 fs::path(dir.filePath(QStringLiteral("out.wav")).toStdU16String()));
        QVERIFY(!received.play);
        QVERIFY(received.keepOutput);
        QVERIFY(server.server.isBusy());

        server.server.reply(ClassicPluginPreviewServer::Done);
        QVERIFY(!server.server.isBusy());
        QCOMPARE(replyOf(*socket).value(QLatin1String("result")).toString(),
                 QStringLiteral("done"));
    }

    // The project is unchanged if the modification file does not exist, as in the original
    // playinplugin.
    void a_missing_modification_changes_nothing() {
        QTemporaryDir dir;
        Server server;
        QVERIFY(server.listening);
        QJsonObject request;
        request.insert(QLatin1String("input"), inputOf(dir, first, count));
        request.insert(QLatin1String("modify"), dir.filePath(QStringLiteral("none.txt")));
        const auto socket = send(request);
        QVERIFY(socket);

        QTRY_VERIFY_WITH_TIMEOUT(server.request.has_value(), 5000);
        QCOMPARE(lyricsOf(server.request->preview.project), lyricsOf(project()));
        QVERIFY(server.request->play);
        QVERIFY(!server.request->output);
        server.server.reply(ClassicPluginPreviewServer::Cancelled);
        QCOMPARE(replyOf(*socket).value(QLatin1String("result")).toString(),
                 QStringLiteral("cancelled"));
    }

    // An input file of notes other than those passed to the plugin, and a line that is not a
    // request, are rejected with the reply failed.
    void a_request_that_does_not_match_is_rejected() {
        QTemporaryDir dir;
        Server server;
        QVERIFY(server.listening);
        QJsonObject request;
        request.insert(QLatin1String("input"), inputOf(dir, first + 1, count - 1));
        auto socket = send(request);
        QVERIFY(socket);
        auto reply = replyOf(*socket);
        QCOMPARE(reply.value(QLatin1String("result")).toString(), QStringLiteral("failed"));
        QVERIFY(!reply.value(QLatin1String("message")).toString().isEmpty());
        QVERIFY(server.rejection);
        QVERIFY(kit::hasError(*server.rejection));
        QVERIFY(!server.request);
        QVERIFY(!server.server.isBusy());

        server.rejection.reset();
        socket = send("not a request\n");
        QVERIFY(socket);
        reply = replyOf(*socket);
        QCOMPARE(reply.value(QLatin1String("result")).toString(), QStringLiteral("failed"));
        QVERIFY(server.rejection);
    }

    // One request is handled at a time. A second request meanwhile fails at once.
    void a_second_request_fails_while_one_awaits_its_reply() {
        QTemporaryDir dir;
        Server server;
        QVERIFY(server.listening);
        QJsonObject request;
        request.insert(QLatin1String("input"), inputOf(dir, first, count));
        const auto socket = send(request);
        QVERIFY(socket);
        QTRY_VERIFY_WITH_TIMEOUT(server.request.has_value(), 5000);

        const auto second = send(request);
        QVERIFY(second);
        QCOMPARE(replyOf(*second).value(QLatin1String("result")).toString(),
                 QStringLiteral("failed"));
        QVERIFY(server.server.isBusy());
        server.server.reply(ClassicPluginPreviewServer::Done);
        QCOMPARE(replyOf(*socket).value(QLatin1String("result")).toString(),
                 QStringLiteral("done"));
    }

    // If hello-plugin-play ends before the reply, as when the plugin terminates it, the request
    // is abandoned, and a later reply does nothing.
    void a_closed_connection_abandons_the_request() {
        QTemporaryDir dir;
        Server server;
        QVERIFY(server.listening);
        QJsonObject request;
        request.insert(QLatin1String("input"), inputOf(dir, first, count));
        auto socket = send(request);
        QVERIFY(socket);
        QTRY_VERIFY_WITH_TIMEOUT(server.request.has_value(), 5000);

        socket->abort();
        socket.reset();
        QTRY_VERIFY_WITH_TIMEOUT(server.abandoned, 5000);
        QVERIFY(!server.server.isBusy());
        server.server.reply(ClassicPluginPreviewServer::Done);
    }
};

QTEST_GUILESS_MAIN(test_ClassicPluginPreviewServer)

#include "test_ClassicPluginPreviewServer.moc"
