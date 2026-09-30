#include "VoiceAliasRule.h"

#include <filesystem>

#include <QtCore/QHash>
#include <QtCore/QSet>

namespace hello::kit {

    QString VoiceAliasRule::apply(const QString &alias) const {
        if (text.isEmpty()) {
            return alias;
        }
        switch (kind) {
            case AddPrefix:
                return text + alias;
            case AddSuffix:
                return alias + text;
            case Replace: {
                auto result = alias;
                return result.replace(text, replacement, Qt::CaseSensitive);
            }
            case RemovePrefix:
                return alias.startsWith(text) ? alias.mid(text.size()) : alias;
            case RemoveSuffix:
                return alias.endsWith(text) ? alias.chopped(text.size()) : alias;
        }
        return alias;
    }

    QString VoiceAliasRule::nameOf(const QString &fileName, const QString &alias) {
        if (!alias.isEmpty()) {
            return alias;
        }
        return QString::fromStdU16String(
            std::filesystem::path(fileName.toStdU16String()).stem().u16string());
    }

    QList<VoiceAliasRule::Change> VoiceAliasRule::plan(const QList<Entry> &entries,
                                                       const QList<int> &selected,
                                                       bool copy) const {
        QList<Change> changes;
        QSet<int> chosen;
        for (const int index : selected) {
            if (index < 0 || index >= entries.size() || chosen.contains(index)) {
                continue;
            }
            chosen.insert(index);
            const auto &entry = entries[index];
            Change change;
            change.index = index;
            change.to = apply(nameOf(entry.fileName, entry.alias));
            changes.push_back(change);
        }
        return check(entries, changes, copy);
    }

    QList<VoiceAliasRule::Change> VoiceAliasRule::check(const QList<Entry> &entries,
                                                        QList<Change> changes, bool copy) {
        QSet<int> chosen;
        for (auto &change : changes) {
            const auto &entry = entries[change.index];
            change.from = nameOf(entry.fileName, entry.alias);
            chosen.insert(change.index);
        }

        // The names of each audio file after the operation, counted
        QHash<QString, QHash<QString, int>> names;
        for (int i = 0; i < entries.size(); ++i) {
            // A renamed entry is counted under its new name below.
            if (!copy && chosen.contains(i)) {
                continue;
            }
            ++names[entries[i].fileName][nameOf(entries[i].fileName, entries[i].alias)];
        }
        for (const auto &change : std::as_const(changes)) {
            if (!change.to.isEmpty()) {
                ++names[entries[change.index].fileName][change.to];
            }
        }

        for (auto &change : changes) {
            if (change.to.isEmpty()) {
                change.problem = tr("The new alias is empty.");
            } else if (names[entries[change.index].fileName][change.to] > 1) {
                change.problem =
                    tr("Another entry of the audio file has the name \"%1\".").arg(change.to);
            }
        }
        return changes;
    }

}
