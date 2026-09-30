#ifndef HELLOUTAU_EDITOR_FINDSUPPORT_P_H
#define HELLOUTAU_EDITOR_FINDSUPPORT_P_H

#include <optional>

#include <QtCore/QList>

#include <hellokit/Support/TextSearch.h>

namespace hello::daw {

    class FindBar;

    /// The parts of find and replace that the project window and the voice bank window share.
    /// A position is an index in the order of the searched items, a note of the track or a row
    /// of the entry table.
    class FindSupport {
    public:
        /// Returns the search that the query and the options of \a bar describe.
        static kit::TextSearch searchOf(const FindBar *bar);

        /// Returns the index in \a matches, positions in ascending order, of the match after
        /// \a from if \a forward is true and before it otherwise. The search continues from the
        /// other end past the last or the first match. With \a inclusive, a match at \a from
        /// itself is returned. A negative \a from precedes every position. Returns
        /// \c std::nullopt if \a matches is empty.
        static std::optional<qsizetype> adjacentMatch(const QList<int> &matches, int from,
                                                      bool forward, bool inclusive);

        /// Shows in \a bar the number of \a matches with the match at \a current as the current
        /// match, or the error of \a search if it is invalid.
        static void showResult(FindBar *bar, const kit::TextSearch &search,
                               const QList<int> &matches, int current);
    };

}

#endif // HELLOUTAU_EDITOR_FINDSUPPORT_P_H
