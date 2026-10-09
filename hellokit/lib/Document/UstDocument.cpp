#include "UstDocument.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <stdutau/ustfile.h>

#include <hellokit/Support/TextCodec.h>

#include "DocumentConstants.h"
#include "PayloadCodec.h"

namespace hello::kit {

    namespace {

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message});
        }

        void complain(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Warning, message});
        }

        /// A non-owning view of \a s , for all read-only consumers.
        QByteArrayView viewOf(const std::string &s) {
            return QByteArrayView(s.data(), qsizetype(s.size()));
        }

        /// An owning copy of \a s , for the few consumers that require ownership.
        QByteArray bytesOf(const std::string &s) {
            return QByteArray(s.data(), qsizetype(s.size()));
        }

        std::string stdOf(QByteArrayView b) {
            return std::string(b.data(), size_t(b.size()));
        }

        /// The entry name of the control note as a map key, constructed once.
        ///
        /// A std::map lookup with a string literal constructs and discards a temporary
        /// std::string, and this lookup occurs once per note.
        const std::string &controlNoteKey() {
            static const std::string key(controlNoteEntry);
            return key;
        }

        bool isControlNote(const utau::Note &note) {
            return note.lyric == controlNoteLyric && note.userData.count(controlNoteKey()) != 0;
        }

        /// Encodes \a text with \a codec , escaping it first if the encoding cannot represent
        /// every character.
        std::string encodeFor(const TextCodec &codec, bool escaping, const QString &text) {
            return escaping ? stdOf(codec.encode(codec.escape(text))) : stdOf(codec.encode(text));
        }

        /// The payload of the control note as read.
        ///
        /// Unrecognized fields are preserved, so that a file written by a newer build retains
        /// them when saved again. The top level of \c .usth provides the same guarantee.
        QJsonObject payloadOf(const utau::Note &note) {
            const auto it = note.userData.find(controlNoteKey());
            if (it == note.userData.end()) {
                return {};
            }
            const auto decoded = PayloadCodec::decode(viewOf(it->second));
            if (!decoded) {
                return {};
            }
            return QJsonDocument::fromJson(*decoded).object();
        }

        // Decoding of a note. Every string enters as bytes and leaves as UTF-8, and no other
        // code in this file handles a std::string.
        class Reader {
        public:
            Reader(const TextCodec &codec, bool unescaping)
                : m_codec(codec), m_unescaping(unescaping) {
            }

            QString text(const std::string &bytes, bool *ok = nullptr) const {
                const auto decoded = m_codec.decode(viewOf(bytes));
                if (ok) {
                    *ok = decoded.has_value();
                }
                if (!decoded) {
                    return {};
                }
                return m_unescaping ? TextCodec::unescape(*decoded) : *decoded;
            }

        private:
            const TextCodec &m_codec;
            bool m_unescaping;
        };

        PortamentoPoint::Type joinOf(utau::Point::Type type) {
            switch (type) {
                case utau::Point::LinearJoin:
                    return PortamentoPoint::Linear;
                case utau::Point::RJoin:
                    return PortamentoPoint::R;
                case utau::Point::JJoin:
                    return PortamentoPoint::J;
                case utau::Point::SJoin:
                    break;
            }
            return PortamentoPoint::S;
        }

        utau::Point::Type joinOf(PortamentoPoint::Type type) {
            switch (type) {
                case PortamentoPoint::Linear:
                    return utau::Point::LinearJoin;
                case PortamentoPoint::R:
                    return utau::Point::RJoin;
                case PortamentoPoint::J:
                    return utau::Point::JJoin;
                case PortamentoPoint::S:
                    break;
            }
            return utau::Point::SJoin;
        }

        Note noteFrom(const utau::Note &from, const Reader &reader) {
            Note note;
            note.lyric = reader.text(from.lyric);
            note.length = from.length;
            note.noteNum = from.noteNum;

            note.intensity = from.intensity;
            note.modulation = from.modulation;
            note.velocity = from.velocity;
            note.preUtterance = from.preUttr;
            note.voiceOverlap = from.overlap;
            note.startPoint = from.stp;
            note.tempo = from.tempo;

            note.flags = reader.text(from.flags);
            note.label = reader.text(from.label);
            note.direct = reader.text(from.direct);
            note.patch = reader.text(from.patch);
            note.regions = Note::regionNamesFromUst(reader.text(from.region));
            note.regionEnds = Note::regionNamesFromUst(reader.text(from.regionEnd));

            if (from.envelope) {
                // stdutau lists the four or five anchors in time order.
                QList<EnvelopeAnchor> anchors;
                for (int i = 0; i < from.envelope->count(); ++i) {
                    const auto &anchor = from.envelope->anchors.at(size_t(i));
                    anchors.push_back({anchor.x, anchor.y});
                }
                note.envelope = Envelope::fromTimeOrder(anchors);
            }

            if (from.vibrato) {
                Vibrato vibrato;
                vibrato.length = from.vibrato->length;
                vibrato.period = from.vibrato->period;
                vibrato.amplitude = from.vibrato->amplitude;
                vibrato.attack = from.vibrato->attack;
                vibrato.release = from.vibrato->release;
                vibrato.phase = from.vibrato->phase;
                vibrato.offset = from.vibrato->offset;
                vibrato.intensity = from.vibrato->intensity;
                note.vibrato = vibrato;
            }

            // stdutau has already added up the intervals of PBW into times from the start of the
            // note, and keeps the heights in tenths of a semitone.
            for (const auto &point : from.portamento) {
                note.portamento.push_back(
                    {point.x, PortamentoPoint::centsFromTenths(point.y), joinOf(point.type)});
            }

            if (from.pbstart || !from.pitches.empty()) {
                PitchBend bend;
                bend.start = from.pbstart;
                for (const double value : from.pitches) {
                    bend.values.push_back(value);
                }
                note.pitchBend = bend;
            }

            for (const auto &[key, value] : from.userData) {
                note.userData.insert(reader.text(key), reader.text(value));
            }
            return note;
        }

        utau::Note noteTo(const Note &from, const TextCodec &codec, bool escaping) {
            const auto out = [&](const QString &text) { return encodeFor(codec, escaping, text); };

            utau::Note note;
            note.lyric = out(from.lyric);
            note.length = from.length;
            note.noteNum = from.noteNum;

            note.intensity = from.intensity;
            note.modulation = from.modulation;
            note.velocity = from.velocity;
            note.preUttr = from.preUtterance;
            note.overlap = from.voiceOverlap;
            note.stp = from.startPoint;
            note.tempo = from.tempo;

            note.flags = out(from.flags);
            note.label = out(from.label);
            note.direct = out(from.direct);
            note.patch = out(from.patch);
            note.region = out(Note::regionNamesToUst(from.regions));
            note.regionEnd = out(Note::regionNamesToUst(from.regionEnds));

            if (from.envelope) {
                // stdutau takes the anchors in time order. Without a middle anchor, index four
                // keeps its default, from which stdutau infers four anchors.
                utau::Envelope envelope;
                const auto anchors = from.envelope->anchorsInTimeOrder();
                for (qsizetype i = 0; i < anchors.size(); ++i) {
                    envelope.anchors[size_t(i)] = utau::Point(anchors[i].x, anchors[i].y);
                }
                note.envelope = envelope;
            }

            if (from.vibrato) {
                utau::Vibrato vibrato;
                vibrato.length = from.vibrato->length;
                vibrato.period = from.vibrato->period;
                vibrato.amplitude = from.vibrato->amplitude;
                vibrato.attack = from.vibrato->attack;
                vibrato.release = from.vibrato->release;
                vibrato.phase = from.vibrato->phase;
                vibrato.offset = from.vibrato->offset;
                vibrato.intensity = from.vibrato->intensity;
                note.vibrato = vibrato;
            }

            for (const auto &point : from.portamento) {
                note.portamento.emplace_back(point.x, PortamentoPoint::tenthsFromCents(point.y),
                                             joinOf(point.type));
            }

            if (from.pitchBend) {
                note.pbstart = from.pitchBend->start;
                for (const double value : from.pitchBend->values) {
                    note.pitches.push_back(value);
                }
            }

            for (auto it = from.userData.begin(); it != from.userData.end(); ++it) {
                note.userData[out(it.key())] = out(it.value());
            }
            return note;
        }

    }

    Note UstDocument::noteFromUst(const utau::Note &note, const TextCodec &codec, bool unescaping) {
        return noteFrom(note, Reader(codec, unescaping));
    }

    utau::Note UstDocument::noteToUst(const Note &note, const TextCodec &codec, bool escaping) {
        return noteTo(note, codec, escaping);
    }

    std::optional<UstDocument> UstDocument::open(const std::filesystem::path &path,
                                                 DiagnosticList &diagnostics) {
        UstDocument document;
        if (!document.m_file.load(path)) {
            fail(diagnostics, tr("This file could not be read."));
            return std::nullopt;
        }

        document.m_utf8 =
            viewOf(document.m_file.version.charset).compare("UTF-8", Qt::CaseInsensitive) == 0;

        for (const auto &note : document.m_file.notes) {
            if (!isControlNote(note)) {
                continue;
            }
            document.m_hasControlNote = true;
            const auto charset = payloadOf(note).value(QLatin1String("ustCharset")).toString();
            if (!charset.isEmpty()) {
                document.m_recorded = charset;
            }
            break;
        }
        return document;
    }

    std::optional<QString> UstDocument::recordedCharset() const {
        return m_recorded;
    }

    bool UstDocument::declaresUtf8() const {
        return m_utf8;
    }

    std::optional<QString> UstDocument::settledCharset() const {
        if (m_recorded) {
            return m_recorded;
        }
        if (m_utf8) {
            return QStringLiteral("UTF-8");
        }
        return std::nullopt;
    }

    QByteArrayView UstDocument::rawProjectName() const {
        return viewOf(m_file.settings.projectName);
    }

    QByteArrayView UstDocument::rawVoiceDir() const {
        return viewOf(m_file.settings.voiceDir);
    }

    QList<QByteArrayView> UstDocument::rawLyrics() const {
        QList<QByteArrayView> lyrics;
        for (const auto &note : m_file.notes) {
            if (!isControlNote(note) && !note.lyric.empty()) {
                lyrics.push_back(viewOf(note.lyric));
            }
        }
        return lyrics;
    }

    std::optional<Project> UstDocument::toProject(const QString &charset,
                                                  DiagnosticList &diagnostics) const {
        const TextCodec codec(charset);
        if (!codec.isValid()) {
            fail(diagnostics, tr("The encoding \"%1\" is not available.").arg(charset));
            return std::nullopt;
        }

        // Escaping applies only to files written by this program, and only where the encoding
        // could not represent every character. A UST from UTAU does not use escaping, and
        // unescaping it would remove its backslashes. The control note identifies which kind of
        // file this is.
        const Reader reader(codec, m_hasControlNote && !codec.isUtf8());
        const auto &file = m_file;

        Project project;
        bool ok = true;
        project.settings.name = reader.text(file.settings.projectName, &ok);
        if (!ok) {
            fail(diagnostics, tr("This file is not valid in the %1 encoding.").arg(codec.name()));
            return std::nullopt;
        }
        project.settings.tempo = file.settings.tempo;
        project.settings.flags = reader.text(file.settings.flags);
        project.settings.outputFile = reader.text(file.settings.outputFileName);
        project.settings.cacheDir = reader.text(file.settings.cacheDir);
        project.settings.wavtool = reader.text(file.settings.wavtoolPath);
        project.settings.resampler = reader.text(file.settings.resamplerPath);
        project.settings.mode2 = file.settings.isMode2;

        Track track;
        track.voiceDir = reader.text(file.settings.voiceDir);

        bool eaten = false;
        for (const auto &note : file.notes) {
            if (!eaten && isControlNote(note)) {
                eaten = true;
                const auto value = payloadOf(note).value(QLatin1String("timeSignature"));
                if (value.isUndefined()) {
                    continue;
                }
                if (const auto timeSignature = TimeSignature::fromJson(value.toObject())) {
                    project.settings.timeSignature = *timeSignature;
                } else {
                    complain(diagnostics, tr("The time signature of the project is not valid and "
                                             "was reset to the default."));
                }
                continue;
            }
            track.notes.push_back(noteFrom(note, reader));
        }
        project.tracks.push_back(track);
        return project;
    }

    bool UstDocument::save(const std::filesystem::path &path, DiagnosticList &diagnostics) const {
        if (!m_file.save(path)) {
            fail(diagnostics, tr("This file could not be written."));
            return false;
        }
        return true;
    }

    std::optional<UstDocument> UstDocument::fromProject(const Project &project,
                                                        const ExportOptions &options,
                                                        DiagnosticList &diagnostics) {
        if (project.tracks.size() != 1) {
            fail(diagnostics, tr("A UST contains exactly one track, but this project contains %1.")
                                  .arg(project.tracks.size()));
            return std::nullopt;
        }

        const TextCodec codec(options.charset);
        if (!codec.isValid()) {
            fail(diagnostics, tr("The encoding \"%1\" is not available.").arg(options.charset));
            return std::nullopt;
        }
        const bool escaping = !codec.isUtf8();
        const auto out = [&](const QString &text) { return encodeFor(codec, escaping, text); };

        UstDocument document;
        auto &file = document.m_file;
        document.m_utf8 = codec.isUtf8();
        document.m_recorded = codec.name();
        document.m_hasControlNote = true;

        // UST can declare only UTF-8, so any other encoding is not declared here and is recorded
        // by the control note instead.
        file.version.version = "1.2";
        if (codec.isUtf8()) {
            file.version.charset = "UTF-8";
        }

        const auto &settings = project.settings;
        file.settings.tempo = settings.tempo;
        file.settings.projectName = out(settings.name);
        file.settings.flags = out(settings.flags);
        file.settings.outputFileName = out(Project::savedPathText(settings.outputFile));
        file.settings.cacheDir = out(Project::savedPathText(
            options.file.empty() ? settings.cacheDir : Project::cacheDirTextOf(options.file)));
        file.settings.voiceDir = out(Project::savedPathText(project.tracks.first().voiceDir));
        file.settings.isMode2 = settings.mode2;

        // The synth tools specified by the project are written unchanged. If it specifies none, the
        // locally configured synth tools are written instead, because UTAU cannot render a UST
        // without synth tools.
        const QString wavtool = settings.wavtool.isEmpty() ? options.wavtool : settings.wavtool;
        const QString resampler =
            settings.resampler.isEmpty() ? options.resampler : settings.resampler;
        file.settings.wavtoolPath = out(Project::savedPathText(wavtool));
        file.settings.resamplerPath = out(Project::savedPathText(resampler));
        if (wavtool.isEmpty() || resampler.isEmpty()) {
            complain(diagnostics, tr("This UST specifies no synth tool, so UTAU cannot "
                                     "render it until a synth tool is configured."));
        }

        QJsonObject payload;
        payload.insert(QLatin1String("version"), controlNotePayloadVersion);
        payload.insert(QLatin1String("ustCharset"), codec.name());
        payload.insert(QLatin1String("timeSignature"), settings.timeSignature.toJson());

        utau::Note control;
        control.lyric = controlNoteLyric;
        control.length = controlNoteLength;
        control.noteNum = controlNoteNoteNum;
        control.userData[controlNoteKey()] =
            stdOf(PayloadCodec::encode(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
        file.notes.push_back(control);

        for (const auto &note : project.tracks.first().notes) {
            file.notes.push_back(noteTo(note, codec, escaping));
        }
        return document;
    }

}
