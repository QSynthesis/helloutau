#include "Vibrato.h"

#include <stdutau/note.h>

namespace hello::kit {

    QJsonObject Vibrato::toJson() const {
        return QJsonObject{
            {QLatin1String("length"),    length   },
            {QLatin1String("period"),    period   },
            {QLatin1String("amplitude"), amplitude},
            {QLatin1String("attack"),    attack   },
            {QLatin1String("release"),   release  },
            {QLatin1String("phase"),     phase    },
            {QLatin1String("offset"),    offset   },
            {QLatin1String("intensity"), intensity},
        };
    }

    Vibrato Vibrato::fromJson(const QJsonObject &object) {
        Vibrato vibrato;
        vibrato.length = object.value(QLatin1String("length")).toDouble();
        vibrato.period = object.value(QLatin1String("period")).toDouble();
        vibrato.amplitude = object.value(QLatin1String("amplitude")).toDouble();
        vibrato.attack = object.value(QLatin1String("attack")).toDouble();
        vibrato.release = object.value(QLatin1String("release")).toDouble();
        vibrato.phase = object.value(QLatin1String("phase")).toDouble();
        vibrato.offset = object.value(QLatin1String("offset")).toDouble();
        vibrato.intensity = object.value(QLatin1String("intensity")).toDouble();
        return vibrato;
    }

    Vibrato Vibrato::utauDefault() {
        const utau::Vibrato utauVibrato;
        Vibrato vibrato;
        vibrato.length = utauVibrato.length;
        vibrato.period = utauVibrato.period;
        vibrato.amplitude = utauVibrato.amplitude;
        vibrato.attack = utauVibrato.attack;
        vibrato.release = utauVibrato.release;
        vibrato.phase = utauVibrato.phase;
        vibrato.offset = utauVibrato.offset;
        vibrato.intensity = utauVibrato.intensity;
        return vibrato;
    }

}
