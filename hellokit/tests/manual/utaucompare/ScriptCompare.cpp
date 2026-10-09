#include "ScriptCompare.h"

#include <cmath>

#include <QtCore/QHash>

namespace utaucompare {

    namespace {

        using hello::kit::SynthStep;

        // The order produced by ResamplerArguments::arguments() of stdutau, which is the order
        // every resampler reads. All arguments from PITCH_AT onward form the curve.
        constexpr const char *RESAMPLER_NAMES[] = {
            "sample",     "cacheFile", "tone",  "velocity",  "flags",      "offset",
            "realLength", "consonant", "blank", "intensity", "modulation", "tempo",
        };
        constexpr int PITCH_AT = 12;

        // WavtoolArguments::arguments(). All arguments from ENVELOPE_AT onward form the envelope.
        constexpr const char *WAVTOOL_NAMES[] = {
            "outputFile",
            "cacheFile",
            "startPoint",
            "outDuration",
        };
        constexpr int ENVELOPE_AT = 4;

        /// Below this tolerance, two numbers are considered equal. It is one thousandth of a
        /// millisecond, a twentieth of a sample at 44.1 kHz. UTAU rounds on output and HelloUtau
        /// does not, and \c 11.539 versus \c 11.5389 reflects only that.
        constexpr double SAME_NUMBER = 1e-3;

        /// Returns whether an argument is a measurement, for which a difference in the last
        /// digit is not a difference. All other arguments are compared as text, because a flag,
        /// a tone name and a path are defined by their exact text.
        bool isMeasurement(const QString &what) {
            static const QStringList names = {
                QStringLiteral("velocity"),    QStringLiteral("offset"),
                QStringLiteral("realLength"),  QStringLiteral("consonant"),
                QStringLiteral("blank"),       QStringLiteral("intensity"),
                QStringLiteral("modulation"),  QStringLiteral("startPoint"),
                QStringLiteral("outDuration"), QStringLiteral("envelope"),
            };
            return names.contains(what);
        }

        bool startsNumber(const QString &text, int at) {
            const bool sign = text.at(at) == QLatin1Char('-') || text.at(at) == QLatin1Char('+');
            const int after = sign ? at + 1 : at;
            return after < text.size() &&
                   (text.at(after).isDigit() || text.at(after) == QLatin1Char('.'));
        }

        /// Splits text into numeric and non-numeric runs, so that \c 240@120-0.024 and
        /// \c 240@120-.024 can be compared run by run.
        QStringList pieces(const QString &text) {
            QStringList out;
            int i = 0;
            while (i < text.size()) {
                const int start = i;
                if (startsNumber(text, i)) {
                    i += (text.at(i).isDigit() || text.at(i) == QLatin1Char('.')) ? 0 : 1;
                    while (i < text.size() &&
                           (text.at(i).isDigit() || text.at(i) == QLatin1Char('.'))) {
                        ++i;
                    }
                } else {
                    while (i < text.size() && !startsNumber(text, i)) {
                        ++i;
                    }
                }
                out += text.mid(start, i - start);
            }
            return out;
        }

        /// \return whether the two are equivalent. Two measurements are equivalent if they are
        ///         numerically equal, regardless of formatting.
        bool same(const QString &what, const QString &ours, const QString &theirs) {
            if (ours == theirs) {
                return true;
            }
            // A path names the same file with either separator, which Windows accepts alike.
            if (what == QLatin1String("sample") || what == QLatin1String("cacheFile") ||
                what == QLatin1String("outputFile")) {
                return QString(ours).replace(QLatin1Char('\\'), QLatin1Char('/')) ==
                       QString(theirs).replace(QLatin1Char('\\'), QLatin1Char('/'));
            }
            if (!isMeasurement(what)) {
                return false;
            }
            const QStringList a = pieces(ours);
            const QStringList b = pieces(theirs);
            if (a.size() != b.size()) {
                return false;
            }
            for (int i = 0; i < a.size(); ++i) {
                if (a.at(i) == b.at(i)) {
                    continue;
                }
                bool okA = false;
                bool okB = false;
                const double x = a.at(i).toDouble(&okA);
                const double y = b.at(i).toDouble(&okB);
                if (!okA || !okB || std::abs(x - y) > SAME_NUMBER) {
                    return false;
                }
            }
            return true;
        }

        QString nameAt(const char *const *names, int count, int position, const char *synthTool) {
            if (position < count) {
                return QString::fromLatin1(names[position]);
            }
            return QStringLiteral("%1 argument %2").arg(QLatin1String(synthTool)).arg(position + 1);
        }

        /// The arguments from \a from onward, as one string. The envelope and the pitch each
        /// consist of several arguments that are meaningful only together, and their count is
        /// part of the comparison.
        QString rest(const QStringList &arguments, int from) {
            if (arguments.size() <= from) {
                return QString();
            }
            return arguments.mid(from).join(QLatin1Char(' '));
        }

        void compareRange(int noteIndex, const QStringList &ours, const QStringList &theirs,
                          const char *const *names, int nameCount, int upTo, const char *synthTool,
                          Comparison &out) {
            const int count = int(std::max(ours.size(), theirs.size()));
            for (int i = 0; i < std::min(count, upTo); ++i) {
                const QString what = nameAt(names, nameCount, i, synthTool);
                const QString a = ours.value(i);
                const QString b = theirs.value(i);
                if (a == b) {
                    continue;
                }
                if (same(what, a, b)) {
                    out.spelling++;
                    continue;
                }
                out.arguments += ArgumentDifference{noteIndex, what, a, b};
            }
        }

    }

    Comparison compare(const QList<SynthStep> &ours, const QList<ScriptCall> &theirs) {
        Comparison out;

        // UTAU numbers the notes passed to its helper. A rest is passed directly to the wavtool
        // without a number. Both sides are in track order, so a call without a number belongs to
        // the note at the same position in the sequence.
        QHash<int, const ScriptCall *> byIndex;
        int running = 0;
        for (const ScriptCall &call : theirs) {
            const int index = call.noteIndex.value_or(running);
            byIndex.insert(index, &call);
            running = index + 1;
        }

        for (const SynthStep &step : ours) {
            const auto found = byIndex.constFind(step.noteIndex);
            if (found == byIndex.constEnd()) {
                out.onlyOurs++;
                continue;
            }
            const ScriptCall &call = **found;
            out.notesCompared++;

            compareRange(step.noteIndex, step.resamplerArguments, call.resamplerArguments,
                         RESAMPLER_NAMES, int(std::size(RESAMPLER_NAMES)), PITCH_AT, "resampler",
                         out);
            compareRange(step.noteIndex, step.wavtoolArguments, call.wavtoolArguments,
                         WAVTOOL_NAMES, int(std::size(WAVTOOL_NAMES)), ENVELOPE_AT, "wavtool", out);

            const QString envelope = QStringLiteral("envelope");
            const QString ourEnvelope = rest(step.wavtoolArguments, ENVELOPE_AT);
            const QString theirEnvelope = rest(call.wavtoolArguments, ENVELOPE_AT);
            if (ourEnvelope != theirEnvelope) {
                if (same(envelope, ourEnvelope, theirEnvelope)) {
                    out.spelling++;
                } else {
                    out.arguments +=
                        ArgumentDifference{step.noteIndex, envelope, ourEnvelope, theirEnvelope};
                }
            }

            const QList<int> ourCurve = decodePitch(rest(step.resamplerArguments, PITCH_AT));
            const QList<int> theirCurve = decodePitch(rest(call.resamplerArguments, PITCH_AT));
            if (!ourCurve.isEmpty() || !theirCurve.isEmpty()) {
                out.curves += CurveDifference{step.noteIndex,
                                              compareCurves(ourCurve, theirCurve, &out.readings),
                                              int(ourCurve.size()), int(theirCurve.size())};
            }
        }

        out.onlyTheirs = int(byIndex.size()) - out.notesCompared;
        return out;
    }

}
