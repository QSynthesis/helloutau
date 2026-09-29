#include "ScriptExport.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>

#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <stdcorelib/console.h>
#include <stdcorelib/path.h>

#include <hellokit/Synth/private/ShellSyntax_p.h>

using namespace hello::kit;
namespace fs = std::filesystem;

namespace {

    void error(const QString &message) {
        stdc::console::u8fprintf(stderr, "error: %s\n", message.toStdString().c_str());
    }

    QString textOf(const fs::path &path) {
        return QString::fromStdU16String(path.u16string());
    }

    fs::path pathOf(const QString &text) {
        return fs::path(text.toStdU16String());
    }

    // One engine call, or the joining of the two files of the wavtool, as the manifest lists it
    struct Step {
        int index = 0;
        QString kind;
        std::optional<int> note;
        QStringList arguments;
        QStringList outputs;
        QStringList snapshots;
    };

    // The steps of a plan and the copies after them, with every path as the script writes it
    class Layout {
    public:
        Layout(const ScriptExport &options, const SynthPlan &plan) : m_options(options) {
            m_voice = QDir::fromNativeSeparators(textOf(options.voice));
            while (m_voice.endsWith(QLatin1Char('/'))) {
                m_voice.chop(1);
            }
            output = written(textOf(plan.outputFile()));
            cache = written(textOf(plan.cacheDirectory()));
            scriptDirectory = written(options.scriptDirectory);
            snapshotDirectory = options.snapshotDirectory.isEmpty()
                                    ? QString()
                                    : written(options.snapshotDirectory);
            voice = options.voiceAs.isEmpty() ? written(m_voice) : written(options.voiceAs);
            log = join(scriptDirectory, QStringLiteral("exitcodes.log"));
            for (const auto &part : options.resamplerCommand) {
                resamplerCommand.push_back(written(part));
            }
            for (const auto &part : options.wavtoolCommand) {
                wavtoolCommand.push_back(written(part));
            }

            int lastSung = -1;
            for (qsizetype i = 0; i < plan.steps().size(); ++i) {
                if (!plan.steps()[i].silent) {
                    lastSung = int(i);
                }
            }

            std::set<QString> sampleDirectories;
            for (qsizetype i = 0; i < plan.steps().size(); ++i) {
                const auto &from = plan.steps()[i];
                if (!from.silent) {
                    Step step;
                    step.kind = QStringLiteral("resampler");
                    step.note = from.noteIndex;
                    step.arguments = from.resamplerArguments;
                    // The sample and the fragment
                    step.arguments[0] = written(step.arguments[0]);
                    step.arguments[1] = written(step.arguments[1]);
                    step.outputs = {step.arguments[1],
                                    step.arguments[1] + QStringLiteral(".llsm.tmp")};
                    add(step);
                    sampleDirectories.insert(
                        QDir::fromNativeSeparators(textOf(from.sample.parent_path())));
                }
                Step step;
                step.kind = QStringLiteral("wavtool");
                step.note = from.noteIndex;
                step.arguments = from.wavtoolArguments;
                // The track and the fragment
                step.arguments[0] = written(step.arguments[0]);
                step.arguments[1] = written(step.arguments[1]);
                if (options.lastNote && i == lastSung) {
                    step.arguments.push_back(QStringLiteral("LAST_NOTE"));
                }
                step.outputs = {output, output + QStringLiteral(".whd"),
                                output + QStringLiteral(".dat")};
                add(step);
            }

            Step concatenation;
            concatenation.kind = QStringLiteral("concatenate");
            concatenation.outputs = {output};
            add(concatenation);

            // What remains once everything has run: the track and the database of moresampler
            // in each directory of the samples
            if (!snapshotDirectory.isEmpty()) {
                finalCopies.push_back(
                    {output, join(snapshotDirectory, QStringLiteral("final_") + nameOf(output))});
                for (const auto &directory : sampleDirectories) {
                    QString relative = directory.mid(m_voice.size());
                    relative.replace(QLatin1Char('/'), QLatin1Char('_'));
                    finalCopies.push_back(
                        {join(written(directory), QStringLiteral("desc.mrq")),
                         join(snapshotDirectory,
                              QStringLiteral("final_desc") + relative + QStringLiteral(".mrq"))});
                }
            }
        }

        QString output;
        QString cache;
        QString scriptDirectory;
        QString snapshotDirectory;
        QString voice;
        QString log;
        // The programs, with the separators of the target
        QStringList resamplerCommand;
        QStringList wavtoolCommand;
        QList<Step> steps;
        // A file and its copy after the last step
        QList<std::pair<QString, QString>> finalCopies;

        QString join(const QString &directory, const QString &name) const {
            return directory + separator() + name;
        }

        static QString nameOf(const QString &path) {
            const auto slash =
                std::max(path.lastIndexOf(QLatin1Char('/')), path.lastIndexOf(QLatin1Char('\\')));
            return path.mid(slash + 1);
        }

    private:
        const ScriptExport &m_options;
        QString m_voice;

        QChar separator() const {
            return m_options.target == ScriptExport::Windows ? QLatin1Char('\\') : QLatin1Char('/');
        }

        // A path as the script writes it: under voiceAs if it is under the voice bank read, and
        // with the separators of the target
        QString written(const QString &path) const {
            QString text = QDir::fromNativeSeparators(path);
            if (!m_options.voiceAs.isEmpty() &&
                (text.compare(m_voice, Qt::CaseInsensitive) == 0 ||
                 text.startsWith(m_voice + QLatin1Char('/'), Qt::CaseInsensitive))) {
                text = QDir::fromNativeSeparators(m_options.voiceAs) + text.mid(m_voice.size());
            }
            text.replace(QLatin1Char('/'), separator());
            text.replace(QLatin1Char('\\'), separator());
            return text;
        }

        void add(Step step) {
            step.index = int(steps.size()) + 1;
            if (!snapshotDirectory.isEmpty()) {
                for (const auto &output : std::as_const(step.outputs)) {
                    step.snapshots.push_back(
                        join(snapshotDirectory,
                             QStringLiteral("%1_").arg(step.index, 4, 10, QLatin1Char('0')) +
                                 nameOf(output)));
                }
            }
            steps.push_back(step);
        }
    };

    // The script of a layout for one shell. The calls are written one by one rather than
    // through temp_helper.bat, so that each is followed by its exit code and its copies. The
    // order of the calls, their arguments, the reuse of an existing fragment and the joining
    // of the files are those of UTAU; the fragment is set as UTAU sets it, since moresampler
    // reads it from temp.bat.
    class ScriptWriter {
    public:
        ScriptWriter(const Layout &layout, const ShellSyntax &syntax, const ScriptExport &options)
            : m_layout(layout), m_syntax(syntax), m_options(options) {
        }

        std::optional<QString> text() {
            const bool batch = m_syntax.isBatch();
            const auto &l = m_layout;
            if (batch) {
                line(QStringLiteral("@rem hellokit render comparison"));
                line(QStringLiteral("@cd /d ") + quoted(l.scriptDirectory));
                assign("log", l.log);
                line(QStringLiteral("@type nul >") + logFile());
            } else {
                line(QStringLiteral("#!/bin/sh"));
                line(QStringLiteral("# hellokit render comparison"));
                line(QStringLiteral("cd ") + quoted(l.scriptDirectory) +
                     QStringLiteral(" || exit 1"));
                line(QStringLiteral("log=") + quoted(l.log));
                line(QStringLiteral(": > \"$log\""));
            }
            for (const auto &file :
                 {l.output, l.output + QStringLiteral(".whd"), l.output + QStringLiteral(".dat")}) {
                line(batch ? QStringLiteral("@del ") + quoted(file) + QStringLiteral(" 2>nul")
                           : QStringLiteral("rm -f ") + quoted(file));
            }
            // The folder of the track as well, which UTAU finds existing: moresampler does not
            // create it, and waits for a lock on the track indefinitely without it.
            const auto slash = std::max(l.output.lastIndexOf(QLatin1Char('/')),
                                        l.output.lastIndexOf(QLatin1Char('\\')));
            for (const auto &directory :
                 {l.output.left(std::max<qsizetype>(slash, 0)), l.cache, l.snapshotDirectory}) {
                if (!directory.isEmpty()) {
                    line(batch ? QStringLiteral("@mkdir ") + quoted(directory) +
                                     QStringLiteral(" 2>nul")
                               : QStringLiteral("mkdir -p ") + quoted(directory));
                }
            }

            const auto total = std::count_if(l.steps.begin(), l.steps.end(), [](const Step &s) {
                return s.kind == QLatin1String("wavtool");
            });
            int note = 0;
            for (const auto &step : l.steps) {
                if (step.kind == QLatin1String("resampler")) {
                    resampler(step);
                } else if (step.kind == QLatin1String("wavtool")) {
                    ++note;
                    wavtool(step, note, int(total));
                } else {
                    concatenate(step);
                }
            }
            for (const auto &[file, copy] : l.finalCopies) {
                this->copy(file, copy);
            }
            if (!m_ok) {
                return std::nullopt;
            }
            return m_text;
        }

    private:
        const Layout &m_layout;
        const ShellSyntax &m_syntax;
        const ScriptExport &m_options;
        QString m_text;
        bool m_ok = true;

        void line(const QString &text) {
            m_text += text;
            m_text += QLatin1String(m_syntax.lineEnd());
        }

        QString checked(const QString &value) {
            if (!isWritable(value)) {
                error(QStringLiteral("\"%1\" contains a quotation mark or a line break, which "
                                     "cannot be written into a script.")
                          .arg(value));
                m_ok = false;
            }
            return value;
        }

        QString quoted(const QString &value) {
            return m_syntax.quoted(checked(value));
        }

        void assign(const char *name, const QString &value) {
            if (const auto written = m_syntax.assign(name, checked(value))) {
                line(*written);
            }
        }

        QString logFile() const {
            return m_syntax.isBatch() ? QStringLiteral("\"%log%\"") : QStringLiteral("\"$log\"");
        }

        QString command(const QStringList &program, const QStringList &arguments) {
            QStringList parts;
            for (const auto &part : program + arguments) {
                parts.push_back(m_syntax.argument(checked(part)));
            }
            return QLatin1String(m_syntax.quiet()) + parts.join(QLatin1Char(' '));
        }

        // The exit code of the call before, or a word in its place
        void record(const Step &step, const QString &what) {
            const auto entry = QStringLiteral("%1 ").arg(step.index, 4, 10, QLatin1Char('0')) +
                               step.kind + QLatin1Char(' ');
            if (m_syntax.isBatch()) {
                // The redirection first, or the digit before > would name a handle.
                line(QStringLiteral("@>>") + logFile() + QStringLiteral(" echo ") + entry +
                     (what.isEmpty() ? QStringLiteral("%errorlevel%") : what));
            } else {
                line(QStringLiteral("echo \"") + entry +
                     (what.isEmpty() ? QStringLiteral("$?") : what) + QStringLiteral("\" >> ") +
                     logFile());
            }
        }

        void copy(const QString &file, const QString &copy) {
            if (m_syntax.isBatch()) {
                line(QStringLiteral("@if exist ") + quoted(file) + QStringLiteral(" copy /Y /B ") +
                     quoted(file) + QLatin1Char(' ') + quoted(copy) + QStringLiteral(" >nul"));
            } else {
                line(QStringLiteral("if [ -f ") + quoted(file) + QStringLiteral(" ]; then cp -f ") +
                     quoted(file) + QLatin1Char(' ') + quoted(copy) + QStringLiteral("; fi"));
            }
        }

        void copies(const Step &step) {
            for (qsizetype i = 0; i < step.snapshots.size(); ++i) {
                copy(step.outputs[i], step.snapshots[i]);
            }
        }

        // An existing fragment is reused, as the helper of UTAU does.
        void resampler(const Step &step) {
            const auto fragment = step.arguments[1];
            const auto label = QString::number(step.index);
            if (m_syntax.isBatch()) {
                if (const auto set = m_syntax.assignUnquoted("temp", checked(fragment))) {
                    line(*set);
                }
                line(QStringLiteral("@if exist ") + quoted(fragment) + QStringLiteral(" goto S") +
                     label);
                line(command(m_layout.resamplerCommand, step.arguments));
                record(step, {});
                line(QStringLiteral("@goto R") + label);
                line(QStringLiteral(":S") + label);
                record(step, QStringLiteral("skipped"));
                line(QStringLiteral(":R") + label);
            } else {
                line(QStringLiteral("if [ -f ") + quoted(fragment) + QStringLiteral(" ]; then"));
                line(QStringLiteral("\t") + QStringLiteral("echo \"") +
                     QStringLiteral("%1 resampler skipped")
                         .arg(step.index, 4, 10, QLatin1Char('0')) +
                     QStringLiteral("\" >> ") + logFile());
                line(QStringLiteral("else"));
                line(QStringLiteral("\t") + command(m_layout.resamplerCommand, step.arguments));
                line(QStringLiteral("\t") + QStringLiteral("echo \"") +
                     QStringLiteral("%1 resampler $?").arg(step.index, 4, 10, QLatin1Char('0')) +
                     QStringLiteral("\" >> ") + logFile());
                line(QStringLiteral("fi"));
            }
            copies(step);
        }

        void wavtool(const Step &step, int note, int total) {
            if (m_syntax.isBatch()) {
                line(QStringLiteral("@echo (%1/%2)").arg(note).arg(total));
            } else {
                line(QStringLiteral("echo '(%1/%2)'").arg(note).arg(total));
            }
            line(command(m_layout.wavtoolCommand, step.arguments));
            record(step, {});
            copies(step);
        }

        // The header and the samples the wavtool wrote, joined into the track, as UTAU does
        void concatenate(const Step &step) {
            const auto &out = m_layout.output;
            const auto whd = out + QStringLiteral(".whd");
            const auto dat = out + QStringLiteral(".dat");
            const auto label = QString::number(step.index);
            if (m_syntax.isBatch()) {
                line(QStringLiteral("@if not exist ") + quoted(whd) + QStringLiteral(" goto C") +
                     label);
                line(QStringLiteral("@if not exist ") + quoted(dat) + QStringLiteral(" goto C") +
                     label);
                line(QStringLiteral("@copy /Y ") + quoted(whd) + QStringLiteral(" /B + ") +
                     quoted(dat) + QStringLiteral(" /B ") + quoted(out) + QStringLiteral(" >nul"));
                record(step, {});
                line(QStringLiteral("@del ") + quoted(whd));
                line(QStringLiteral("@del ") + quoted(dat));
                line(QStringLiteral("@goto D") + label);
                line(QStringLiteral(":C") + label);
                record(step, QStringLiteral("skipped"));
                line(QStringLiteral(":D") + label);
            } else {
                line(QStringLiteral("if [ -f ") + quoted(whd) + QStringLiteral(" ] && [ -f ") +
                     quoted(dat) + QStringLiteral(" ]; then"));
                line(QStringLiteral("\tcat ") + quoted(whd) + QLatin1Char(' ') + quoted(dat) +
                     QStringLiteral(" > ") + quoted(out));
                line(QStringLiteral("\techo \"") +
                     QStringLiteral("%1 concatenate $?").arg(step.index, 4, 10, QLatin1Char('0')) +
                     QStringLiteral("\" >> ") + logFile());
                line(QStringLiteral("\trm -f ") + quoted(whd) + QLatin1Char(' ') + quoted(dat));
                line(QStringLiteral("else"));
                line(QStringLiteral("\techo \"") +
                     QStringLiteral("%1 concatenate skipped")
                         .arg(step.index, 4, 10, QLatin1Char('0')) +
                     QStringLiteral("\" >> ") + logFile());
                line(QStringLiteral("fi"));
            }
            copies(step);
        }
    };

    QJsonArray arrayOf(const QStringList &items) {
        return QJsonArray::fromStringList(items);
    }

    QJsonObject manifestOf(const ScriptExport &options, const Layout &layout) {
        QJsonArray steps;
        for (const auto &step : layout.steps) {
            steps.append(QJsonObject{
                {QStringLiteral("index"),     step.index                                       },
                {QStringLiteral("kind"),      step.kind                                        },
                {QStringLiteral("note"),      step.note ? QJsonValue(*step.note) : QJsonValue()},
                {QStringLiteral("arguments"), arrayOf(step.arguments)                          },
                {QStringLiteral("outputs"),   arrayOf(step.outputs)                            },
                {QStringLiteral("snapshots"), arrayOf(step.snapshots)                          },
            });
        }
        QJsonArray finals;
        for (const auto &[file, copy] : layout.finalCopies) {
            finals.append(QJsonObject{
                {QStringLiteral("source"),   file},
                {QStringLiteral("snapshot"), copy},
            });
        }
        const auto orNull = [](const QString &text) {
            return text.isEmpty() ? QJsonValue() : QJsonValue(text);
        };
        return QJsonObject{
            {QStringLiteral("target"),            options.target == ScriptExport::Windows
                                           ? QStringLiteral("windows")
                                           : QStringLiteral("linux")},
            {QStringLiteral("project"),           options.project                                         },
            {QStringLiteral("voice"),             layout.voice                                            },
            {QStringLiteral("cache"),             layout.cache                                            },
            {QStringLiteral("output"),            layout.output                                           },
            {QStringLiteral("scriptDirectory"),   layout.scriptDirectory                                  },
            {QStringLiteral("snapshotDirectory"), orNull(layout.snapshotDirectory)                        },
            {QStringLiteral("log"),               layout.log                                              },
            {QStringLiteral("resamplerCommand"),  arrayOf(layout.resamplerCommand)                        },
            {QStringLiteral("wavtoolCommand"),    arrayOf(layout.wavtoolCommand)                          },
            {QStringLiteral("lastNote"),          options.lastNote                                        },
            {QStringLiteral("steps"),             steps                                                   },
            {QStringLiteral("finalSnapshots"),    finals                                                  },
        };
    }

    bool put(const fs::path &path, const QByteArray &bytes) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(bytes.constData(), bytes.size());
        if (!out) {
            error(QStringLiteral("%1 could not be written.").arg(textOf(path)));
            return false;
        }
        stdc::u8printf("wrote %s\n", stdc::path::to_utf8(path).c_str());
        return true;
    }

}

int ScriptExport::write(const SynthPlan &plan) const {
    if (resamplerCommand.isEmpty() || wavtoolCommand.isEmpty()) {
        error(QStringLiteral("--resampler-command and --wavtool-command are required."));
        return 1;
    }
    const Layout layout(*this, plan);

    const auto batchSyntax =
        ShellSyntax(ClassicSynthRunner::ScriptShell::Batch, ClassicSynthRunner::Quoting::Escaped);
    const auto shellSyntax =
        ShellSyntax(ClassicSynthRunner::ScriptShell::Posix, ClassicSynthRunner::Quoting::Escaped);
    const auto batch = ScriptWriter(layout, batchSyntax, *this).text();
    if (!batch) {
        return 1;
    }

    const auto directory = emitDirectory.empty() ? pathOf(scriptDirectory) : emitDirectory;
    std::error_code ignored;
    fs::create_directories(directory, ignored);

    if (target == Windows) {
        // The ANSI code page, as ClassicSynthRunner writes temp.bat, in which the command
        // processor reads it
        const auto bytes = batch->toLocal8Bit();
        if (QString::fromLocal8Bit(bytes) != *batch) {
            error(QStringLiteral("A path or an argument cannot be written in the ANSI code page."));
            return 1;
        }
        if (!put(directory / "temp.bat", bytes)) {
            return 1;
        }
    } else {
        const auto shell = ScriptWriter(layout, shellSyntax, *this).text();
        if (!shell) {
            return 1;
        }
        // temp.bat is read by moresampler and never executed, in UTF-8 as the script.
        if (!put(directory / "temp.sh", shell->toUtf8()) ||
            !put(directory / "temp.bat", batch->toUtf8())) {
            return 1;
        }
    }
    const auto manifest = QJsonDocument(manifestOf(*this, layout)).toJson(QJsonDocument::Indented);
    return put(directory / "manifest.json", manifest) ? 0 : 1;
}

namespace {

    std::optional<QJsonObject> readManifest(const fs::path &path) {
        std::ifstream in(path, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
        QJsonParseError parsed;
        const auto document =
            QJsonDocument::fromJson(QByteArray(bytes.data(), qsizetype(bytes.size())), &parsed);
        if (!in.good() && !in.eof()) {
            error(QStringLiteral("%1 could not be read.").arg(textOf(path)));
            return std::nullopt;
        }
        if (!document.isObject()) {
            error(
                QStringLiteral("%1 is not a manifest: %2").arg(textOf(path), parsed.errorString()));
            return std::nullopt;
        }
        return document.object();
    }

    // Every string of value with the roots of the manifest replaced by placeholders and the
    // separators made forward slashes
    QJsonValue stripped(const QJsonValue &value, const QList<std::pair<QString, QString>> &roots) {
        if (value.isString()) {
            QString text = QDir::fromNativeSeparators(value.toString());
            for (const auto &[root, placeholder] : roots) {
                if (text.startsWith(root)) {
                    text = placeholder + text.mid(root.size());
                    break;
                }
            }
            return text;
        }
        if (value.isArray()) {
            QJsonArray out;
            for (const auto &item : value.toArray()) {
                out.append(stripped(item, roots));
            }
            return out;
        }
        if (value.isObject()) {
            QJsonObject out;
            const auto object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it) {
                out.insert(it.key(), stripped(it.value(), roots));
            }
            return out;
        }
        return value;
    }

    QJsonObject comparable(const QJsonObject &manifest) {
        // The longer roots first, so that one inside another is replaced as itself
        QList<std::pair<QString, QString>> roots;
        for (const char *key :
             {"output", "log", "voice", "cache", "scriptDirectory", "snapshotDirectory"}) {
            const auto root =
                QDir::fromNativeSeparators(manifest.value(QLatin1String(key)).toString());
            if (!root.isEmpty()) {
                roots.push_back({root, QLatin1Char('<') + QLatin1String(key) + QLatin1Char('>')});
            }
        }
        std::sort(roots.begin(), roots.end(),
                  [](const auto &a, const auto &b) { return a.first.size() > b.first.size(); });
        QJsonObject out;
        for (const char *key : {"lastNote", "steps", "finalSnapshots"}) {
            out.insert(QLatin1String(key), stripped(manifest.value(QLatin1String(key)), roots));
        }
        return out;
    }

    QString shown(const QJsonValue &value) {
        if (value.isString()) {
            return value.toString();
        }
        return QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact))
            .mid(1)
            .chopped(1);
    }

}

int compareManifests(const fs::path &first, const fs::path &second) {
    const auto a = readManifest(first);
    const auto b = readManifest(second);
    if (!a || !b) {
        return 1;
    }
    const auto x = comparable(*a);
    const auto y = comparable(*b);

    if (x.value(QStringLiteral("lastNote")) != y.value(QStringLiteral("lastNote"))) {
        stdc::u8printf("differ: lastNote\n");
        return 2;
    }
    const auto stepsA = x.value(QStringLiteral("steps")).toArray();
    const auto stepsB = y.value(QStringLiteral("steps")).toArray();
    for (qsizetype i = 0; i < std::min(stepsA.size(), stepsB.size()); ++i) {
        const auto s = stepsA[i].toObject();
        const auto t = stepsB[i].toObject();
        for (const char *key : {"index", "kind", "note", "arguments", "outputs", "snapshots"}) {
            const auto u = s.value(QLatin1String(key));
            const auto v = t.value(QLatin1String(key));
            if (u != v) {
                stdc::u8printf("differ: step %d, %s\n  %s\n  %s\n",
                               s.value(QStringLiteral("index")).toInt(), key,
                               shown(u).toStdString().c_str(), shown(v).toStdString().c_str());
                return 2;
            }
        }
    }
    if (stepsA.size() != stepsB.size()) {
        stdc::u8printf("differ: %d steps and %d steps\n", int(stepsA.size()), int(stepsB.size()));
        return 2;
    }
    if (x.value(QStringLiteral("finalSnapshots")) != y.value(QStringLiteral("finalSnapshots"))) {
        stdc::u8printf("differ: finalSnapshots\n  %s\n  %s\n",
                       shown(x.value(QStringLiteral("finalSnapshots"))).toStdString().c_str(),
                       shown(y.value(QStringLiteral("finalSnapshots"))).toStdString().c_str());
        return 2;
    }
    stdc::u8printf("same: %d steps\n", int(stepsA.size()));
    return 0;
}

namespace {

    // The files an engine derives from the samples of a voice bank: the frequency tables of
    // the resamplers and the models and database of moresampler. Everything else is kept.
    bool isDerived(const fs::path &file) {
        auto name = QString::fromStdU16String(file.filename().u16string()).toLower();
        static const char *const suffixes[] = {
            ".frq",  ".frt",   ".frc", ".pmk",  ".mrq",      ".llsm",    ".llsm.tmp",
            ".gfrq", ".uspec", ".dio", ".star", ".platinum", ".vs4ufrq", ".spec",
        };
        return std::any_of(std::begin(suffixes), std::end(suffixes), [&name](const char *suffix) {
            return name.endsWith(QLatin1String(suffix));
        });
    }

}

int copyVoice(const fs::path &from, const fs::path &to) {
    std::error_code code;
    if (!fs::is_directory(from, code)) {
        error(QStringLiteral("%1 is not a folder.").arg(textOf(from)));
        return 1;
    }
    // Never into existing files, so that nothing is overwritten or left over from before
    if (fs::exists(to, code) && !fs::is_empty(to, code)) {
        error(QStringLiteral("%1 exists and is not empty. Remove it first.").arg(textOf(to)));
        return 1;
    }
    int copied = 0;
    std::map<QString, int> skipped;
    for (auto it = fs::recursive_directory_iterator(from, code);
         it != fs::recursive_directory_iterator(); it.increment(code)) {
        if (code) {
            error(QStringLiteral("%1 could not be read.").arg(textOf(from)));
            return 1;
        }
        const auto relative = fs::relative(it->path(), from, code);
        const auto target = to / relative;
        if (it->is_directory(code)) {
            fs::create_directories(target, code);
            continue;
        }
        if (isDerived(it->path())) {
            skipped[QString::fromStdU16String(it->path().extension().u16string()).toLower()]++;
            continue;
        }
        fs::create_directories(target.parent_path(), code);
        if (!fs::copy_file(it->path(), target, code)) {
            error(QStringLiteral("%1 could not be copied.").arg(textOf(it->path())));
            return 1;
        }
        ++copied;
    }
    stdc::u8printf("copied %d files\n", copied);
    for (const auto &[extension, count] : skipped) {
        stdc::u8printf("left out %d %s\n", count, extension.toStdString().c_str());
    }
    return 0;
}
