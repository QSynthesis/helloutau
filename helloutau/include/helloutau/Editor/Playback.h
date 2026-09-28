#ifndef HELLOUTAU_EDITOR_PLAYBACK_H
#define HELLOUTAU_EDITOR_PLAYBACK_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QObject>

#include <hellokit/Support/Diagnostic.h>
#include <hellokit/Synth/SynthRunner.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::kit {
    class ProjectDocument;
}

namespace hello::daw {

    class AudioOutput;

    /// Renders a project, or some of its notes, and plays the result: step 5 of docs/Widgets.md.
    ///
    /// Rendering runs on a worker thread with the engines of the settings, never those the
    /// project names, and reports its progress; it can be cancelled. The track file is then read,
    /// converted to the sample rate of the output device, and played. position() maps what is
    /// heard to the track, see kit::SynthPlan::startTime().
    ///
    /// The render cache of a document is the directory beside its \c .usth, or beside the UST it
    /// was imported from, as UTAU uses it; a document without a file renders into a temporary
    /// directory that lives as long as this object.
    class HELLOUTAU_EDITOR_EXPORT Playback : public QObject {
        Q_OBJECT
    public:
        enum State {
            Stopped,
            Rendering,
            Playing,
        };
        Q_ENUM(State)

        explicit Playback(QObject *parent = nullptr);
        ~Playback() override;

        /// Replaces the runner, for tests. The default is a kit::ThreadedSynthRunner.
        void setRunner(std::shared_ptr<const kit::SynthRunner> runner);

        State state() const;

        /// Starts rendering notes \a range of the first track of \a document, or all of them, and
        /// plays the result once it is rendered. Stops what played or rendered before.
        ///
        /// \return whether rendering started; the reason is in \a diagnostics otherwise, such as
        ///         a document without a voice bank or engines that are not set
        bool play(const kit::ProjectDocument &document, std::optional<std::pair<int, int>> range,
                  const kit::SynthEngines &engines, kit::DiagnosticList &diagnostics);

        /// Plays the track as it is rendered, from the start of note \a fromNote or from the
        /// start: the realtime preview of step 6 of docs/Widgets.md.
        ///
        /// Notes are resampled around the playback position and concatenated in the process, see
        /// kit::RealtimeSynth, whose fragments are kept for the next preview. When a note is not
        /// yet rendered, playback waits: isBuffering() holds and position() stands still. An edit
        /// takes effect through updatePlan().
        ///
        /// \return whether playing started; the reason is in \a diagnostics otherwise
        bool preview(const kit::ProjectDocument &document, std::optional<int> fromNote,
                     const kit::SynthEngines &engines, kit::DiagnosticList &diagnostics);

        /// Whether the preview waits for a note to be rendered.
        bool isBuffering() const;

        /// The notes the preview has still to render.
        int pendingNotes() const;

        /// Replaces the notes that the preview plays with those of \a document, after an edit.
        /// Does nothing unless a preview plays.
        void updatePlan(const kit::ProjectDocument &document);

        /// Returns the warnings of the notes the preview could not render since the last call.
        kit::DiagnosticList takePreviewDiagnostics();

        /// Cancels rendering, or stops playing.
        void stop();

        /// The position heard, in milliseconds from the start of the track, while playing.
        std::optional<double> position() const;

        /// The directory of the render cache of \a document.
        std::filesystem::path cacheDirectoryFor(const kit::ProjectDocument &document);

    Q_SIGNALS:
        void stateChanged(State state);

        /// \a done of \a total notes of the render in progress are processed.
        void progressed(int done, int total);

        /// Rendering or playing failed, for the reasons in \a diagnostics.
        void failed(const kit::DiagnosticList &diagnostics);

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_PLAYBACK_H
