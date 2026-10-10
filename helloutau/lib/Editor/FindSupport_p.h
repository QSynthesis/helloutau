#ifndef HELLOUTAU_EDITOR_FINDSUPPORT_P_H
#define HELLOUTAU_EDITOR_FINDSUPPORT_P_H

#include <QtCore/QList>

#include <hellokit/Support/TextSearch.h>

#include <helloutau/Widgets/FindMatches.h>

namespace hello::daw {

    class FindBar;

    /// The parts of find and replace that the project window and the voice bank window share and
    /// that depend on kit::TextSearch. The parts that do not are in FindMatches. A position is an
    /// index in the order of the searched items, a note of the track or a row of the entry table.
    class FindSupport {
    public:
        /// Returns the search that the query and the options of \a bar describe.
        static kit::TextSearch searchOf(const FindBar *bar);

        /// Shows in \a bar the number of \a matches with the match at \a current as the current
        /// match, or the error of \a search if it is invalid.
        static void showResult(FindBar *bar, const kit::TextSearch &search,
                               const QList<int> &matches, int current);

        /// Returns \a matches as the ranges that FindMatches highlights.
        static QList<FindMatches::Range> rangesOf(const QList<kit::TextSearch::Match> &matches);
    };

}

#endif // HELLOUTAU_EDITOR_FINDSUPPORT_P_H
