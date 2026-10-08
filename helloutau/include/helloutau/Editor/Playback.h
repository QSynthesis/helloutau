#ifndef HELLOUTAU_EDITOR_PLAYBACK_H
#define HELLOUTAU_EDITOR_PLAYBACK_H

#include <filesystem>
#include <memory>
#include <optional>
#include <utility>

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
    /// project names, by \c temp.bat in a console as UTAU renders, or by several threads.
    /// Cancelling it kills the script with the engines it started, see
    /// kit::EngineProcess::runScript(), or lets the running engine calls end. The track file is
    /// then read, converted to the sample rate of the output device, and played. position()
    /// maps what is heard to the track, see kit::SynthPlan::startTime().
    ///
    /// Every plan is made on a worker thread from a snapshot of the document, because a plan of
    /// a track of many notes takes long enough to stall the window: that of a render before it
    /// starts, which planProgressed() reports, and those of the realtime mode and of
    /// noteStates(). A failure found in the plan of a render is reported by failed().
    ///
    /// The render cache of a document is the directory beside its \c .usth, or beside the UST it
    /// was imported from, as UTAU uses it. Every render also requires the temporary directory of
    /// setTemporaryDirectory(), which the owning project window supplies: it holds the track
    /// file of play(), the scripts and the log, and the render cache of a document without a
    /// file. Without it, play(), renderTrack(), preview() and prepare() fail.
    class HELLOUTAU_EDITOR_EXPORT Playback : public QObject {
        Q_OBJECT
    public:
        enum State {
            Stopped,
            Rendering,
            Playing,
            /// Stopped where it was, to go on from there with resume(), or with a preview from
            /// position() for a paused preview
            Paused,
        };
        Q_ENUM(State)

        explicit Playback(QObject *parent = nullptr);
        Playback(std::shared_ptr<kit::EngineOutputLog> outputLog, QObject *parent);
        Playback(std::shared_ptr<kit::EngineOutputLog> outputLog,
                 std::filesystem::path temporaryDirectory, QObject *parent);
        ~Playback() override;

        /// Replaces the runner, for tests. The default is a kit::ClassicSynthRunner.
        void setRunner(std::shared_ptr<const kit::SynthRunner> runner);

        /// Sets the directory owned by the project window for renders, scripts, and logs, and
        /// forgets the kept render and clears the log, which belong to the previous directory.
        /// Playback never creates or removes this directory.
        ///
        /// \note No render, plan, scan or preview may be under way, because each of them writes
        ///       into or reads from the previous directory. The caller calls stopAndWait() first.
        ///       The function asserts the condition.
        void setTemporaryDirectory(std::filesystem::path temporaryDirectory);

        /// Sets the number of threads of the realtime synthesis, zero for one per hardware
        /// thread, the default. A change takes effect with the next preview or prepare().
        void setThreadCount(int count);

        State state() const;

        /// Starts rendering notes \a range of the first track of \a document, or all of them, and
        /// plays the result once it is rendered. Stops what played or rendered before.
        ///
        /// The last render is kept: while every engine call of the notes would be the same, it
        /// plays again at once, without the engines.
        ///
        /// \return whether rendering started. The reason is in \a diagnostics otherwise, such as
        ///         a document without a voice bank, engines that are not set, or a render
        ///         cancelled before that has not ended. The reasons found in the plan, such as
        ///         a range without notes, are reported by failed().
        bool play(const kit::ProjectDocument &document, std::optional<std::pair<int, int>> range,
                  const kit::SynthEngines &engines, kit::DiagnosticList &diagnostics);

        /// Starts rendering the whole track of \a document into \a file with the runner of
        /// setRunner(), and emits trackRendered() once the file is written. Nothing is played,
        /// and the kept render of play() stays as it is. Stops what played or rendered before.
        /// The state is Rendering until the render ends, and stop() cancels it.
        ///
        /// \return whether rendering started; the reason is in \a diagnostics otherwise
        bool renderTrack(const kit::ProjectDocument &document, const std::filesystem::path &file,
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
        /// \a engines must name both engines, although the preview runs no wavtool, so that
        /// every playback mode requires the engines that Render Track requires.
        ///
        /// The state is Rendering until the plan of the track is made, and Playing from then
        /// on. A plan that cannot be made is reported by failed().
        ///
        /// \return whether the preview started. The reason is in \a diagnostics otherwise.
        bool preview(const kit::ProjectDocument &document, std::optional<double> fromTime,
                     const kit::SynthEngines &engines, kit::DiagnosticList &diagnostics);

        /// Renders the track of \a document in the background, the notes after \a fromTime
        /// first, so that preview() from there plays at once: the realtime mode. Called again
        /// after the playhead moves, it renders from there first. While a preview plays, only
        /// the notes are replaced, as by updatePlan(). \a engines must name both engines, as for
        /// preview().
        ///
        /// \return whether rendering started. The reason is in \a diagnostics otherwise.
        bool prepare(const kit::ProjectDocument &document, std::optional<double> fromTime,
                     const kit::SynthEngines &engines, kit::DiagnosticList &diagnostics);

        /// Stops the preview and the rendering in the background, and forgets the fragments
        /// held in memory: the prerender mode.
        void release();

        /// Whether the preview waits for a note to be rendered.
        bool isBuffering() const;

        /// The notes of the preview or of prepare() still to render.
        int pendingNotes() const;

        /// The progress in notes of the plan of the preview or of prepare() while it is made,
        /// as done and total, or \c std::nullopt if none is made. The total is zero until the
        /// notes are counted.
        std::optional<std::pair<int, int>> planProgress() const;

        /// How far each note of the first track is rendered, by its index: as the preview or
        /// prepare() has it, or else by the fragments in the render cache as the last
        /// refreshNoteStates() found them, Ready if the fragment is there and Waiting if not.
        /// Empty without a voice bank.
        QList<kit::RealtimeSynth::NoteState> noteStates() const;

        /// Updates noteStates() for \a document and emits noteStatesChanged() once it is
        /// updated: at once while the preview or prepare() renders, and otherwise after the
        /// render cache is scanned on a worker thread.
        void refreshNoteStates(const kit::ProjectDocument &document);

        /// Replaces the notes that the preview plays, or that prepare() renders, with those of
        /// \a document once their plan is made, after an edit. Does nothing unless either is
        /// under way.
        void updatePlan(const kit::ProjectDocument &document);

        /// Returns the warnings of the notes the preview could not render since the last call.
        kit::DiagnosticList takePreviewDiagnostics();

        /// Pauses what plays, keeping position(), and returns whether something played.
        bool pause();

        /// Plays a paused render on from where it was paused, and returns whether it did. A
        /// paused preview is not resumed here: the caller previews from position().
        bool resume();

        /// Whether a preview is paused.
        bool isPreviewPaused() const;

        /// Cancels rendering, or stops playing or being paused.
        void stop();

        /// Stops as stop() does, ends the preview, the plans and the scans of the render cache,
        /// kills the engines that they started, and waits until every worker thread has ended,
        /// so that nothing writes into the temporary directory afterwards. Unlike stop(), the
        /// function blocks, and the fragments held in memory are released. The owner calls it
        /// before the temporary directory is replaced or removed.
        void stopAndWait();

        /// The position heard, in milliseconds from the start of the track, while playing, or
        /// where playback was paused.
        std::optional<double> position() const;

        /// Returns the track file of the last completed render of play(), the \c temp.wav of
        /// UTAU, or an empty path if play() has not completed a render. The file stays in the
        /// render cache until the next render or until the cache is cleared.
        std::filesystem::path lastRenderFile() const;

        /// Returns the directory of the render cache of \a document: beside its file, or in the
        /// temporary directory if it has no file. Returns \c std::nullopt if the document has no
        /// file and no temporary directory is set.
        std::optional<std::filesystem::path>
            cacheDirectoryFor(const kit::ProjectDocument &document);

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

        /// \a done of \a total steps of the render in progress are complete. Emitted with zero
        /// and zero once the plan is made and the runner starts, before the steps are known.
        ///
        /// \sa kit::SynthObserver::progressed()
        void progressed(int done, int total);

        /// The plan of the render in progress, or of the preview or prepare(), covers \a done
        /// of \a total notes. The plans of a render and of the realtime mode report alike.
        void planProgressed(int done, int total);

        /// noteStates() changed: refreshNoteStates() updated it, or a new plan of the preview
        /// or of prepare() arrived.
        void noteStatesChanged();

        /// The render of renderTrack() wrote \a file.
        void trackRendered(const std::filesystem::path &file);

        /// Rendering or playing failed, for the reasons in \a diagnostics.
        void failed(const kit::DiagnosticList &diagnostics);

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_PLAYBACK_H
