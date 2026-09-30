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

        /// A text shown after the label on the same line in a dimmer color, and matched as
        /// well, such as the folder of a recent file in VS Code. Empty for none.
        QString description;
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
            /// matched through another text.
            QList<qsizetype> positions;

            /// The positions of the matched characters in the description, empty if the entry
            /// matched through another text.
            QList<qsizetype> descriptionPositions;
        };

        /// The order of the entries that rank() returns.
        enum Order {
            /// The best match first
            ByScore,
            /// The order of the given entries, as the list of recent files of VS Code keeps
            /// the latest first while the user types
            AsGiven,
        };

        /// Returns the best match of \a query in \a text, or \c std::nullopt if the characters
        /// of \a query do not all appear in \a text in order. An empty query matches every text
        /// with a score of zero.
        static std::optional<Match> match(QStringView query, QStringView text);

        /// Returns the entries that match \a query through their label, description or
        /// alternative text, whichever matches best. With \c ByScore, the best match comes first,
        /// and entries that score equally are ordered by the length and then the text of their
        /// labels, so that the order does not depend on the order of \a entries.
        static QList<Ranked> rank(QStringView query, const QList<CommandEntry> &entries,
                                  Order order = ByScore);
    };

}

#endif // HELLOUTAU_WIDGETS_COMMANDMATCHER_H
