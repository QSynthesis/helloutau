#ifndef HELLOUTAU_EDITOR_PLAYBACK_H
#define HELLOUTAU_EDITOR_PLAYBACK_H

#include <filesystem>
#include <memory>
#include <optional>

#include <QtCore/QObject>

#include <hellokit/Support/Diagnostic.h>
#include <hellokit/Synth/RealtimeSynth.h>
#include <hellokit/Synth/SynthRunner.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::kit {
    class ProjectDocument;
}

namespace hello::daw {

    class AudioOutput;

    /// Plays a project in either of the playback modes of docs/Widgets.md: renders some of its
    /// notes and plays the result, or renders the track in the background and plays it as it is
    /// rendered.
    ///
    /// A render runs on a worker thread with the engines of the settings, never those the
    /// project names, by \c temp.bat in a console as UTAU renders; it can be cancelled, though
    /// the script runs on until it ends or its window is closed. The track file is then read,
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

        /// Replaces the runner, for tests. The default is a kit::ClassicSynthRunner.
        void setRunner(std::shared_ptr<const kit::SynthRunner> runner);

        State state() const;

        /// Starts rendering notes \a range of the first track of \a document, or all of them, and
        /// plays the result once it is rendered. Stops what played or rendered before.
        ///
        /// \return whether rendering started; the reason is in \a diagnostics otherwise, such as
        ///         a document without a voice bank, engines that are not set, or a render
        ///         cancelled before that has not ended
        bool play(const kit::ProjectDocument &document, std::optional<std::pair<int, int>> range,
                  const kit::SynthEngines &engines, kit::DiagnosticList &diagnostics);

        /// Plays the track as it is rendered, from \a fromTime in milliseconds from the start of
        /// the track, or from the start: the realtime mode of docs/Widgets.md. The notes that
        /// sound at that time are heard from there on, and those prepare() rendered are kept.
        ///
        /// Notes are resampled around the playback position and concatenated in the process, see
        /// kit::RealtimeSynth, whose fragments are kept for the next preview. When a note is not
        /// yet rendered, playback waits: isBuffering() holds and position() stands still. An edit
        /// takes effect through updatePlan().
        ///
        /// \return whether playing started; the reason is in \a diagnostics otherwise
        bool preview(const kit::ProjectDocument &document, std::optional<double> fromTime,
                     const kit::SynthEngines &engines, kit::DiagnosticList &diagnostics);

        /// Renders the track of \a document in the background, the notes after \a fromTime
        /// first, so that preview() from there plays at once: the realtime mode. Called again
        /// after the playhead moves, it renders from there first. While a preview plays, only
        /// the notes are replaced, as by updatePlan().
        ///
        /// \return whether rendering started; the reason is in \a diagnostics otherwise
        bool prepare(const kit::ProjectDocument &document, std::optional<double> fromTime,
                     const kit::SynthEngines &engines, kit::DiagnosticList &diagnostics);

        /// Stops the preview and the rendering in the background, and forgets the fragments
        /// held in memory: the prerender mode.
        void release();

        /// Whether the preview waits for a note to be rendered.
        bool isBuffering() const;

        /// The notes of the preview or of prepare() still to render.
        int pendingNotes() const;

        /// How far each note of the first track of \a document is rendered, by its index: as
        /// the preview or prepare() has it, or else by the fragments in the render cache, Ready
        /// where the fragment is there and Waiting where not. Empty without a voice bank.
        QList<kit::RealtimeSynth::NoteState> noteStates(const kit::ProjectDocument &document);

        /// Replaces the notes that the preview plays, or that prepare() renders, with those of
        /// \a document, after an edit. Does nothing unless either is under way.
        void updatePlan(const kit::ProjectDocument &document);

        /// Returns the warnings of the notes the preview could not render since the last call.
        kit::DiagnosticList takePreviewDiagnostics();

        /// Cancels rendering, or stops playing.
        void stop();

        /// The position heard, in milliseconds from the start of the track, while playing.
        std::optional<double> position() const;

        /// The directory of the render cache of \a document.
        std::filesystem::path cacheDirectoryFor(const kit::ProjectDocument &document);

        /// Stops playing, forgets the fragments held in memory, and deletes the files of the
        /// render cache of \a document: the files directly in the directory, which the engines
        /// and the renders wrote, and not the folders in it.
        ///
        /// \return the number of files deleted, or \c std::nullopt if a render has not ended,
        ///         whose script would write into the cache meanwhile; the files that could not
        ///         be deleted are in \a diagnostics
        std::optional<int> clearCache(const kit::ProjectDocument &document,
                                      kit::DiagnosticList &diagnostics);

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
