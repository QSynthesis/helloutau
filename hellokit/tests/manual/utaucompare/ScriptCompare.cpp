#include "ScriptCompare.h"

#include <cmath>

#include <QtCore/QHash>

namespace utaucompare {

    namespace {

        using hello::kit::SynthStep;

        // The order stdutau's ResamplerArguments::arguments() builds, which is the order every
        // resampler reads. Everything from PITCH_AT on is the curve.
        constexpr const char *RESAMPLER_NAMES[] = {
            "sample",     "cacheFile", "tone",  "velocity",  "flags",      "offset",
            "realLength", "consonant", "blank", "intensity", "modulation", "tempo",
        };
        constexpr int PITCH_AT = 12;

        // WavtoolArguments::arguments(). Everything from ENVELOPE_AT on is the envelope.
        constexpr const char *WAVTOOL_NAMES[] = {
            "outputFile",
            "cacheFile",
            "startPoint",
            "outDuration",
        };
        constexpr int ENVELOPE_AT = 4;

        /// Below this, two numbers are the same number written differently. It is a thousandth
        /// of a millisecond, a twentieth of a sample at 44.1 kHz: UTAU rounds where it prints
        /// and we do not, and \c 11.539 against \c 11.5389 is that and nothing else.
        constexpr double SAME_NUMBER = 1e-3;

        /// Whether an argument is a measurement, and so whether the two sides differing in the
        /// last digit is a difference at all. Everything else is compared as the text it is: a
        /// flag, a tone name and a path mean what they spell.
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

        /// Splits into runs of number and runs of anything else, so that \c 240@120-0.024 can be
        /// held against \c 240@120-.024 a piece at a time.
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

        /// \return whether the two say the same thing, which for a measurement means the same
        ///         number however it is spelled
        bool same(const QString &what, const QString &ours, const QString &theirs) {
            if (ours == theirs) {
                return true;
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

        QString nameAt(const char *const *names, int count, int position, const char *engine) {
            if (position < count) {
                return QString::fromLatin1(names[position]);
            }
            return QStringLiteral("%1 argument %2").arg(QLatin1String(engine)).arg(position + 1);
        }

        /// The arguments from \a from on, as one string. The envelope and the pitch are each
        /// several arguments that only mean anything together, and how many there are is part of
        /// what is being compared.
        QString rest(const QStringList &arguments, int from) {
            if (arguments.size() <= from) {
                return QString();
            }
            return arguments.mid(from).join(QLatin1Char(' '));
        }

        void compareRange(int noteIndex, const QStringList &ours, const QStringList &theirs,
                          const char *const *names, int nameCount, int upTo, const char *engine,
                          Comparison &out) {
            const int count = int(std::max(ours.size(), theirs.size()));
            for (int i = 0; i < std::min(count, upTo); ++i) {
                const QString what = nameAt(names, nameCount, i, engine);
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

        // UTAU numbers the notes it hands its helper; a rest goes to the wavtool directly and
        // carries no number. Both sides are in track order, so a call with no number of its own
        // belongs to the note at the same place in the sequence.
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
