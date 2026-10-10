#include "ProjectCommands.h"

#include <algorithm>
#include <cmath>
#include <utility>

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
            if (arguments.size() < 3) {
                return usage(diagnostics, "note insert <notes> <index> <note>...");
            }
            const auto notes = notesAt(session, arguments[0], diagnostics);
            const auto index = edit::NodeCommands::integerOf(
                arguments[1], ProjectCommands::tr("index"), diagnostics);
            if (!notes || !index) {
                return false;
            }
            QList<Note> inserted;
            for (const auto &argument : arguments.mid(2)) {
                const auto json = edit::CommandSyntax::valueOf(argument);
                if (!json.isObject()) {
                    return fail(diagnostics, ProjectCommands::tr("The note must be an object."));
                }
                // Converted through a tree, which checks each field against the field table.
                const auto tree = edit::NodeCommands::treeOf(*projectRecordOf(NoteType),
                                                             json.toObject(), diagnostics);
                if (!tree) {
                    return false;
                }
                inserted.push_back(edit::fromTree<Note>(tree.get()));
            }
            return ProjectEdits::insertNotes(*notes, *index, inserted, diagnostics);
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

        bool envelopeCommand(ProjectSession &session, const Arguments &arguments,
                             DiagnosticList &diagnostics) {
            if (arguments.size() < 2) {
                return usage(diagnostics, "note envelope <envelope or null> <note>...");
            }
            const auto json = edit::CommandSyntax::valueOf(arguments[0]);
            std::optional<Envelope> envelope;
            if (json.isObject()) {
                envelope = Envelope::fromJson(json.toObject());
                if (!envelope) {
                    return fail(diagnostics,
                                ProjectCommands::tr("An envelope has four or five anchors."));
                }
            } else if (!json.isNull()) {
                return fail(diagnostics,
                            ProjectCommands::tr("The envelope must be an object or null."));
            }
            QList<NoteRef> notes;
            for (const auto &argument : arguments.mid(1)) {
                const auto note = noteAt(session, argument, diagnostics);
                if (!note) {
                    return false;
                }
                notes.push_back(*note);
            }
            return ProjectEdits::setEnvelope(notes, envelope, diagnostics);
        }

        bool scaleCommand(ProjectSession &session, const Arguments &arguments,
                          DiagnosticList &diagnostics) {
            if (arguments.size() < 3) {
                return usage(diagnostics,
                             "note scale <portamento factor> <vibrato factor> <note>...");
            }
            const auto portamento = edit::NodeCommands::numberOf(
                arguments[0], ProjectCommands::tr("portamento factor"), diagnostics);
            const auto vibrato = edit::NodeCommands::numberOf(
                arguments[1], ProjectCommands::tr("vibrato factor"), diagnostics);
            if (!portamento || !vibrato) {
                return false;
            }
            QList<NoteRef> notes;
            for (const auto &argument : arguments.mid(2)) {
                const auto note = noteAt(session, argument, diagnostics);
                if (!note) {
                    return false;
                }
                notes.push_back(*note);
            }
            return ProjectEdits::scalePitch(notes, *portamento, *vibrato, diagnostics);
        }

        bool parameterCommand(ProjectSession &session, const Arguments &arguments,
                              DiagnosticList &diagnostics) {
            if (arguments.size() < 3) {
                return usage(diagnostics,
                             "note parameter <intensity, modulation or velocity> <value or null> "
                             "<note>...");
            }
            const std::pair<const char *, ProjectEdits::NoteParameter> names[] = {
                {"intensity",  ProjectEdits::Intensity },
                {"modulation", ProjectEdits::Modulation},
                {"velocity",   ProjectEdits::Velocity  },
            };
            std::optional<ProjectEdits::NoteParameter> parameter;
            for (const auto &[name, value] : names) {
                if (arguments[0].text() == QLatin1String(name)) {
                    parameter = value;
                }
            }
            if (!parameter) {
                return fail(diagnostics, ProjectCommands::tr("%1 is not intensity, modulation or "
                                                             "velocity.")
                                             .arg(arguments[0].text()));
            }
            std::optional<double> value;
            if (edit::CommandSyntax::valueOf(arguments[1]).isNull()) {
                value = std::nullopt;
            } else if (const auto number = edit::NodeCommands::numberOf(
                           arguments[1], ProjectCommands::tr("value"), diagnostics)) {
                value = *number;
            } else {
                return false;
            }
            QList<NoteRef> notes;
            for (const auto &argument : arguments.mid(2)) {
                const auto note = noteAt(session, argument, diagnostics);
                if (!note) {
                    return false;
                }
                notes.push_back(*note);
            }
            return ProjectEdits::setParameter(notes, *parameter, value, diagnostics);
        }

        bool bendCommand(ProjectSession &session, const Arguments &arguments,
                         DiagnosticList &diagnostics) {
            if (arguments.size() != 4) {
                return usage(diagnostics, "note bend <notes> <index> <ticks> <values>");
            }
            const auto notes = notesAt(session, arguments[0], diagnostics);
            const auto index = edit::NodeCommands::integerOf(
                arguments[1], ProjectCommands::tr("index"), diagnostics);
            const auto tick = edit::NodeCommands::numberOf(
                arguments[2], ProjectCommands::tr("ticks"), diagnostics);
            if (!notes || !index || !tick) {
                return false;
            }
            const auto json = edit::CommandSyntax::valueOf(arguments[3]);
            if (!json.isArray()) {
                return fail(diagnostics, ProjectCommands::tr("The values must be an array."));
            }
            QList<double> values;
            for (const auto &item : json.toArray()) {
                if (!item.isDouble()) {
                    return fail(diagnostics, ProjectCommands::tr("Each value must be a number."));
                }
                values.push_back(item.toDouble());
            }
            return ProjectEdits::drawPitchBend(*notes, *index, *tick, values, diagnostics);
        }

        bool pitchBendCommand(ProjectSession &session, const Arguments &arguments,
                              DiagnosticList &diagnostics) {
            if (arguments.size() != 2) {
                return usage(diagnostics, "note pitchbend <Mode1 values or null> <note>");
            }
            const auto json = edit::CommandSyntax::valueOf(arguments[0]);
            std::optional<PitchBend> bend;
            if (json.isObject()) {
                bend = PitchBend::fromJson(json.toObject());
            } else if (!json.isNull()) {
                return fail(diagnostics,
                            ProjectCommands::tr("The Mode1 values must be an object or null."));
            }
            const auto note = noteAt(session, arguments[1], diagnostics);
            if (!note) {
                return false;
            }
            return ProjectEdits::setPitchBend(*note, bend, diagnostics);
        }

        bool mode2Command(ProjectSession &session, const Arguments &arguments,
                          DiagnosticList &diagnostics) {
            if (arguments.size() != 1) {
                return usage(diagnostics, "settings mode2 <true or false>");
            }
            const auto json = edit::CommandSyntax::valueOf(arguments[0]);
            if (!json.isBool()) {
                return fail(diagnostics, ProjectCommands::tr("Mode2 is true or false."));
            }
            return ProjectEdits::setMode2(ProjectRef(&session).settings(), json.toBool(),
                                          diagnostics);
        }

        // settings properties {"name": "...", "tempo": 120, ...}: the fields set replace the
        // properties, see ProjectPropertyChanges.
        bool propertiesCommand(ProjectSession &session, const Arguments &arguments,
                               DiagnosticList &diagnostics) {
            if (arguments.size() != 1) {
                return usage(diagnostics, "settings properties <object>");
            }
            const auto json = edit::CommandSyntax::valueOf(arguments[0]);
            if (!json.isObject()) {
                return fail(diagnostics, ProjectCommands::tr("The properties must be an object."));
            }
            ProjectPropertyChanges changes;
            const auto object = json.toObject();
            for (auto it = object.begin(); it != object.end(); ++it) {
                const auto &key = it.key();
                const auto value = it.value();
                const auto text = [&](std::optional<QString> &field) {
                    if (!value.isString()) {
                        return fail(diagnostics,
                                    ProjectCommands::tr("%1 must be a string.").arg(key));
                    }
                    field = value.toString();
                    return true;
                };
                bool ok = true;
                if (key == QLatin1String("name")) {
                    ok = text(changes.name);
                } else if (key == QLatin1String("flags")) {
                    ok = text(changes.flags);
                } else if (key == QLatin1String("outputFile")) {
                    ok = text(changes.outputFile);
                } else if (key == QLatin1String("voiceDir")) {
                    ok = text(changes.voiceDir);
                } else if (key == QLatin1String("wavtool")) {
                    ok = text(changes.wavtool);
                } else if (key == QLatin1String("resampler")) {
                    ok = text(changes.resampler);
                } else if (key == QLatin1String("tempo")) {
                    if (!value.isDouble()) {
                        return fail(diagnostics, ProjectCommands::tr("tempo must be a number."));
                    }
                    changes.tempo = value.toDouble();
                } else if (key == QLatin1String("mode2")) {
                    if (!value.isBool()) {
                        return fail(diagnostics, ProjectCommands::tr("Mode2 is true or false."));
                    }
                    changes.mode2 = value.toBool();
                } else if (key == QLatin1String("timeSignature")) {
                    const auto timeSignature =
                        value.isObject() ? TimeSignature::fromJson(value.toObject()) : std::nullopt;
                    if (!timeSignature) {
                        return fail(diagnostics,
                                    ProjectCommands::tr("timeSignature must be an object of a "
                                                        "valid numerator and denominator."));
                    }
                    changes.timeSignature = timeSignature;
                } else {
                    return fail(
                        diagnostics,
                        ProjectCommands::tr("%1 is not a property of the project.").arg(key));
                }
                if (!ok) {
                    return false;
                }
            }
            return ProjectEdits::setProperties(ProjectRef(&session), changes, diagnostics);
        }

        // note properties {"lyric": "a", "tempo": null, ...} <notes>...: the fields set replace
        // the properties of the notes, null clearing one that may be left to the default, see
        // NotePropertyChanges.
        bool notePropertiesCommand(ProjectSession &session, const Arguments &arguments,
                                   DiagnosticList &diagnostics) {
            if (arguments.size() < 2) {
                return usage(diagnostics, "note properties <object> <notes>...");
            }
            const auto json = edit::CommandSyntax::valueOf(arguments[0]);
            if (!json.isObject()) {
                return fail(diagnostics, ProjectCommands::tr("The properties must be an object."));
            }
            NotePropertyChanges changes;
            const auto object = json.toObject();
            const std::pair<const char *,
                            std::optional<std::optional<double>> NotePropertyChanges::*>
                numbers[] = {
                    {"tempo",        &NotePropertyChanges::tempo       },
                    {"intensity",    &NotePropertyChanges::intensity   },
                    {"modulation",   &NotePropertyChanges::modulation  },
                    {"velocity",     &NotePropertyChanges::velocity    },
                    {"preUtterance", &NotePropertyChanges::preUtterance},
                    {"voiceOverlap", &NotePropertyChanges::voiceOverlap},
                    {"startPoint",   &NotePropertyChanges::startPoint  },
            };
            for (auto it = object.begin(); it != object.end(); ++it) {
                const auto &key = it.key();
                const auto value = it.value();
                const auto number =
                    std::find_if(std::begin(numbers), std::end(numbers), [&key](const auto &entry) {
                        return key == QLatin1String(entry.first);
                    });
                if (number != std::end(numbers)) {
                    if (value.isNull()) {
                        changes.*(number->second) = std::optional<double>();
                    } else if (value.isDouble()) {
                        changes.*(number->second) = std::optional(value.toDouble());
                    } else {
                        return fail(diagnostics,
                                    ProjectCommands::tr("%1 must be a number or null.").arg(key));
                    }
                } else if (key == QLatin1String("lyric") || key == QLatin1String("flags")) {
                    if (!value.isString()) {
                        return fail(diagnostics,
                                    ProjectCommands::tr("%1 must be a string.").arg(key));
                    }
                    (key == QLatin1String("lyric") ? changes.lyric : changes.flags) =
                        value.toString();
                } else if (key == QLatin1String("length")) {
                    if (!value.isDouble() || value.toDouble() != std::floor(value.toDouble())) {
                        return fail(diagnostics,
                                    ProjectCommands::tr("length must be a whole number."));
                    }
                    changes.length = value.toInt();
                } else {
                    return fail(diagnostics,
                                ProjectCommands::tr("%1 is not a property of a note.").arg(key));
                }
            }
            QList<NoteRef> notes;
            for (const auto &argument : arguments.mid(1)) {
                const auto note = noteAt(session, argument, diagnostics);
                if (!note) {
                    return false;
                }
                notes.push_back(*note);
            }
            return ProjectEdits::setNoteProperties(notes, changes, diagnostics);
        }

        bool combineCommand(ProjectSession &session, const Arguments &arguments,
                            DiagnosticList &diagnostics) {
            if (arguments.size() != 3) {
                return usage(diagnostics, "note combine <notes> <index> <count>");
            }
            const auto notes = notesAt(session, arguments[0], diagnostics);
            const auto index = edit::NodeCommands::integerOf(
                arguments[1], ProjectCommands::tr("index"), diagnostics);
            const auto count = edit::NodeCommands::integerOf(
                arguments[2], ProjectCommands::tr("count"), diagnostics);
            if (!notes || !index || !count) {
                return false;
            }
            return ProjectEdits::combineNotes(*notes, *index, *count, diagnostics);
        }

        bool labelCommand(ProjectSession &session, const Arguments &arguments,
                          DiagnosticList &diagnostics) {
            if (arguments.size() != 2) {
                return usage(diagnostics, "note label <text> <note>");
            }
            const auto label = edit::NodeCommands::stringOf(
                arguments[0], ProjectCommands::tr("label"), diagnostics);
            const auto note = noteAt(session, arguments[1], diagnostics);
            if (!label || !note) {
                return false;
            }
            return ProjectEdits::setLabel(*note, *label, diagnostics);
        }

        bool regionCommand(ProjectSession &session, const Arguments &arguments,
                           DiagnosticList &diagnostics) {
            if (arguments.size() != 4) {
                return usage(diagnostics, "note region <notes> <index> <count> <name>");
            }
            const auto notes = notesAt(session, arguments[0], diagnostics);
            const auto index = edit::NodeCommands::integerOf(
                arguments[1], ProjectCommands::tr("index"), diagnostics);
            const auto count = edit::NodeCommands::integerOf(
                arguments[2], ProjectCommands::tr("count"), diagnostics);
            const auto name = edit::NodeCommands::stringOf(
                arguments[3], ProjectCommands::tr("name"), diagnostics);
            if (!notes || !index || !count || !name) {
                return false;
            }
            return ProjectEdits::nameRegion(*notes, *index, *count, *name, diagnostics);
        }

        // The notes and the region of the arguments <notes> <first> <last> <name>
        std::optional<std::pair<NoteListRef, Region>> regionOf(ProjectSession &session,
                                                               const Arguments &arguments,
                                                               DiagnosticList &diagnostics) {
            const auto notes = notesAt(session, arguments[0], diagnostics);
            const auto first = edit::NodeCommands::integerOf(
                arguments[1], ProjectCommands::tr("first"), diagnostics);
            const auto last = edit::NodeCommands::integerOf(
                arguments[2], ProjectCommands::tr("last"), diagnostics);
            const auto name = edit::NodeCommands::stringOf(
                arguments[3], ProjectCommands::tr("name"), diagnostics);
            if (!notes || !first || !last || !name) {
                return std::nullopt;
            }
            return std::pair{
                *notes, Region{*name, *first, *last}
            };
        }

        bool renameRegionCommand(ProjectSession &session, const Arguments &arguments,
                                 DiagnosticList &diagnostics) {
            if (arguments.size() != 5) {
                return usage(diagnostics,
                             "note renameregion <notes> <first> <last> <name> <new name>");
            }
            const auto region = regionOf(session, arguments, diagnostics);
            const auto name = edit::NodeCommands::stringOf(
                arguments[4], ProjectCommands::tr("new name"), diagnostics);
            if (!region || !name) {
                return false;
            }
            return ProjectEdits::renameRegion(region->first, region->second, *name, diagnostics);
        }

        bool removeRegionCommand(ProjectSession &session, const Arguments &arguments,
                                 DiagnosticList &diagnostics) {
            if (arguments.size() != 4) {
                return usage(diagnostics, "note removeregion <notes> <first> <last> <name>");
            }
            const auto region = regionOf(session, arguments, diagnostics);
            if (!region) {
                return false;
            }
            return ProjectEdits::removeRegion(region->first, region->second, diagnostics);
        }

        using DomainCommand = bool (*)(ProjectSession &, const Arguments &, DiagnosticList &);

        // The domain commands, each with the function of ProjectEdits that it calls.
        struct DomainCommandInfo {
            const char *noun;
            const char *verb;
            DomainCommand command;
            const char *function;
        };

        constexpr DomainCommandInfo domainCommands[] = {
            {"note",     "transpose",    transposeCommand,      "transpose"        },
            {"note",     "split",        splitCommand,          "splitNote"        },
            {"note",     "insert",       insertCommand,         "insertNotes"      },
            {"note",     "tempo",        tempoCommand,          "setTempo"         },
            {"note",     "remove",       removeCommand,         "removeNotes"      },
            {"note",     "length",       lengthCommand,         "setLength"        },
            {"note",     "move",         moveCommand,           "moveNotes"        },
            {"note",     "portamento",   portamentoCommand,     "setPortamento"    },
            {"note",     "vibrato",      vibratoCommand,        "setVibrato"       },
            {"note",     "envelope",     envelopeCommand,       "setEnvelope"      },
            {"note",     "scale",        scaleCommand,          "scalePitch"       },
            {"note",     "parameter",    parameterCommand,      "setParameter"     },
            {"note",     "bend",         bendCommand,           "drawPitchBend"    },
            {"note",     "pitchbend",    pitchBendCommand,      "setPitchBend"     },
            {"note",     "properties",   notePropertiesCommand, "setNoteProperties"},
            {"note",     "combine",      combineCommand,        "combineNotes"     },
            {"note",     "label",        labelCommand,          "setLabel"         },
            {"note",     "region",       regionCommand,         "nameRegion"       },
            {"note",     "renameregion", renameRegionCommand,   "renameRegion"     },
            {"note",     "removeregion", removeRegionCommand,   "removeRegion"     },
            {"settings", "mode2",        mode2Command,          "setMode2"         },
            {"settings", "properties",   propertiesCommand,     "setProperties"    },
        };

        bool run(ProjectSession &session, const Arguments &arguments, DiagnosticList &diagnostics) {
            const auto &name = arguments[0];
            if (name.kind != edit::CommandArgument::Word) {
                return fail(diagnostics, ProjectCommands::tr("A command begins with its name."));
            }
            const auto noun = name.text();
            QStringList verbs;
            for (const auto &command : domainCommands) {
                if (noun == QLatin1String(command.noun)) {
                    verbs.push_back(QLatin1String(command.verb));
                }
            }
            if (verbs.isEmpty()) {
                return edit::NodeCommands::execute(session, projectRecord(), noun, arguments.mid(1),
                                                   diagnostics);
            }
            if (arguments.size() < 2 || arguments[1].kind != edit::CommandArgument::Word) {
                return fail(diagnostics, ProjectCommands::tr("The command %1 requires a verb: %2.")
                                             .arg(noun, verbs.join(QStringLiteral(", "))));
            }
            const auto verb = arguments[1].text();
            for (const auto &command : domainCommands) {
                if (noun == QLatin1String(command.noun) && verb == QLatin1String(command.verb)) {
                    return command.command(session, arguments.mid(2), diagnostics);
                }
            }
            return fail(diagnostics,
                        ProjectCommands::tr("%1 %2 is not a command.").arg(noun, verb));
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
        for (const auto &command : domainCommands) {
            names.push_back(QLatin1String(command.noun) + QLatin1Char(' ') +
                            QLatin1String(command.verb));
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
        for (const auto &command : domainCommands) {
            functions.insert(QLatin1String(command.function), QLatin1String(command.noun) +
                                                                  QLatin1Char(' ') +
                                                                  QLatin1String(command.verb));
        }
        return functions;
    }

}
