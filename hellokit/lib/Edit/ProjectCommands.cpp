#include "ProjectCommands.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <hellokit/EditBase/CommandSyntax.h>
#include <hellokit/EditBase/private/NodeCommands_p.h>

#include "ProjectEdits.h"
#include "ProjectFields_p.h"
#include "ProjectRefs.h"
#include "ProjectTree_p.h"

namespace hello::kit {

    namespace {

        using Arguments = QList<edit::CommandArgument>;

        bool fail(DiagnosticList &diagnostics, const QString &message) {
            return edit::NodeCommands::fail(diagnostics, message);
        }

        bool usage(DiagnosticList &diagnostics, const char *form) {
            return fail(diagnostics, ProjectCommands::tr("Usage: %1").arg(QLatin1String(form)));
        }

        std::optional<edit::NodeCommands::Target> targetOf(ProjectSession &session,
                                                           const edit::CommandArgument &argument,
                                                           DiagnosticList &diagnostics) {
            const auto path =
                edit::NodeCommands::stringOf(argument, ProjectCommands::tr("path"), diagnostics);
            if (!path) {
                return std::nullopt;
            }
            return edit::NodeCommands::resolve(session, projectRecord(), *path, diagnostics);
        }

        std::optional<NoteRef> noteAt(ProjectSession &session,
                                      const edit::CommandArgument &argument,
                                      DiagnosticList &diagnostics) {
            const auto target = targetOf(session, argument, diagnostics);
            if (!target) {
                return std::nullopt;
            }
            if (target->field || target->info->nodeType != NoteType) {
                fail(diagnostics, ProjectCommands::tr("The path %1 does not denote a note.")
                                      .arg(argument.text()));
                return std::nullopt;
            }
            return NoteRef(&session, target->record->id());
        }

        std::optional<NoteListRef> notesAt(ProjectSession &session,
                                           const edit::CommandArgument &argument,
                                           DiagnosticList &diagnostics) {
            const auto target = targetOf(session, argument, diagnostics);
            if (!target) {
                return std::nullopt;
            }
            if (!target->field || !target->members.isEmpty() ||
                target->field->kind != edit::FieldInfo::List ||
                target->field->record->nodeType != NoteType) {
                fail(diagnostics,
                     ProjectCommands::tr("The path %1 does not denote a list of notes.")
                         .arg(argument.text()));
                return std::nullopt;
            }
            return NoteListRef(&session, target->child()->id());
        }

        bool transposeCommand(ProjectSession &session, const Arguments &arguments,
                              DiagnosticList &diagnostics) {
            if (arguments.size() < 2) {
                return usage(diagnostics, "note transpose <semitones> <note>...");
            }
            const auto semitones = edit::NodeCommands::integerOf(
                arguments[0], ProjectCommands::tr("number of semitones"), diagnostics);
            if (!semitones) {
                return false;
            }
            QList<NoteRef> notes;
            for (const auto &argument : arguments.mid(1)) {
                const auto note = noteAt(session, argument, diagnostics);
                if (!note) {
                    return false;
                }
                notes.push_back(*note);
            }
            return ProjectEdits::transpose(notes, *semitones, diagnostics);
        }

        bool splitCommand(ProjectSession &session, const Arguments &arguments,
                          DiagnosticList &diagnostics) {
            if (arguments.size() != 3) {
                return usage(diagnostics, "note split <notes> <index> <ticks>");
            }
            const auto notes = notesAt(session, arguments[0], diagnostics);
            const auto index = edit::NodeCommands::integerOf(
                arguments[1], ProjectCommands::tr("index"), diagnostics);
            const auto ticks = edit::NodeCommands::integerOf(
                arguments[2], ProjectCommands::tr("ticks"), diagnostics);
            if (!notes || !index || !ticks) {
                return false;
            }
            if (*index < 0 || *index >= notes->size()) {
                return fail(diagnostics,
                            ProjectCommands::tr("The track has %1 notes, not a note %2.")
                                .arg(notes->size())
                                .arg(*index));
            }
            return ProjectEdits::splitNote(*notes, *index, *ticks, diagnostics);
        }

        bool insertCommand(ProjectSession &session, const Arguments &arguments,
                           DiagnosticList &diagnostics) {
            if (arguments.size() != 3) {
                return usage(diagnostics, "note insert <notes> <index> <note>");
            }
            const auto notes = notesAt(session, arguments[0], diagnostics);
            const auto index = edit::NodeCommands::integerOf(
                arguments[1], ProjectCommands::tr("index"), diagnostics);
            if (!notes || !index) {
                return false;
            }
            const auto json = edit::CommandSyntax::valueOf(arguments[2]);
            if (!json.isObject()) {
                return fail(diagnostics, ProjectCommands::tr("The note must be an object."));
            }
            // Converted through a tree, which checks each field against the field table.
            const auto tree = edit::NodeCommands::treeOf(*projectRecordOf(NoteType),
                                                         json.toObject(), diagnostics);
            if (!tree) {
                return false;
            }
            return ProjectEdits::insertNote(*notes, *index, edit::fromTree<Note>(tree.get()),
                                            diagnostics);
        }

        bool tempoCommand(ProjectSession &session, const Arguments &arguments,
                          DiagnosticList &diagnostics) {
            if (arguments.size() != 2) {
                return usage(diagnostics, "note tempo <note> <tempo>");
            }
            const auto note = noteAt(session, arguments[0], diagnostics);
            const auto tempo = edit::NodeCommands::numberOf(
                arguments[1], ProjectCommands::tr("tempo"), diagnostics);
            if (!note || !tempo) {
                return false;
            }
            return ProjectEdits::setTempo(*note, *tempo, diagnostics);
        }

        bool removeCommand(ProjectSession &session, const Arguments &arguments,
                           DiagnosticList &diagnostics) {
            if (arguments.size() < 2) {
                return usage(diagnostics, "note remove <notes> <index>...");
            }
            const auto notes = notesAt(session, arguments[0], diagnostics);
            if (!notes) {
                return false;
            }
            QList<int> indices;
            for (const auto &argument : arguments.mid(1)) {
                const auto index = edit::NodeCommands::integerOf(
                    argument, ProjectCommands::tr("index"), diagnostics);
                if (!index) {
                    return false;
                }
                indices.push_back(*index);
            }
            return ProjectEdits::removeNotes(*notes, indices, diagnostics);
        }

        bool lengthCommand(ProjectSession &session, const Arguments &arguments,
                           DiagnosticList &diagnostics) {
            if (arguments.size() != 2) {
                return usage(diagnostics, "note length <note> <ticks>");
            }
            const auto note = noteAt(session, arguments[0], diagnostics);
            const auto ticks = edit::NodeCommands::integerOf(
                arguments[1], ProjectCommands::tr("ticks"), diagnostics);
            if (!note || !ticks) {
                return false;
            }
            return ProjectEdits::setLength(*note, *ticks, diagnostics);
        }

        bool moveCommand(ProjectSession &session, const Arguments &arguments,
                         DiagnosticList &diagnostics) {
            if (arguments.size() != 4) {
                return usage(diagnostics, "note move <notes> <index> <count> <destination>");
            }
            const auto notes = notesAt(session, arguments[0], diagnostics);
            const auto index = edit::NodeCommands::integerOf(
                arguments[1], ProjectCommands::tr("index"), diagnostics);
            const auto count = edit::NodeCommands::integerOf(
                arguments[2], ProjectCommands::tr("count"), diagnostics);
            const auto destination = edit::NodeCommands::integerOf(
                arguments[3], ProjectCommands::tr("destination"), diagnostics);
            if (!notes || !index || !count || !destination) {
                return false;
            }
            return ProjectEdits::moveNotes(*notes, *index, *count, *destination, diagnostics);
        }

        bool portamentoCommand(ProjectSession &session, const Arguments &arguments,
                               DiagnosticList &diagnostics) {
            if (arguments.size() != 2) {
                return usage(diagnostics, "note portamento <note> <points>");
            }
            const auto note = noteAt(session, arguments[0], diagnostics);
            if (!note) {
                return false;
            }
            const auto json = edit::CommandSyntax::valueOf(arguments[1]);
            if (!json.isArray()) {
                return fail(diagnostics, ProjectCommands::tr("The points must be an array."));
            }
            QList<PortamentoPoint> points;
            for (const auto &item : json.toArray()) {
                if (!item.isObject()) {
                    return fail(diagnostics, ProjectCommands::tr("Each point must be an object."));
                }
                // Converted through a tree, which checks each field against the field table.
                const auto tree = edit::NodeCommands::treeOf(*projectRecordOf(PortamentoPointType),
                                                             item.toObject(), diagnostics);
                if (!tree) {
                    return false;
                }
                points.push_back(edit::fromTree<PortamentoPoint>(tree.get()));
            }
            return ProjectEdits::setPortamento(*note, points, diagnostics);
        }

        bool vibratoCommand(ProjectSession &session, const Arguments &arguments,
                            DiagnosticList &diagnostics) {
            if (arguments.size() < 2) {
                return usage(diagnostics, "note vibrato <vibrato or null> <note>...");
            }
            const auto json = edit::CommandSyntax::valueOf(arguments[0]);
            std::optional<Vibrato> vibrato;
            if (json.isObject()) {
                vibrato = Vibrato::fromJson(json.toObject());
            } else if (!json.isNull()) {
                return fail(diagnostics,
                            ProjectCommands::tr("The vibrato must be an object or null."));
            }
            QList<NoteRef> notes;
            for (const auto &argument : arguments.mid(1)) {
                const auto note = noteAt(session, argument, diagnostics);
                if (!note) {
                    return false;
                }
                notes.push_back(*note);
            }
            return ProjectEdits::setVibrato(notes, vibrato, diagnostics);
        }

        using DomainCommand = bool (*)(ProjectSession &, const Arguments &, DiagnosticList &);

        // The domain commands, each with the function of ProjectEdits that it calls.
        struct NoteCommand {
            const char *verb;
            DomainCommand command;
            const char *function;
        };

        constexpr NoteCommand noteCommands[] = {
            {"transpose",  transposeCommand,  "transpose"    },
            {"split",      splitCommand,      "splitNote"    },
            {"insert",     insertCommand,     "insertNote"   },
            {"tempo",      tempoCommand,      "setTempo"     },
            {"remove",     removeCommand,     "removeNotes"  },
            {"length",     lengthCommand,     "setLength"    },
            {"move",       moveCommand,       "moveNotes"    },
            {"portamento", portamentoCommand, "setPortamento"},
            {"vibrato",    vibratoCommand,    "setVibrato"   },
        };

        bool run(ProjectSession &session, const Arguments &arguments, DiagnosticList &diagnostics) {
            const auto &name = arguments[0];
            if (name.kind != edit::CommandArgument::Word) {
                return fail(diagnostics, ProjectCommands::tr("A command begins with its name."));
            }
            if (name.text() != QLatin1String("note")) {
                return edit::NodeCommands::execute(session, projectRecord(), name.text(),
                                                   arguments.mid(1), diagnostics);
            }
            if (arguments.size() < 2 || arguments[1].kind != edit::CommandArgument::Word) {
                return fail(
                    diagnostics,
                    ProjectCommands::tr(
                        "The command note requires a verb: transpose, "
                        "split, insert, tempo, remove, length, move, portamento or vibrato."));
            }
            const auto verb = arguments[1].text();
            for (const auto &command : noteCommands) {
                if (verb == QLatin1String(command.verb)) {
                    return command.command(session, arguments.mid(2), diagnostics);
                }
            }
            return fail(diagnostics, ProjectCommands::tr("note %1 is not a command.").arg(verb));
        }

    }

    bool ProjectCommands::execute(ProjectSession &session, QStringView line,
                                  DiagnosticList &diagnostics) {
        const auto arguments = edit::CommandSyntax::split(line, diagnostics);
        if (!arguments) {
            return false;
        }
        if (arguments->isEmpty()) {
            return true;
        }
        // A refused command ends the transaction without commit, which discards any
        // modification that the command made before it was refused.
        auto transaction = session.transaction(line.trimmed().toString());
        if (!run(session, *arguments, diagnostics)) {
            return false;
        }
        return transaction.commit(diagnostics);
    }

    QStringList ProjectCommands::names() {
        auto names = edit::NodeCommands::names();
        for (const auto &command : noteCommands) {
            names.push_back(QStringLiteral("note ") + QLatin1String(command.verb));
        }
        return names;
    }

    std::optional<QJsonValue> ProjectCommands::query(const ProjectSession &session,
                                                     QStringView line,
                                                     DiagnosticList &diagnostics) {
        const auto arguments = edit::CommandSyntax::split(line, diagnostics);
        if (!arguments) {
            return std::nullopt;
        }
        if (arguments->isEmpty() || arguments->first().kind != edit::CommandArgument::Word) {
            fail(diagnostics, ProjectCommands::tr("A query begins with its name."));
            return std::nullopt;
        }
        return edit::NodeCommands::query(session, projectRecord(), arguments->first().text(),
                                         arguments->mid(1), diagnostics);
    }

    QStringList ProjectCommands::queryNames() {
        return edit::NodeCommands::queryNames();
    }

    QMap<QString, QString> ProjectCommands::domainFunctions() {
        QMap<QString, QString> functions;
        for (const auto &command : noteCommands) {
            functions.insert(QLatin1String(command.function),
                             QStringLiteral("note ") + QLatin1String(command.verb));
        }
        return functions;
    }

}
