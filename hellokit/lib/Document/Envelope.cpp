#include "Envelope.h"

#include <cmath>
#include <utility>

#include <QtCore/QJsonArray>

namespace hello::kit {

    Envelope Envelope::withoutAnchor(int index, double length) const {
        const auto anchors = anchorsInTimeOrder();
        if (index < 0 || index >= anchors.size()) {
            return *this;
        }
        const auto thousandth = [](double milliseconds) {
            return std::round(milliseconds * 1000) / 1000;
        };

        // The time of each anchor from the start of the fragment, and its volume: p1 counts
        // from the start, p2 and p5 from the anchor before, p4 back from the end and p3 back
        // from p4
        QList<std::pair<double, double>> points;
        double time = 0;
        for (qsizetype k = 0; k + 2 < anchors.size(); ++k) {
            time += anchors[k].x;
            points.push_back({time, anchors[k].y});
        }
        const auto &end = anchors.last();
        const auto &release = anchors[anchors.size() - 2];
        points.push_back({length - end.x - release.x, release.y});
        points.push_back({length - end.x, end.y});

        points.removeAt(index);
        if (points.size() == 3) {
            const auto before = index > 0 ? points[index - 1] : std::pair{0.0, 0.0};
            const auto after = index < 3 ? points[index] : std::pair{length, 0.0};
            points.insert(index, {(before.first + after.first) / 2,
                                  std::round((before.second + after.second) / 2)});
        }
        // Distances again, rounded so that the subtraction leaves no binary residue
        const QList<EnvelopeAnchor> result{
            {thousandth(points[0].first),                   points[0].second},
            {thousandth(points[1].first - points[0].first), points[1].second},
            {thousandth(points[3].first - points[2].first), points[2].second},
            {thousandth(length - points[3].first),          points[3].second},
        };
        return *fromTimeOrder(result);
    }

    QJsonObject Envelope::toJson() const {
        QJsonArray anchors;
        for (const auto &anchor : anchorsInTimeOrder()) {
            anchors.append(QJsonObject{
                {QLatin1String("x"), anchor.x},
                {QLatin1String("y"), anchor.y}
            });
        }
        return QJsonObject{
            {QLatin1String("anchors"), anchors}
        };
    }

    std::optional<Envelope> Envelope::fromJson(const QJsonObject &object) {
        QList<EnvelopeAnchor> anchors;
        for (const auto anchor : object.value(QLatin1String("anchors")).toArray()) {
            const auto fields = anchor.toObject();
            anchors.push_back({fields.value(QLatin1String("x")).toDouble(),
                               fields.value(QLatin1String("y")).toDouble()});
        }
        return fromTimeOrder(anchors);
    }

}
