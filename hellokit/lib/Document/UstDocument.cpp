#include "UstDocument.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <stdutau/ustfile.h>

#include <hellokit/Support/TextCodec.h>

#include <hellokit/Document/DocumentConstants.h>
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
                : _codec(codec), _unescaping(unescaping) {
            }

            QString text(const std::string &bytes, bool *ok = nullptr) const {
                const auto decoded = _codec.decode(viewOf(bytes));
                if (ok) {
                    *ok = decoded.has_value();
                }
                if (!decoded) {
                    return {};
                }
                return _unescaping ? TextCodec::unescape(*decoded) : *decoded;
            }

        private:
            const TextCodec &_codec;
            bool _unescaping;
        };

        PortamentoType joinOf(utau::Point::Type type) {
            switch (type) {
                case utau::Point::LinearJoin:
                    return PortamentoType::Linear;
                case utau::Point::RJoin:
                    return PortamentoType::R;
                case utau::Point::JJoin:
                    return PortamentoType::J;
                case utau::Point::SJoin:
                    break;
            }
            return PortamentoType::S;
        }

        utau::Point::Type joinOf(PortamentoType type) {
            switch (type) {
                case PortamentoType::Linear:
                    return utau::Point::LinearJoin;
                case PortamentoType::R:
                    return utau::Point::RJoin;
                case PortamentoType::J:
                    return utau::Point::JJoin;
                case PortamentoType::S:
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
            note.region = reader.text(from.region);
            note.regionEnd = reader.text(from.regionEnd);

            if (from.envelope) {
                Envelope envelope;
                for (int i = 0; i < from.envelope->count(); ++i) {
                    const auto &anchor = from.envelope->anchors.at(size_t(i));
                    envelope.anchors.push_back({anchor.x, anchor.y});
                }
                note.envelope = envelope;
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

            for (const auto &point : from.portamento) {
                note.portamento.push_back({point.x, point.y, joinOf(point.type)});
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
            note.region = out(from.region);
            note.regionEnd = out(from.regionEnd);

            if (from.envelope) {
                utau::Envelope envelope;
                for (qsizetype i = 0; i < from.envelope->anchors.size() && i < 5; ++i) {
                    const auto &anchor = from.envelope->anchors.at(i);
                    envelope.anchors[size_t(i)] = utau::Point(anchor.x, anchor.y);
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
                note.portamento.emplace_back(point.x, point.y, joinOf(point.type));
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
        file.settings.outputFileName = out(settings.outputFile);
        file.settings.cacheDir = out(settings.cacheDir);
        file.settings.voiceDir = out(project.tracks.first().voiceDir);
        file.settings.isMode2 = settings.mode2;

        // The engines specified by the project are written unchanged. If it specifies none, the
        // locally configured engines are written instead, because UTAU cannot render a UST
        // without engines.
        const QString wavtool = settings.wavtool.isEmpty() ? options.wavtool : settings.wavtool;
        const QString resampler =
            settings.resampler.isEmpty() ? options.resampler : settings.resampler;
        file.settings.wavtoolPath = out(wavtool);
        file.settings.resamplerPath = out(resampler);
        if (wavtool.isEmpty() || resampler.isEmpty()) {
            complain(diagnostics, tr("This UST specifies no rendering engine, so UTAU cannot "
                                     "render it until an engine is configured."));
        }

        QJsonObject payload;
        payload.insert(QLatin1String("version"), controlNotePayloadVersion);
        payload.insert(QLatin1String("ustCharset"), codec.name());

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
