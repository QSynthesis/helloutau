#include "CommandMatcher.h"

#include <algorithm>
#include <vector>

namespace hello::daw {

    namespace {

        // Each matched character scores one, and more at the start of a word or right after the
        // previous matched character. The two bonuses are equal, so that "sa" scores the same in
        // "Save" and as the initials of "Save As", and the shorter label ranks first.
        constexpr int CharacterScore = 1;
        constexpr int WordStartBonus = 8;
        constexpr int ConsecutiveBonus = 8;

        // A character of a script without spaces, such as Chinese or Japanese, is a word of its
        // own.
        bool isIdeographic(QChar c) {
            return c.unicode() >= 0x2E80;
        }

        bool isWordStart(QStringView text, qsizetype i) {
            if (i == 0) {
                return true;
            }
            const auto current = text[i];
            const auto previous = text[i - 1];
            return !previous.isLetterOrNumber() || isIdeographic(current) ||
                   (current.isUpper() && previous.isLower());
        }

    }

    std::optional<CommandMatcher::Match> CommandMatcher::match(QStringView query,
                                                               QStringView text) {
        const auto m = query.size();
        const auto n = text.size();
        if (m == 0) {
            return Match{};
        }
        if (m > n) {
            return std::nullopt;
        }

        // best[i][j]: the best score of the first i + 1 query characters with the last one at
        // text position j, or -1 if impossible. from[i][j]: the text position of the previous
        // query character in that match.
        std::vector<std::vector<int>> best(size_t(m), std::vector<int>(size_t(n), -1));
        std::vector<std::vector<qsizetype>> from(size_t(m), std::vector<qsizetype>(size_t(n), -1));

        for (qsizetype i = 0; i < m; ++i) {
            const auto wanted = query[i].toCaseFolded();

            // The best score of the previous query character at any position before j - 1,
            // and where it was, updated as j advances.
            int earlierBest = -1;
            qsizetype earlierAt = -1;

            for (qsizetype j = i; j < n; ++j) {
                if (i > 0 && j >= 2 && best[size_t(i - 1)][size_t(j - 2)] > earlierBest) {
                    earlierBest = best[size_t(i - 1)][size_t(j - 2)];
                    earlierAt = j - 2;
                }
                if (text[j].toCaseFolded() != wanted) {
                    continue;
                }

                const int own = CharacterScore + (isWordStart(text, j) ? WordStartBonus : 0);
                if (i == 0) {
                    best[0][size_t(j)] = own;
                    continue;
                }

                const int adjacent = best[size_t(i - 1)][size_t(j - 1)];
                if (adjacent >= 0 && adjacent + ConsecutiveBonus >= earlierBest) {
                    best[size_t(i)][size_t(j)] = adjacent + ConsecutiveBonus + own;
                    from[size_t(i)][size_t(j)] = j - 1;
                } else if (earlierBest >= 0) {
                    best[size_t(i)][size_t(j)] = earlierBest + own;
                    from[size_t(i)][size_t(j)] = earlierAt;
                }
            }
        }

        const auto &last = best[size_t(m - 1)];
        const auto top = std::max_element(last.begin(), last.end());
        if (*top < 0) {
            return std::nullopt;
        }

        Match result;
        result.score = *top;
        result.positions.resize(m);
        auto at = qsizetype(top - last.begin());
        for (auto i = m - 1; i >= 0; --i) {
            result.positions[i] = at;
            at = from[size_t(i)][size_t(at)];
        }
        return result;
    }

    QList<CommandMatcher::Ranked> CommandMatcher::rank(QStringView query,
                                                       const QList<CommandEntry> &entries,
                                                       Order order) {
        struct Candidate {
            Ranked ranked;
            int score;
        };

        std::vector<Candidate> candidates;
        for (qsizetype i = 0; i < entries.size(); ++i) {
            const auto &entry = entries[i];
            const auto byLabel = match(query, entry.label);
            const auto byDescription =
                entry.description.isEmpty() ? std::nullopt : match(query, entry.description);
            const auto byAlternative =
                entry.alternative.isEmpty() ? std::nullopt : match(query, entry.alternative);
            const auto score = [](const std::optional<Match> &m) { return m ? m->score : -1; };
            // The label wins a tie, then the description.
            if (byLabel && score(byLabel) >= std::max(score(byDescription), score(byAlternative))) {
                candidates.push_back({
                    {i, byLabel->positions, {}},
                    byLabel->score
                });
            } else if (byDescription && score(byDescription) >= score(byAlternative)) {
                candidates.push_back({
                    {i, {}, byDescription->positions},
                    byDescription->score
                });
            } else if (byAlternative) {
                candidates.push_back({
                    {i, {}, {}},
                    byAlternative->score
                });
            }
        }

        if (order == ByScore) {
            std::stable_sort(candidates.begin(), candidates.end(),
                         [&entries](const Candidate &a, const Candidate &b) {
                             if (a.score != b.score) {
                                 return a.score > b.score;
                             }
                             const auto &labelA = entries[a.ranked.index].label;
                             const auto &labelB = entries[b.ranked.index].label;
                             if (labelA.size() != labelB.size()) {
                                 return labelA.size() < labelB.size();
                             }
                             return labelA < labelB;
                         });
        }

        QList<Ranked> result;
        result.reserve(qsizetype(candidates.size()));
        for (auto &candidate : candidates) {
            result.push_back(std::move(candidate.ranked));
        }
        return result;
    }

}
