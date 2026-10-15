#ifndef HELLOKIT_VOICEBANK_FREQUENCYMAPREVISIONBUFFER_H
#define HELLOKIT_VOICEBANK_FREQUENCYMAPREVISIONBUFFER_H

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include <hellokit/VoiceBank/FrequencyMap.h>
#include <hellokit/VoiceBank/FrequencyMapCheck.h>

namespace hello::kit {

    /// A frequency map being edited, with the map as last saved and a history of the edits for
    /// undo and redo. See the editing in docs/FrequencyMaps.md.
    ///
    /// Only the editing dialog holds the buffer. The buffer belongs to no editing session and is
    /// excluded from crash recovery, therefore the history ends with the dialog.
    ///
    /// An edit changes the frequencies of frames and the average frequency, the key of frqeditor,
    /// and nothing else. The frames, their times and amplitudes stay as read. The key is edited
    /// only if the map records an average frequency. A frame is voiced if its frequency is above
    /// lowestFrequency, as in frqeditor. Each frequency that an edit writes is limited as in
    /// frqeditor:
    /// - A frequency above highestFrequency becomes highestFrequency.
    /// - A frequency not above lowestFrequency marks the frame unvoiced.
    ///
    /// Each edit that changes the frequency of a frame or the key is one step of the history, and
    /// an edit that changes neither adds no step. A new step discards the undone steps. A range of
    /// frames is limited to the frames of the map.
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

        /// Returns whether a frequency or the average frequency of map() differs from that of
        /// savedMap().
        bool isModified() const;

        /// Returns the average frequency of map(), the key of frqeditor, or
        /// FrequencyMap::pitchAverage() if the map records none.
        double key() const;

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

        /// Restores the frequencies and the average frequency of the saved map, as one step.
        /// Returns whether a value changed, and false while a stroke is in progress.
        bool revertAll();

        /// Sets the frequency of \a frame to \a frequency, as the Set button of frqeditor does, as
        /// one step. Returns whether it changed, and false while a stroke is in progress.
        bool setFrequency(std::size_t frame, double frequency);

        /// Sets the average frequency to \a frequency, at least lowestFrequency and at most
        /// highestFrequency, as one step. Returns whether it changed, and false if the map
        /// records no average frequency or a stroke is in progress.
        bool setKey(double frequency);

        /// Sets the average frequency to the arithmetic mean in hertz of the voiced frames among
        /// \a count frames from \a first, as the button that sets the key to the average of the
        /// selection in frqeditor does, as one step. Returns whether it changed, and false
        /// without a voiced frame or if setKey() returns false.
        bool setKeyToMean(std::size_t first, std::size_t count);

        /// Writes the proposed frequencies of \a corrections, from frame \a first, as frqeditor
        /// adopts the corrections of its check, as one step. A correction without a proposed
        /// frequency above 0 leaves its frame as it is. If the map records an average frequency,
        /// the step also sets it to the mean of the voiced frames on the pitch scale, in whole
        /// cents rounded toward zero, or to C4 without a voiced frame. Returns whether a value
        /// changed, and false while a stroke is in progress.
        bool adoptCorrections(std::size_t first,
                              const std::vector<FrequencyMapCheck::Correction> &corrections);

        /// Sets every frame and, if the map records one, the average frequency to \a frequency,
        /// as a double click on the labels of frqeditor does, as one step. The frames take the
        /// limited frequency, and the key takes at least lowestFrequency. Returns whether a value
        /// changed, and false while a stroke is in progress.
        bool flatten(double frequency);

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
        // The frequencies of consecutive frames before and after a step, and the average
        // frequency before and after if the step changes it
        struct Step {
            std::size_t first = 0;
            std::vector<double> before;
            std::vector<double> after;
            std::optional<std::pair<std::optional<double>, std::optional<double>>> key;
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

        // Writes frequencies to the frames from first and key to the average frequency, if given,
        // as one step without the unchanged frames at either end, and returns whether a value
        // changed
        bool apply(std::size_t first, const std::vector<double> &frequencies,
                   std::optional<std::optional<double>> key = std::nullopt);

        void write(std::size_t first, const std::vector<double> &frequencies);
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYMAPREVISIONBUFFER_H
