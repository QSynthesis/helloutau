#include "FindSupport_p.h"

#include <algorithm>

#include <helloutau/Widgets/FindBar.h>

namespace hello::daw {

    kit::TextSearch FindSupport::searchOf(const FindBar *bar) {
        kit::TextSearch::Options options;
        if (bar->isCaseSensitive()) {
            options |= kit::TextSearch::CaseSensitive;
        }
        if (bar->isWholeWord()) {
            options |= kit::TextSearch::WholeWord;
        }
        if (bar->isRegularExpression()) {
            options |= kit::TextSearch::RegularExpression;
        }
        return kit::TextSearch(bar->text(), options);
    }

    void FindSupport::showResult(FindBar *bar, const kit::TextSearch &search,
                                 const QList<int> &matches, int current) {
        if (!search.errorString().isEmpty()) {
            bar->setError(search.errorString());
            return;
        }
        const auto at = std::lower_bound(matches.begin(), matches.end(), current);
        const bool isMatch = current >= 0 && at != matches.end() && *at == current;
        bar->setResult(isMatch ? int(at - matches.begin()) + 1 : 0, int(matches.size()));
    }

    QList<FindMatches::Range> FindSupport::rangesOf(const QList<kit::TextSearch::Match> &matches) {
        QList<FindMatches::Range> ranges;
        ranges.reserve(matches.size());
        for (const auto &match : matches) {
            ranges.push_back({match.start, match.length});
        }
        return ranges;
    }

}
