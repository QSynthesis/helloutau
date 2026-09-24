#include "VoiceBankValidation_p.h"

#include <filesystem>

#include <QtCore/QHash>
#include <QtCore/QSet>

#include <hellokit/EditBase/private/EditSession_p.h>

#include "VoiceBankSession.h"
#include "VoiceBankTree_p.h"

namespace hello::kit {

    namespace {

        template <class NodeType>
        const NodeType &recordOf(const ss::Node *node) {
            return static_cast<const NodeType &>(*node);
        }

        template <class T>
        T get(const ss::StructNodeBase &node, edit::Slot<T> slot) {
            return edit::SlotValue<T>::fromVariant(node.variant(slot.index));
        }

        // The file name without its extension, under which UTAU matches an entry without an
        // alias, as VoiceBank::find() does.
        QString stemOf(const QString &fileName) {
            return QString::fromStdU16String(
                std::filesystem::path(fileName.toStdU16String()).stem().u16string());
        }

        // Returns the note number written as key in decimal ASCII digits without a sign or a
        // leading zero, the form in which the tree stores it, or -1 for any other key. Another
        // form of the same number would be a second key for the same note.
        int noteNumOf(const QString &key) {
            if (key.isEmpty() || key.size() > 3 || (key.size() > 1 && key.front() == u'0')) {
                return -1;
            }
            int value = 0;
            for (const auto c : key) {
                if (c < u'0' || c > u'9') {
                    return -1;
                }
                value = value * 10 + (c.unicode() - u'0');
            }
            return value;
        }

        // Every entry names its audio file, and the entries of one audio file have distinct
        // aliases, where an empty alias counts as the stem of the file name. The messages name
        // the entries by content rather than by position, because an insertion elsewhere moves
        // the positions, and a violation is new if its message is new.
        void checkEntries(const VoiceDirectoryNode &record, QList<edit::Violation> &violations) {
            const auto &entries = static_cast<const ss::VectorNode &>(
                *record.child(VoiceDirectorySlots::OtoEntries.index));
            QHash<QString, QSet<QString>> aliases;
            QSet<QString> reported;
            for (int i = 0; i < entries.size(); ++i) {
                const auto &entry = recordOf<OtoEntryNode>(entries.at(i));
                const auto fileName = get(entry, OtoEntrySlots::FileName);
                const auto alias = get(entry, OtoEntrySlots::Alias);
                if (fileName.isEmpty()) {
                    violations.push_back(
                        {VoiceDirectorySlots::OtoEntries.index,
                         VoiceBankSession::tr("The oto entry with the alias \"%1\" has no file "
                                              "name.")
                             .arg(alias)});
                    continue;
                }
                const auto name = alias.isEmpty() ? stemOf(fileName) : alias;
                auto &names = aliases[fileName];
                if (names.contains(name)) {
                    const auto message =
                        VoiceBankSession::tr("The alias \"%1\" occurs more than once for \"%2\".")
                            .arg(name, fileName);
                    if (!reported.contains(message)) {
                        reported.insert(message);
                        violations.push_back({VoiceDirectorySlots::OtoEntries.index, message});
                    }
                }
                names.insert(name);
            }
        }

        void checkPrefixMap(const VoiceBankNode &record, QList<edit::Violation> &violations) {
            const auto map = record.child(VoiceBankSlots::PrefixMap.index);
            if (!map) {
                return;
            }
            for (const auto &key : static_cast<const ss::MappingNode &>(*map).keys()) {
                const auto noteNum = noteNumOf(key);
                if (noteNum < VoicePrefix::minimumKey || noteNum > VoicePrefix::maximumKey) {
                    violations.push_back(
                        {VoiceBankSlots::PrefixMap.index,
                         VoiceBankSession::tr("The prefix map has the key \"%1\", which is not a "
                                              "note number from %2 to %3.")
                             .arg(key)
                             .arg(VoicePrefix::minimumKey)
                             .arg(VoicePrefix::maximumKey)});
                }
            }
        }

        // A directory that was not read, or whose text did not decode, cannot be saved, see
        // VoiceBankDirectory::leftOut and VoiceBankDirectory::lossy. It is read again rather than
        // edited.
        QString lockOf(const VoiceDirectoryNode &record) {
            const auto path = get(record, VoiceDirectorySlots::Path);
            const auto folder = path.isEmpty() ? VoiceBankSession::tr("the root folder")
                                               : QStringLiteral("\"%1\"").arg(path);
            if (get(record, VoiceDirectorySlots::LeftOut)) {
                return VoiceBankSession::tr("The folder %1 was not read, because no encoding was "
                                            "specified for it. Read it again in an encoding "
                                            "before editing it.")
                    .arg(folder);
            }
            if (get(record, VoiceDirectorySlots::Lossy)) {
                return VoiceBankSession::tr("Part of the text in the folder %1 is not valid in its "
                                            "encoding. Read it again in another encoding before "
                                            "editing it.")
                    .arg(folder);
            }
            return {};
        }

    }

    void registerVoiceBankValidators(edit::EditSession &session) {
        edit::EditSessionPrivate::registerValidator(
            session, VoiceBankType, [](const ss::Node *node, QList<edit::Violation> &violations) {
                checkPrefixMap(recordOf<VoiceBankNode>(node), violations);
            });
        edit::EditSessionPrivate::registerValidator(
            session, VoiceDirectoryType,
            [](const ss::Node *node, QList<edit::Violation> &violations) {
                checkEntries(recordOf<VoiceDirectoryNode>(node), violations);
            });
        edit::EditSessionPrivate::registerLock(
            session, VoiceDirectoryType,
            [](const ss::Node *node) { return lockOf(recordOf<VoiceDirectoryNode>(node)); });
    }

}
