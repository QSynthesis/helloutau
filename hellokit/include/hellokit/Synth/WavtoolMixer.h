#ifndef HELLOKIT_SYNTH_WAVTOOLMIXER_H
#define HELLOKIT_SYNTH_WAVTOOLMIXER_H

#include <functional>
#include <optional>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QStringList>

#include <hellokit/Synth/HelloKitSynthGlobal.h>

namespace hello::kit {

    /// One wavtool call as \c SynthPlan passes it, with its numbers read back from the arguments.
    ///
    /// Read from the arguments rather than taken from the calculation, so that the numbers are
    /// exactly those a wavtool receives.
    struct HELLOKIT_SYNTH_EXPORT WavtoolCall {
        /// The milliseconds skipped at the start of the fragment.
        double startPoint = 0;

        /// The milliseconds appended: the length in ticks at the tempo, plus the correction.
        double length = 0;

        /// Whether the call has an envelope. A rest has none, and contributes silence.
        bool hasEnvelope = false;

        /// \name The envelope, in the order of the arguments
        ///
        /// Positions in milliseconds and volumes in percent. \c p4 is 0 and \c p5 and \c v5
        /// absent unless given.
        /// @{
        double p1 = 0, p2 = 0, p3 = 0;
        double v1 = 0, v2 = 0, v3 = 0, v4 = 0;
        double overlap = 0;
        double p4 = 0;
        std::optional<double> p5;
        std::optional<double> v5;
        /// @}

        /// Reads \a arguments, which begin with the track file and the fragment, or returns
        /// \c std::nullopt if they are not in the form SynthPlan writes.
        static std::optional<WavtoolCall> parse(const QStringList &arguments);
    };

    /// Concatenates rendered fragments into a track as \c wavtool.exe does, in the process, so
    /// that any part of the track can be computed alone. See docs/claude/wavtool-concatenation.md
    /// for the rules and their measurement.
    ///
    /// All positions are samples at 44100 Hz, the rate of the fragments and of the track file.
    class HELLOKIT_SYNTH_EXPORT WavtoolMixer {
    public:
        static constexpr int sampleRate = 44100;

        /// Where one call places its fragment in the track.
        struct Segment {
            /// The first sample of the track that the call writes.
            qint64 start = 0;
            /// The number of samples it writes.
            qint64 length = 0;
            /// The samples of the fragment skipped before the first one written.
            qint64 skip = 0;
            /// Whether the call contributes silence, as a rest does.
            bool silent = false;
            /// The envelope as (sample within the segment, gain), in order, starting at 0.
            QList<std::pair<qint64, double>> envelope;
        };

        /// Places \a calls one after another, each starting where the track ended less its
        /// overlap, the first at 0.
        static QList<Segment> layOut(const QList<WavtoolCall> &calls);

        /// The end of the track that \a segments form.
        static qint64 lengthOf(const QList<Segment> &segments);

        /// Returns the fragment of segment \a index, or \c nullptr for silence.
        using FragmentGetter = std::function<const std::vector<qint16> *(int index)>;

        /// Writes samples \a first to \a first + \a count of the track into \a out.
        ///
        /// Each sample sums the contributions of the segments that cover it, in the order of the
        /// calls, and is limited to 16 bits after each addition, as each call of wavtool.exe
        /// limits it. A contribution is the fragment sample times the gain of the envelope there,
        /// truncated toward zero; past the end of a fragment it is zero.
        static void mix(const QList<Segment> &segments, const FragmentGetter &fragment,
                        qint64 first, qint64 count, qint16 *out);

        /// The gain of \a segment at sample \a position within it: linear between the points,
        /// where of several points at one position the last applies.
        static double gainAt(const Segment &segment, qint64 position);
    };

}

#endif // HELLOKIT_SYNTH_WAVTOOLMIXER_H
