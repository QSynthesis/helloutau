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

    std::optional<qsizetype> FindSupport::adjacentMatch(const QList<int> &matches, int from,
                                                        bool forward, bool inclusive) {
        if (matches.isEmpty()) {
            return std::nullopt;
        }
        if (forward) {
            const auto found = inclusive ? std::lower_bound(matches.begin(), matches.end(), from)
                                         : std::upper_bound(matches.begin(), matches.end(), from);
            return found == matches.end() ? 0 : found - matches.begin();
        }
        if (from < 0) {
            return matches.size() - 1;
        }
        const auto found = inclusive ? std::upper_bound(matches.begin(), matches.end(), from)
                                     : std::lower_bound(matches.begin(), matches.end(), from);
        return found == matches.begin() ? matches.size() - 1 : found - matches.begin() - 1;
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

}
