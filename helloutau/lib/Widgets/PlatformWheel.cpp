#include "PlatformWheel.h"

#include <algorithm>
#include <iterator>

#include <QtCore/QLatin1StringView>
#include <QtGui/QGuiApplication>
#include <QtGui/QWheelEvent>

namespace hello::daw {

    namespace {

        using namespace Qt::Literals::StringLiterals;

        // The platform plugins that transpose the angle delta while Alt is held
        constexpr QLatin1StringView transposingPlatforms[] = {"windows"_L1, "xcb"_L1};

    }

    QPoint PlatformWheel::angleDelta(const QWheelEvent &event) {
        // The platform stays the same while the application runs, and the first wheel event
        // comes after the application exists.
        static const bool transposes =
            std::find(std::begin(transposingPlatforms), std::end(transposingPlatforms),
                      QGuiApplication::platformName()) != std::end(transposingPlatforms);
        const auto delta = event.angleDelta();
        return transposes && (event.modifiers() & Qt::AltModifier) ? delta.transposed() : delta;
    }

}
