#ifndef HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINPREVIEWSERVER_H
#define HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINPREVIEWSERVER_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <hellokit/Support/Diagnostic.h>

#include <ClassicPluginHost/ClassicPluginExchange.h>
#include <ClassicPluginHost/ClassicPluginHostPluginGlobal.h>

namespace hello::daw {

    class ClassicPlugin;

    /// The pipe through which hello-plugin-play passes the previews of one ClassicPlugin run to
    /// the host. See docs/PluginPlay.md.
    ///
    /// The pipe is named after the process of the plugin, which hello-plugin-play finds in its
    /// job object. Each request is one line of JSON. A request whose input file does not match
    /// the selection of the run is rejected. For a request that matches the run, the result of
    /// the plugin is applied to a copy of the project, and requested() is emitted with the copy.
    /// The receiver renders it and calls reply(). One request is handled at a time.
    class CLASSICPLUGINHOST_EXPORT ClassicPluginPreviewServer : public QObject {
        Q_OBJECT
    public:
        /// A preview requested by hello-plugin-play, in the terms of the original command line.
        struct Request {
            /// The project with the result of the plugin applied, and the selection in it
            ClassicPluginExchange::Preview preview;

            /// The file into which the rendered notes are written (\c /o: ), if any
            std::optional<std::filesystem::path> output;

            /// Whether the rendered notes are played, false for \c /noplay
            bool play = true;

            /// Whether \a output is kept after the reply, true for \c /nodeletewav
            bool keepOutput = false;
        };

        enum Result {
            Done,     ///< the preview was rendered, and played if requested
            Canceled, ///< the user canceled the preview
            Failed,   ///< the preview could not be rendered
        };

        /// Creates the server of the run of \a plugin on the \a count notes of \a project from
        /// index \a first , as passed to ClassicPluginExchange::input().
        ClassicPluginPreviewServer(const ClassicPlugin &plugin, const kit::Project &project,
                                   int first, int count, QObject *parent = nullptr);
        ~ClassicPluginPreviewServer();

        /// Returns the name of the pipe of the plugin process \a processId .
        static QString pipeName(qint64 processId);

        /// Starts to accept requests on the pipe of the plugin process \a processId .
        ///
        /// \return whether the pipe was created. If it was not, \a error receives the reason.
        bool listen(qint64 processId, QString *error);

        /// Returns whether a request awaits reply().
        bool isBusy() const;

        /// Replies \a result to the request that awaits it, with \a message for Failed, and
        /// closes its connection. Does nothing if no request awaits a reply.
        void reply(Result result, const QString &message = {});

    Q_SIGNALS:
        /// Emitted for a request that matches the run. The receiver renders the preview and
        /// calls reply().
        void requested(const hello::daw::ClassicPluginPreviewServer::Request &request);

        /// Emitted after the reply Failed to a request that does not match the run or cannot be
        /// read, with the reasons.
        void rejected(const hello::kit::DiagnosticList &diagnostics);

        /// Emitted if the connection of the request that awaits reply() closes, as when the
        /// plugin terminates hello-plugin-play. The receiver stops the preview.
        void abandoned();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_CLASSICPLUGINHOST_CLASSICPLUGINPREVIEWSERVER_H
