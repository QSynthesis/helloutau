#include "PortamentoPoint.h"

#include <cmath>

#include "JsonFields_p.h"

namespace hello::kit {

    QString PortamentoPoint::typeName(Type type) {
        switch (type) {
            case Linear:
                return QStringLiteral("Linear");
            case R:
                return QStringLiteral("R");
            case J:
                return QStringLiteral("J");
            case S:
                break;
        }
        return QStringLiteral("S");
    }

    std::optional<PortamentoPoint::Type> PortamentoPoint::typeFromName(QStringView name) {
        for (const auto type : {S, Linear, R, J}) {
            if (name == typeName(type)) {
                return type;
            }
        }
        return std::nullopt;
    }

    double PortamentoPoint::centsFromTenths(double tenths) {
        // A tenth of a semitone is ten cents, and the product is counted in millionths of a cent
        return std::round(tenths * 1e7) / 1e6;
    }

    double PortamentoPoint::tenthsFromCents(double cents) {
        return cents / 10;
    }

    double PortamentoPoint::heightAt(const QList<PortamentoPoint> &points, double x) {
        if (points.isEmpty()) {
            return 0;
        }
        if (x <= points.first().x) {
            return points.first().y;
        }
        for (qsizetype i = 1; i < points.size(); ++i) {
            const auto &left = points.at(i - 1);
            const auto &right = points.at(i);
            if (x > right.x) {
                continue;
            }
            if (right.x == left.x) {
                return right.y;
            }
            // The shapes of PitchCurve, from 0 at the left point to 1 at the right point
            constexpr double pi = 3.14159265358979323846;
            const double t = (x - left.x) / (right.x - left.x);
            const double y1 = left.y;
            const double y2 = right.y;
            switch (right.type) {
                case Linear:
                    return y1 + (y2 - y1) * t;
                case J:
                    return y2 + (y1 - y2) * std::cos(pi / 2 * t);
                case R:
                    return y1 + (y2 - y1) * std::sin(pi / 2 * t);
                case S:
                    break;
            }
            return (y1 + y2) / 2 + (y1 - y2) / 2 * std::cos(pi * t);
        }
        return points.last().y;
    }

    QJsonObject PortamentoPoint::toJson() const {
        return QJsonObject{
            {QLatin1String("x"),    x             },
            {QLatin1String("y"),    y             },
            {QLatin1String("type"), typeName(type)},
        };
    }

    PortamentoPoint PortamentoPoint::fromJson(const QJsonObject &object,
                                              DiagnosticList &diagnostics) {
        const auto type = typeFromName(object.value(QLatin1String("type")).toString());
        if (!type) {
            JsonFields::complain(diagnostics, tr("A portamento point has an unknown curve type "
                                                 "and was read as the default type."));
        }
        return {object.value(QLatin1String("x")).toDouble(),
                object.value(QLatin1String("y")).toDouble(), type.value_or(S)};
    }

}
