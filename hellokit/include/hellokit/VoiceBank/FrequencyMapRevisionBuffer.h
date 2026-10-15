#ifndef HELLOKIT_VOICEBANK_FREQUENCYMAPREVISIONBUFFER_H
#define HELLOKIT_VOICEBANK_FREQUENCYMAPREVISIONBUFFER_H

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include <hellokit/VoiceBank/FrequencyMap.h>

namespace hello::kit {

    /// A frequency map being edited, with the map as last saved and a history of the edits for
    /// undo and redo. See the editing in docs/FrequencyMaps.md.
    ///
    /// Only the editing dialog holds the buffer. The buffer belongs to no editing session and is
    /// excluded from crash recovery, therefore the history ends with the dialog.
    ///
    /// An edit changes the frequencies of frames and nothing else. The frames, their times and
    /// amplitudes and the average frequency stay as read. A frame is voiced if its frequency is
    /// above lowestFrequency, as in frqeditor. Each frequency that an edit writes is limited as
    /// in frqeditor:
    /// - A frequency above highestFrequency becomes highestFrequency.
    /// - A frequency not above lowestFrequency marks the frame unvoiced.
    ///
    /// Each edit that changes a frequency is one step of the history, and an edit that changes no
    /// frequency adds no step. A new step discards the undone steps. A range of frames is
    /// limited to the frames of the map.
    class HELLOKIT_VOICEBANK_EXPORT FrequencyMapRevisionBuffer {
    public:
        /// The frequency up to which a frame is unvoiced, A1 in hertz
        static constexpr double lowestFrequency = 55;

        /// The highest frequency that an edit writes, B8 in hertz
        static constexpr double highestFrequency = 7902.132820097988;

        /// Creates the buffer of \a map, which is also the saved map.
        explicit FrequencyMapRevisionBuffer(FrequencyMap map);

        /// Returns the map with the edits.
        const FrequencyMap &map() const;

        /// Returns the map as last saved, or as created if not saved since.
        const FrequencyMap &savedMap() const;

        /// Returns whether a frequency of map() differs from that of savedMap().
        bool isModified() const;

        /// Records map() as the saved map and keeps the history.
        void markSaved();

        /// Starts a stroke at \a frame with \a frequency, in hertz or 0 for an unvoiced frame,
        /// and writes the frequency at once. Ends a stroke in progress first.
        void beginStroke(std::size_t frame, double frequency);

        /// Continues the stroke in progress to \a frame with \a frequency. The frames from the
        /// previous point of the stroke to \a frame, both included, take frequencies interpolated
        /// linearly on the pitch scale, or are unvoiced if either point is unvoiced. Does nothing
        /// if no stroke is in progress.
        void strokeTo(std::size_t frame, double frequency);

        /// Ends the stroke in progress as one step.
        ///
        /// \return whether the stroke changed a frequency
        bool endStroke();

        /// Restores the frequencies from before the stroke in progress and ends the stroke
        /// without a step.
        void cancelStroke();

        /// Returns whether a stroke is in progress.
        bool isStroking() const;

        /// Multiplies the frequencies of the voiced frames among \a count frames from \a first by
        /// \a factor, as one step. Returns whether a frequency changed, and false while a stroke
        /// is in progress.
        bool scale(std::size_t first, std::size_t count, double factor);

        /// Marks \a count frames from \a first unvoiced, as one step. Returns whether a frequency
        /// changed, and false while a stroke is in progress.
        bool unvoice(std::size_t first, std::size_t count);

        /// Interpolates the frequencies of the frames between the first and the last of \a count
        /// frames from \a first linearly in hertz, as the avg button of frqeditor does, as one
        /// step. Returns whether a frequency changed, and false if either end is unvoiced or a
        /// stroke is in progress.
        bool interpolate(std::size_t first, std::size_t count);

        /// Restores the frequencies of the saved map to \a count frames from \a first, as one
        /// step. Returns whether a frequency changed, and false while a stroke is in progress.
        bool revert(std::size_t first, std::size_t count);

        /// Returns whether a step can be undone, which requires no stroke in progress.
        bool canUndo() const;

        /// Returns whether an undone step can be redone, which requires no stroke in progress.
        bool canRedo() const;

        /// Undoes the last step. Returns false if canUndo() is false.
        bool undo();

        /// Redoes the last undone step. Returns false if canRedo() is false.
        bool redo();

        /// Returns \a frequency limited as an edit writes it.
        static double limited(double frequency);

    private:
        // The frequencies of consecutive frames before and after a step
        struct Step {
            std::size_t first = 0;
            std::vector<double> before;
            std::vector<double> after;
        };

        struct Stroke {
            // The previous point
            std::size_t frame = 0;
            double frequency = 0;
            // The frequencies of every frame before the stroke
            std::vector<double> before;
        };

        FrequencyMap m_map;
        FrequencyMap m_saved;
        std::vector<Step> m_steps;
        // The number of steps done, which is the index of the step that redo() redoes
        std::size_t m_done = 0;
        std::optional<Stroke> m_stroke;

        // Returns the frames of count frames from first within the map, as the first frame and
        // the count
        std::pair<std::size_t, std::size_t> rangeOf(std::size_t first, std::size_t count) const;

        // Writes frequencies to the frames from first as one step without the unchanged frames
        // at either end, and returns whether a frequency changed
        bool apply(std::size_t first, const std::vector<double> &frequencies);

        void write(std::size_t first, const std::vector<double> &frequencies);
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYMAPREVISIONBUFFER_H
