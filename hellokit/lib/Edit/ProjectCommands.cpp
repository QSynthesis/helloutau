#include "ProjectCommands.h"

#include <QtCore/QJsonObject>

#include "CommandSyntax.h"
#include "NodeCommands_p.h"
#include "ProjectEdits.h"
#include "ProjectFields_p.h"
#include "ProjectRefs.h"
#include "ProjectTree_p.h"

namespace hello::kit {

    namespace {

        using Arguments = QList<CommandArgument>;

        bool fail(DiagnosticList &diagnostics, const QString &message) {
            return NodeCommands::fail(diagnostics, message);
        }

        bool usage(DiagnosticList &diagnostics, const char *form) {
            return fail(diagnostics, ProjectCommands::tr("Usage: %1").arg(QLatin1String(form)));
        }

        std::optional<NodeCommands::Target> targetOf(ProjectSession &session,
                                                     const CommandArgument &argument,
                                                     DiagnosticList &diagnostics) {
            const auto path =
                NodeCommands::stringOf(argument, ProjectCommands::tr("path"), diagnostics);
            if (!path) {
                return std::nullopt;
            }
            return NodeCommands::resolve(session, projectRecord(), *path, diagnostics);
        }

        std::optional<NoteRef> noteAt(ProjectSession &session, const CommandArgument &argument,
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

        std::optional<NoteListRef> notesAt(ProjectSession &session, const CommandArgument &argument,
                                           DiagnosticList &diagnostics) {
            const auto target = targetOf(session, argument, diagnostics);
            if (!target) {
                return std::nullopt;
            }
            if (!target->field || !target->members.isEmpty() ||
                target->field->kind != FieldInfo::List ||
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
            const auto semitones = NodeCommands::integerOf(
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
            const auto index =
                NodeCommands::integerOf(arguments[1], ProjectCommands::tr("index"), diagnostics);
            const auto ticks =
                NodeCommands::integerOf(arguments[2], ProjectCommands::tr("ticks"), diagnostics);
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
            const auto index =
                NodeCommands::integerOf(arguments[1], ProjectCommands::tr("index"), diagnostics);
            if (!notes || !index) {
                return false;
            }
            const auto json = CommandSyntax::valueOf(arguments[2]);
            if (!json.isObject()) {
                return fail(diagnostics, ProjectCommands::tr("The note must be an object."));
            }
            // Converted through a tree, which checks each field against the field table.
            const auto tree =
                NodeCommands::treeOf(*projectRecordOf(NoteType), json.toObject(), diagnostics);
            if (!tree) {
                return false;
            }
            return ProjectEdits::insertNote(*notes, *index, fromTree<Note>(tree.get()),
                                            diagnostics);
        }

        bool tempoCommand(ProjectSession &session, const Arguments &arguments,
                          DiagnosticList &diagnostics) {
            if (arguments.size() != 2) {
                return usage(diagnostics, "note tempo <note> <tempo>");
            }
            const auto note = noteAt(session, arguments[0], diagnostics);
            const auto tempo =
                NodeCommands::numberOf(arguments[1], ProjectCommands::tr("tempo"), diagnostics);
            if (!note || !tempo) {
                return false;
            }
            return ProjectEdits::setTempo(*note, *tempo, diagnostics);
        }

        using DomainCommand = bool (*)(ProjectSession &, const Arguments &, DiagnosticList &);

        const std::pair<const char *, DomainCommand> noteCommands[] = {
            {"transpose", transposeCommand},
            {"split",     splitCommand    },
            {"insert",    insertCommand   },
            {"tempo",     tempoCommand    },
        };

        bool run(ProjectSession &session, const Arguments &arguments, DiagnosticList &diagnostics) {
            const auto &name = arguments[0];
            if (name.kind != CommandArgument::Word) {
                return fail(diagnostics, ProjectCommands::tr("A command begins with its name."));
            }
            if (name.text() != QLatin1String("note")) {
                return NodeCommands::execute(session, projectRecord(), name.text(),
                                             arguments.mid(1), diagnostics);
            }
            if (arguments.size() < 2 || arguments[1].kind != CommandArgument::Word) {
                return fail(diagnostics,
                            ProjectCommands::tr("The command note requires a verb: transpose, "
                                                "split, insert or tempo."));
            }
            const auto verb = arguments[1].text();
            for (const auto &[commandVerb, command] : noteCommands) {
                if (verb == QLatin1String(commandVerb)) {
                    return command(session, arguments.mid(2), diagnostics);
                }
            }
            return fail(diagnostics, ProjectCommands::tr("note %1 is not a command.").arg(verb));
        }

    }

    bool ProjectCommands::execute(ProjectSession &session, QStringView line,
                                  DiagnosticList &diagnostics) {
        const auto arguments = CommandSyntax::split(line, diagnostics);
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
        auto names = NodeCommands::names();
        for (const auto &[verb, command] : noteCommands) {
            Q_UNUSED(command)
            names.push_back(QStringLiteral("note ") + QLatin1String(verb));
        }
        return names;
    }

}
