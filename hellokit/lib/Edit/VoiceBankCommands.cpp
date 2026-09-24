#include "VoiceBankCommands.h"

#include <QtCore/QJsonObject>

#include <hellokit/EditBase/CommandSyntax.h>
#include <hellokit/EditBase/private/NodeCommands_p.h>

#include "VoiceBankEdits.h"
#include "VoiceBankFields_p.h"
#include "VoiceBankRefs.h"
#include "VoiceBankTree_p.h"

namespace hello::kit {

    namespace {

        using Arguments = QList<edit::CommandArgument>;

        bool fail(DiagnosticList &diagnostics, const QString &message) {
            return edit::NodeCommands::fail(diagnostics, message);
        }

        bool usage(DiagnosticList &diagnostics, const char *form) {
            return fail(diagnostics, VoiceBankCommands::tr("Usage: %1").arg(QLatin1String(form)));
        }

        // Returns the record at the path of argument if it has the node type nodeType, or
        // std::nullopt with the reason in diagnostics, which names the record as what.
        std::optional<edit::NodeId> recordAt(VoiceBankSession &session,
                                             const edit::CommandArgument &argument, int nodeType,
                                             const QString &what, DiagnosticList &diagnostics) {
            const auto path =
                edit::NodeCommands::stringOf(argument, VoiceBankCommands::tr("path"), diagnostics);
            if (!path) {
                return std::nullopt;
            }
            const auto target =
                edit::NodeCommands::resolve(session, voiceBankRecord(), *path, diagnostics);
            if (!target) {
                return std::nullopt;
            }
            if (target->field || target->info->nodeType != nodeType) {
                fail(diagnostics, VoiceBankCommands::tr("The path %1 does not denote %2.")
                                      .arg(argument.text(), what));
                return std::nullopt;
            }
            return target->record->id();
        }

        std::optional<VoiceDirectoryRef> directoryAt(VoiceBankSession &session,
                                                     const edit::CommandArgument &argument,
                                                     DiagnosticList &diagnostics) {
            const auto id = recordAt(session, argument, VoiceDirectoryType,
                                     VoiceBankCommands::tr("a folder"), diagnostics);
            return id ? std::optional<VoiceDirectoryRef>(VoiceDirectoryRef(&session, *id))
                      : std::nullopt;
        }

        // Returns the entry written as the JSON of argument, checked against the field table.
        std::optional<VoiceOtoEntry> entryOf(const edit::CommandArgument &argument,
                                             DiagnosticList &diagnostics) {
            const auto json = edit::CommandSyntax::valueOf(argument);
            if (!json.isObject()) {
                fail(diagnostics, VoiceBankCommands::tr("An oto entry must be an object."));
                return std::nullopt;
            }
            const auto tree = edit::NodeCommands::treeOf(*voiceBankRecordOf(OtoEntryType),
                                                         json.toObject(), diagnostics);
            if (!tree) {
                return std::nullopt;
            }
            return edit::fromTree<VoiceOtoEntry>(tree.get());
        }

        bool entrySetCommand(VoiceBankSession &session, const Arguments &arguments,
                             DiagnosticList &diagnostics) {
            if (arguments.size() != 2) {
                return usage(diagnostics, "entry set <entry> <entry JSON>");
            }
            const auto id = recordAt(session, arguments[0], OtoEntryType,
                                     VoiceBankCommands::tr("an oto entry"), diagnostics);
            if (!id) {
                return false;
            }
            const auto value = entryOf(arguments[1], diagnostics);
            if (!value) {
                return false;
            }
            return VoiceBankEdits::setEntry(OtoEntryRef(&session, *id), *value, diagnostics);
        }

        bool entryInsertCommand(VoiceBankSession &session, const Arguments &arguments,
                                DiagnosticList &diagnostics) {
            if (arguments.size() < 2) {
                return usage(diagnostics, "entry insert <directory> <entry JSON>...");
            }
            const auto directory = directoryAt(session, arguments[0], diagnostics);
            if (!directory) {
                return false;
            }
            QList<VoiceOtoEntry> entries;
            for (const auto &argument : arguments.mid(1)) {
                const auto entry = entryOf(argument, diagnostics);
                if (!entry) {
                    return false;
                }
                entries.push_back(*entry);
            }
            return VoiceBankEdits::insertEntries(*directory, entries, diagnostics);
        }

        bool entryIncludeCommand(VoiceBankSession &session, const Arguments &arguments,
                                 DiagnosticList &diagnostics) {
            if (arguments.size() < 2) {
                return usage(diagnostics, "entry include <directory> <file name>...");
            }
            const auto directory = directoryAt(session, arguments[0], diagnostics);
            if (!directory) {
                return false;
            }
            QStringList fileNames;
            for (const auto &argument : arguments.mid(1)) {
                const auto fileName = edit::NodeCommands::stringOf(
                    argument, VoiceBankCommands::tr("file name"), diagnostics);
                if (!fileName) {
                    return false;
                }
                fileNames.push_back(*fileName);
            }
            return VoiceBankEdits::includeAudio(*directory, fileNames, diagnostics);
        }

        bool entryRemoveCommand(VoiceBankSession &session, const Arguments &arguments,
                                DiagnosticList &diagnostics) {
            if (arguments.size() < 2) {
                return usage(diagnostics, "entry remove <directory> <index>...");
            }
            const auto directory = directoryAt(session, arguments[0], diagnostics);
            if (!directory) {
                return false;
            }
            QList<int> indices;
            for (const auto &argument : arguments.mid(1)) {
                const auto index = edit::NodeCommands::integerOf(
                    argument, VoiceBankCommands::tr("index"), diagnostics);
                if (!index) {
                    return false;
                }
                indices.push_back(*index);
            }
            return VoiceBankEdits::removeEntries(*directory, indices, diagnostics);
        }

        bool prefixSetCommand(VoiceBankSession &session, const Arguments &arguments,
                              DiagnosticList &diagnostics) {
            if (arguments.size() != 2) {
                return usage(diagnostics, "prefix set <note number> <prefix JSON>");
            }
            const auto noteNum = edit::NodeCommands::integerOf(
                arguments[0], VoiceBankCommands::tr("note number"), diagnostics);
            if (!noteNum) {
                return false;
            }
            const auto &format = *voiceBankRecord().field(u"prefixMap")->format;
            const auto prefix = format.fromJson(edit::CommandSyntax::valueOf(arguments[1]));
            if (!prefix) {
                return fail(diagnostics, VoiceBankCommands::tr("The prefix must be a %1.")
                                             .arg(QLatin1String(format.typeName)));
            }
            return VoiceBankEdits::setPrefix(VoiceBankRef(&session), *noteNum,
                                             edit::SlotValue<VoicePrefix>::fromVariant(*prefix),
                                             diagnostics);
        }

        bool prefixRemoveCommand(VoiceBankSession &session, const Arguments &arguments,
                                 DiagnosticList &diagnostics) {
            if (arguments.size() != 1) {
                return usage(diagnostics, "prefix remove <note number>");
            }
            const auto noteNum = edit::NodeCommands::integerOf(
                arguments[0], VoiceBankCommands::tr("note number"), diagnostics);
            if (!noteNum) {
                return false;
            }
            return VoiceBankEdits::removePrefix(VoiceBankRef(&session), *noteNum, diagnostics);
        }

        bool directoryCharsetCommand(VoiceBankSession &session, const Arguments &arguments,
                                     DiagnosticList &diagnostics) {
            if (arguments.size() != 2) {
                return usage(diagnostics, "directory charset <directory> <encoding>");
            }
            const auto directory = directoryAt(session, arguments[0], diagnostics);
            const auto charset = edit::NodeCommands::stringOf(
                arguments[1], VoiceBankCommands::tr("encoding"), diagnostics);
            if (!directory || !charset) {
                return false;
            }
            return VoiceBankEdits::convertCharset(*directory, *charset, diagnostics);
        }

        using DomainCommand = bool (*)(VoiceBankSession &, const Arguments &, DiagnosticList &);

        // The domain commands, each with the function of VoiceBankEdits that it calls.
        struct VoiceBankCommand {
            const char *noun;
            const char *verb;
            DomainCommand command;
            const char *function;
        };

        constexpr VoiceBankCommand domainCommands[] = {
            {"entry",     "set",     entrySetCommand,         "setEntry"      },
            {"entry",     "insert",  entryInsertCommand,      "insertEntries" },
            {"entry",     "include", entryIncludeCommand,     "includeAudio"  },
            {"entry",     "remove",  entryRemoveCommand,      "removeEntries" },
            {"prefix",    "set",     prefixSetCommand,        "setPrefix"     },
            {"prefix",    "remove",  prefixRemoveCommand,     "removePrefix"  },
            {"directory", "charset", directoryCharsetCommand, "convertCharset"},
        };

        QString nameOf(const VoiceBankCommand &command) {
            return QLatin1String(command.noun) + QLatin1Char(' ') + QLatin1String(command.verb);
        }

        bool isNoun(QStringView word) {
            for (const auto &command : domainCommands) {
                if (word == QLatin1String(command.noun)) {
                    return true;
                }
            }
            return false;
        }

        bool run(VoiceBankSession &session, const Arguments &arguments,
                 DiagnosticList &diagnostics) {
            const auto &name = arguments[0];
            if (name.kind != edit::CommandArgument::Word) {
                return fail(diagnostics, VoiceBankCommands::tr("A command begins with its name."));
            }
            const auto noun = name.text();
            if (!isNoun(noun)) {
                return edit::NodeCommands::execute(session, voiceBankRecord(), noun,
                                                   arguments.mid(1), diagnostics);
            }
            if (arguments.size() < 2 || arguments[1].kind != edit::CommandArgument::Word) {
                return fail(diagnostics,
                            VoiceBankCommands::tr("The command %1 requires a verb.").arg(noun));
            }
            const auto verb = arguments[1].text();
            for (const auto &command : domainCommands) {
                if (noun == QLatin1String(command.noun) && verb == QLatin1String(command.verb)) {
                    return command.command(session, arguments.mid(2), diagnostics);
                }
            }
            return fail(diagnostics,
                        VoiceBankCommands::tr("%1 %2 is not a command.").arg(noun, verb));
        }

    }

    bool VoiceBankCommands::execute(VoiceBankSession &session, QStringView line,
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

    QStringList VoiceBankCommands::names() {
        auto names = edit::NodeCommands::names();
        for (const auto &command : domainCommands) {
            names.push_back(nameOf(command));
        }
        return names;
    }

    QMap<QString, QString> VoiceBankCommands::domainFunctions() {
        QMap<QString, QString> functions;
        for (const auto &command : domainCommands) {
            functions.insert(QLatin1String(command.function), nameOf(command));
        }
        return functions;
    }

}
