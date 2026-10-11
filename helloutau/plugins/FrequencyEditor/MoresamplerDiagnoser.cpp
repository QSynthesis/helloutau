#include "MoresamplerDiagnoser.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QStringList>

namespace fs = std::filesystem;

namespace hello::daw {

    namespace {

        fs::path configurationOf(const fs::path &resampler) {
            return resampler.parent_path() / "moreconfig.txt";
        }

        QString displayed(const fs::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

    }

    QString MoresamplerDiagnoser::id() const {
        return QLatin1String(diagnoserId);
    }

    int MoresamplerDiagnoser::priority() const {
        return 100;
    }

    QList<kit::SynthDiagnosis>
        MoresamplerDiagnoser::diagnose(const kit::SynthDiagnosisContext &context) const {
        QList<int> failures;
        for (int i = 0; i < context.failures.size(); ++i) {
            const auto &failure = context.failures.at(i);
            // The fragment of a note, which the wavtool rejected or realtime playback could not
            // read, or which exists although the classic wavtool wrote no track
            const bool ofFragment =
                failure.stage == kit::SynthFailure::Fragment ||
                failure.stage == kit::SynthFailure::Unconfirmed ||
                (failure.stage == kit::SynthFailure::Wavtool && failure.noteIndex.has_value());
            if (ofFragment && failure.fileHead.startsWith(placeholder)) {
                failures.push_back(i);
            }
        }
        if (failures.isEmpty()) {
            return {};
        }

        const auto configuration = displayed(configurationOf(context.synthTools.resampler));
        const auto clearCache = tr("Clear the render cache after the change, because the "
                                   "fragments already written remain placeholders.");
        kit::SynthDiagnosis diagnosis;
        diagnosis.failures = failures;
        diagnosis.cause = context.joinedInternally
                              ? tr("Realtime playback cannot read the fragments of moresampler, "
                                   "which writes LLSM data instead of wave files.")
                              : tr("Moresampler writes LLSM data instead of wave files, which "
                                   "the wavtool cannot join.");
        // Moresampler writes wave files with the option on and without the file, so that the
        // fragments are those of an earlier render.
        const auto option = compatibility(context.synthTools.resampler);
        if (option != false) {
            diagnosis.explanation =
                (option ? tr("The option resampler-compatibility is on in %1, so that moresampler "
                             "wrote these fragments before the change.")
                        : tr("The file %1 does not exist, in which case moresampler writes wave "
                             "files, so that moresampler wrote these fragments before the "
                             "change."))
                    .arg(configuration) +
                QLatin1Char(' ') +
                tr("Moresampler with the option off writes a placeholder text at the path of "
                   "each fragment and the data beside it with the extension .llsm.tmp.");
            diagnosis.remedies = {clearCache};
            return {diagnosis};
        }
        diagnosis.explanation =
            tr("With the option resampler-compatibility off in %1, moresampler writes a "
               "placeholder text at the path of each fragment and the data beside it with the "
               "extension .llsm.tmp. Only moresampler as the wavtool joins this data.")
                .arg(configuration);
        diagnosis.remedies.push_back(
            tr("Set resampler-compatibility to on in %1.").arg(configuration));
        // Realtime playback joins the fragments itself, regardless of the wavtool.
        if (!context.joinedInternally) {
            diagnosis.remedies.push_back(tr("Alternatively, set the wavtool to moresampler as "
                                            "well."));
        }
        diagnosis.remedies.push_back(clearCache);
        return {diagnosis};
    }

    std::optional<bool> MoresamplerDiagnoser::compatibility(const fs::path &resampler) {
        QFile file(QString::fromStdU16String(configurationOf(resampler).u16string()));
        if (!file.open(QIODevice::ReadOnly)) {
            return std::nullopt;
        }
        // Lines of an option name and a value, with comments after "#". An option with a
        // leading "*" in the global configuration overrides the configuration of a voice bank,
        // which has no output options.
        std::optional<bool> value;
        for (auto line : QString::fromLatin1(file.readAll()).split(u'\n')) {
            line = line.section(u'#', 0, 0).trimmed();
            if (line.startsWith(u'*')) {
                line.remove(0, 1);
            }
            const auto parts = line.simplified().split(u' ');
            if (parts.size() == 2 && parts.at(0) == u"resampler-compatibility") {
                value = parts.at(1) == u"on";
            }
        }
        return value;
    }

}
