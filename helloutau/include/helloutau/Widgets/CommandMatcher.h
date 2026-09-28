#ifndef HELLOUTAU_WIDGETS_COMMANDMATCHER_H
#define HELLOUTAU_WIDGETS_COMMANDMATCHER_H

#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtGui/QKeySequence>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// One command that the command palette offers.
    struct CommandEntry {
        QString id;

        /// The text shown and matched, such as "File: Save", in the language of the interface.
        QString label;

        /// A text that matches without being shown, the label in the language in which the
        /// command was declared, so that a command can be found by its untranslated name. Empty
        /// if it equals the label.
        QString alternative;

        QKeySequence shortcut;

        /// Whether the command switches something on and off, and whether it is on now. The
        /// palette shows what running the command does, "(On)" or "(Off)", after the label.
        bool checkable = false;
        bool checked = false;
    };

    /// Fuzzy matching and ranking of commands by the text typed into the command palette.
    ///
    /// A query matches a text if its characters appear in the text in the same order, ignoring
    /// case, not necessarily next to each other. Among the ways they can appear, the best one is
    /// scored: characters that follow each other and characters at the start of a word score
    /// higher, so that "sa" ranks "File: Save" above "Settings: Language".
    class HELLOUTAU_WIDGETS_EXPORT CommandMatcher {
    public:
        struct Match {
            int score = 0;

            /// The positions in the text of the matched characters, in order.
            QList<qsizetype> positions;
        };

        /// One entry that matches, as rank() returns it.
        struct Ranked {
            /// The index of the entry in the list given to rank().
            qsizetype index = 0;

            /// The positions of the matched characters in the label, empty if the entry
            /// matched through its alternative text only.
            QList<qsizetype> positions;
        };

        /// Returns the best match of \a query in \a text, or \c std::nullopt if the characters
        /// of \a query do not all appear in \a text in order. An empty query matches every text
        /// with a score of zero.
        static std::optional<Match> match(QStringView query, QStringView text);

        /// Returns the entries that match \a query, the best first. Entries that score equally
        /// are ordered by the length and then the text of their labels, so that the order does
        /// not depend on the order of \a entries.
        static QList<Ranked> rank(QStringView query, const QList<CommandEntry> &entries);
    };

}

#endif // HELLOUTAU_WIDGETS_COMMANDMATCHER_H
