#include "UstNoteEntries.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QSet>

#include <stdutau/ustfile.h>
#include <stdutau/utaconst.h>

#include <hellokit/Support/TextCodec.h>

#include "UstDocument.h"

namespace hello::kit {

    namespace {

        QString keyOf(const char *name) {
            return QString::fromLatin1(name);
        }

        // The keys of the built-in entries. The names of the Mode1 pitch, which UTAU and the
        // plugins write in three spellings, count as one key.
        const QSet<QString> &builtInKeys() {
            static const QSet<QString> keys = {
                keyOf(utau::KEY_NAME_ENVELOPE),     keyOf(utau::KEY_NAME_VBR),
                keyOf(utau::KEY_NAME_PBS),          keyOf(utau::KEY_NAME_PBW),
                keyOf(utau::KEY_NAME_PBY),          keyOf(utau::KEY_NAME_PBM),
                keyOf(utau::KEY_NAME_PB_TYPE),      keyOf(utau::KEY_NAME_PB_START),
                keyOf(utau::KEY_NAME_PITCH_BEND),   keyOf(utau::KEY_NAME_PITCHES),
                keyOf(utau::KEY_NAME_PICHES),       keyOf(utau::KEY_NAME_LABEL),
                keyOf(utau::KEY_NAME_DIRECT),       keyOf(utau::KEY_NAME_PATCH),
                keyOf(utau::KEY_NAME_REGION_START), keyOf(utau::KEY_NAME_REGION_END),
            };
            return keys;
        }

        // The keys of the entries that UTAU reads, which the user data cannot hold
        const QSet<QString> &readKeys() {
            static const QSet<QString> keys = [] {
                auto all = builtInKeys();
                for (const auto name : {
                         utau::KEY_NAME_LYRIC,
                         utau::KEY_NAME_NOTE_NUM,
                         utau::KEY_NAME_LENGTH,
                         utau::KEY_NAME_FLAGS,
                         utau::KEY_NAME_INTENSITY,
                         utau::KEY_NAME_MODULATION,
                         utau::KEY_NAME_MODURATION,
                         utau::KEY_NAME_PRE_UTTERANCE,
                         utau::KEY_NAME_VOICE_OVERLAP,
                         utau::KEY_NAME_VELOCITY,
                         utau::KEY_NAME_START_POINT,
                         utau::KEY_NAME_TEMPO,
                         utau::KEY_NAME_PRE_UTTERANCE_READONLY,
                         utau::KEY_NAME_VOICE_OVERLAP_READONLY,
                         utau::KEY_NAME_START_POINT_READONLY,
                         utau::KEY_NAME_FILENAME_READONLY,
                         utau::KEY_NAME_ALIAS_READONLY,
                         utau::KEY_NAME_CACHE_READONLY,
                     }) {
                    all.insert(keyOf(name));
                }
                return all;
            }();
            return keys;
        }

        // Returns the key under which the entry of key is compared
        QString canonicalKey(const QString &key) {
            if (key == keyOf(utau::KEY_NAME_PITCHES) || key == keyOf(utau::KEY_NAME_PICHES)) {
                return keyOf(utau::KEY_NAME_PITCH_BEND);
            }
            return key;
        }

        // Returns the parts of value between , and ;, without the trailing parts that are zero
        // or %
        QStringList partsOf(const QString &value) {
            static const QRegularExpression separator(QStringLiteral("[,;]"));
            auto parts = value.split(separator);
            while (!parts.isEmpty()) {
                const auto last = parts.last().trimmed();
                bool number = false;
                const double zero = last.toDouble(&number);
                if (last != QLatin1String("%") && !(number && zero == 0)) {
                    break;
                }
                parts.removeLast();
            }
            return parts;
        }

        // Returns whether the value that a line gives and the value that a UST writes are the
        // same, part by part
        bool sameValue(const QString &given, const QString &written) {
            const auto a = partsOf(given);
            const auto b = partsOf(written);
            if (a.size() != b.size()) {
                return false;
            }
            for (qsizetype i = 0; i < a.size(); ++i) {
                if (a[i] == b[i]) {
                    continue;
                }
                bool numberA = false;
                bool numberB = false;
                const double x = a[i].toDouble(&numberA);
                const double y = b[i].toDouble(&numberB);
                if (!numberA || !numberB || x != y) {
                    return false;
                }
            }
            return true;
        }

        void addError(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message, std::nullopt});
        }

        const TextCodec &utf8() {
            static const TextCodec codec(QStringLiteral("UTF-8"));
            return codec;
        }

    }

    QStringList UstNoteEntries::builtInLinesOf(const Note &note) {
        utau::UstFile file;
        file.notes.push_back(UstDocument::noteToUst(note, utf8(), false));
        const auto text = QString::fromUtf8(file.write());
        QStringList lines;
        bool inNote = false;
        for (auto line : text.split(QLatin1Char('\n'))) {
            line.remove(QLatin1Char('\r'));
            if (line.startsWith(QLatin1String("[#"))) {
                inNote = line == QLatin1String("[#0000]");
                continue;
            }
            const auto eq = line.indexOf(QLatin1Char('='));
            if (inNote && eq > 0 && builtInKeys().contains(line.left(eq))) {
                lines.push_back(line);
            }
        }
        return lines;
    }

    QStringList UstNoteEntries::userDataLinesOf(const Note &note) {
        QStringList lines;
        for (auto it = note.userData.cbegin(); it != note.userData.cend(); ++it) {
            lines.push_back(it.key() + QLatin1Char('=') + it.value());
        }
        return lines;
    }

    std::optional<Note> UstNoteEntries::withBuiltInLines(Note note, const QStringList &lines,
                                                         DiagnosticList &diagnostics) {
        const auto failures = diagnostics.size();
        // The key and the value of each line that is not empty, by its line number
        QList<std::tuple<int, QString, QString>> entries;
        QSet<QString> seen;
        for (qsizetype i = 0; i < lines.size(); ++i) {
            const int number = int(i + 1);
            const auto &line = lines[i];
            if (line.trimmed().isEmpty()) {
                continue;
            }
            const auto eq = line.indexOf(QLatin1Char('='));
            if (eq < 0) {
                addError(diagnostics, tr("Line %1 is not of the form key=value.").arg(number));
                continue;
            }
            const auto key = line.left(eq);
            if (!builtInKeys().contains(key)) {
                addError(diagnostics, tr("Line %1: %2 is not a built-in entry. An entry of another "
                                         "key belongs to the user data.")
                                          .arg(number)
                                          .arg(key));
                continue;
            }
            if (seen.contains(canonicalKey(key))) {
                addError(diagnostics, tr("Line %1 repeats the entry %2.").arg(number).arg(key));
                continue;
            }
            seen.insert(canonicalKey(key));
            entries.push_back({number, key, line.mid(eq + 1)});
        }
        if (diagnostics.size() > failures) {
            return std::nullopt;
        }

        // The lines are read as the entries of a note of a UST, which needs a positive length.
        QString text = QStringLiteral("[#0000]\nLength=480\nLyric=a\nNoteNum=60\n");
        for (const auto &[number, key, value] : std::as_const(entries)) {
            text += key + QLatin1Char('=') + value + QLatin1Char('\n');
        }
        utau::UstFile file;
        file.read(text.toUtf8().toStdString());
        const auto read = file.notes.empty()
                              ? Note()
                              : UstDocument::noteFromUst(file.notes.front(), utf8(), false);
        note.envelope = read.envelope;
        note.vibrato = read.vibrato;
        note.portamento = read.portamento;
        note.pitchBend = read.pitchBend;
        note.label = read.label;
        note.direct = read.direct;
        note.patch = read.patch;
        note.regions = read.regions;
        note.regionEnds = read.regionEnds;

        QMap<QString, QString> written;
        for (const auto &line : builtInLinesOf(note)) {
            const auto eq = line.indexOf(QLatin1Char('='));
            written.insert(canonicalKey(line.left(eq)), line.mid(eq + 1));
        }
        for (const auto &[number, key, value] : std::as_const(entries)) {
            // An entry with an empty value, such as $direct=, is left out, as a UST does.
            const auto it = written.constFind(canonicalKey(key));
            if (it == written.cend() ? !value.trimmed().isEmpty() : !sameValue(value, *it)) {
                addError(diagnostics,
                         tr("Line %1: the value of %2 is not written as a UST writes it.")
                             .arg(number)
                             .arg(key));
            }
        }
        if (diagnostics.size() > failures) {
            return std::nullopt;
        }
        return note;
    }

    std::optional<QMap<QString, QString>> UstNoteEntries::userDataOf(const QStringList &lines,
                                                                     DiagnosticList &diagnostics) {
        const auto failures = diagnostics.size();
        QMap<QString, QString> userData;
        for (qsizetype i = 0; i < lines.size(); ++i) {
            const int number = int(i + 1);
            const auto &line = lines[i];
            if (line.trimmed().isEmpty()) {
                continue;
            }
            const auto eq = line.indexOf(QLatin1Char('='));
            if (eq < 0) {
                addError(diagnostics, tr("Line %1 is not of the form key=value.").arg(number));
                continue;
            }
            const auto key = line.left(eq);
            static const QRegularExpression space(QStringLiteral("\\s"));
            if (key.isEmpty() || key.contains(space)) {
                addError(diagnostics,
                         tr("Line %1: the key is empty or has a space.").arg(number));
                continue;
            }
            if (readKeys().contains(key)) {
                addError(
                    diagnostics,
                    tr("Line %1: %2 is an entry that UTAU reads, which belongs to the built-in "
                       "entries or to the note properties.")
                        .arg(number)
                        .arg(key));
                continue;
            }
            if (userData.contains(key)) {
                addError(diagnostics, tr("Line %1 repeats the entry %2.").arg(number).arg(key));
                continue;
            }
            userData.insert(key, line.mid(eq + 1));
        }
        if (diagnostics.size() > failures) {
            return std::nullopt;
        }
        return userData;
    }

    DiagnosticList UstNoteEntries::utauChangesOf(const QMap<QString, QString> &userData) {
        DiagnosticList warnings;
        const auto warn = [&warnings](const QString &message) {
            warnings.push_back({DiagnosticSeverity::Warning, message, std::nullopt});
        };
        for (auto it = userData.cbegin(); it != userData.cend(); ++it) {
            const auto &key = it.key();
            const auto &value = it.value();
            if (!key.startsWith(QLatin1Char('$'))) {
                warn(tr("UTAU drops the entry %1 when it saves the file, because the key does not "
                        "begin with $.")
                         .arg(key));
                continue;
            }
            if (value.isEmpty()) {
                warn(tr("UTAU drops the entry %1 when it saves the file, because the value is "
                        "empty.")
                         .arg(key));
            } else if (value.contains(QLatin1Char('\t'))) {
                warn(
                    tr("UTAU cuts the value of %1 at the tab when it saves the file, and drops the "
                       "entries of the note after it.")
                        .arg(key));
            } else if (value.contains(QLatin1Char('='))) {
                warn(tr("UTAU cuts the value of %1 at the first = when it saves the file.")
                         .arg(key));
            } else if (value.contains(QLatin1Char(' '))) {
                warn(tr("UTAU replaces the spaces in the value of %1 with commas when it saves the "
                        "file.")
                         .arg(key));
            }
        }
        return warnings;
    }

}
