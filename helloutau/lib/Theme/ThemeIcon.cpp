#include "ThemeIcon.h"

#include <utility>

#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QUrl>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtGui/QPixmap>
#include <QtGui/QPixmapCache>
#include <QtSvg/QSvgRenderer>

#include "ThemeArguments_p.h"
#include "ThemeIconEngine_p.h"
#include "ThemeLogging_p.h"
#include "ThemeReader.h"

namespace hello::daw {

    namespace {

        constexpr QStringView Suffix = u".svgx";

        // The cached file contents by path, and the generation of the rendered images.
        // clearCache() increments the generation, so that lookups in QPixmapCache no longer
        // match the earlier images.
        struct FileCache {
            QMutex mutex;
            QHash<QString, QByteArray> files;
            quint64 generation = 0;
        };

        FileCache &fileCache() {
            static FileCache cache;
            return cache;
        }

        // Returns the contents of \a path, or an empty array if the file cannot be read. The
        // failure is reported once per path.
        QByteArray fileData(const QString &path, quint64 *generation) {
            auto &cache = fileCache();
            QMutexLocker lock(&cache.mutex);
            *generation = cache.generation;
            auto it = cache.files.find(path);
            if (it == cache.files.end()) {
                QByteArray data;
                QFile file(path);
                if (file.open(QIODevice::ReadOnly)) {
                    data = file.readAll();
                } else {
                    qCWarning(lcTheme) << "The icon file" << path << "cannot be read.";
                }
                it = cache.files.insert(path, data);
            }
            return *it;
        }

        QString quoted(const QString &text) {
            QString result = QStringLiteral("\"");
            for (const QChar c : text) {
                if (c == u'\\' || c == u'"') {
                    result += u'\\';
                }
                result += c;
            }
            return result + u'"';
        }

        // Writes \a states as a group that lists only the states whose values differ from their
        // fallback states. ThemeStates::read() restores the same values from the group.
        template <class T, class Write>
        QString writeStates(const ThemeStates<T> &states, Write write) {
            QStringList parts{write(states.value(ThemeButtonState::Up))};
            for (size_t i = 1; i < 8; ++i) {
                const auto state = ThemeButtonState(i);
                if (!(states.value(state) == states.value(themeStateFallback(state)))) {
                    parts.push_back(themeStateKey(state).toString() + u'=' +
                                    write(states.value(state)));
                }
            }
            return u'(' + parts.join(QStringLiteral(", ")) + u')';
        }

        QString writeColor(const QColor &color) {
            return color.isValid() ? color.name(QColor::HexArgb) : QStringLiteral("auto");
        }

        std::optional<QColor> readColor(const ThemeValue &value, ThemeError *error) {
            if (value.kind == ThemeValue::Word && value.text == u"auto") {
                return QColor();
            }
            return ThemeReader::color(value, error);
        }

    }

    bool ThemeIcon::isNull() const {
        for (size_t i = 0; i < 8; ++i) {
            if (!files.value(ThemeButtonState(i)).isEmpty()) {
                return false;
            }
        }
        return true;
    }

    QString ThemeIcon::fileName() const {
        const auto text =
            writeStates(files, quoted) + QStringLiteral(", ") + writeStates(colors, writeColor);
        // Percent-encoded, so that the name contains no path separator and no style sheet quote
        return QString::fromLatin1(QUrl::toPercentEncoding(text)) + Suffix;
    }

    std::optional<ThemeIcon> ThemeIcon::fromFileName(QStringView fileName) {
        if (!fileName.endsWith(Suffix, Qt::CaseInsensitive)) {
            return std::nullopt;
        }
        const auto text =
            QUrl::fromPercentEncoding(fileName.first(fileName.size() - Suffix.size()).toLatin1());
        const auto arguments = ThemeSyntax::parseArguments(text);
        return arguments ? read(*arguments, nullptr) : std::nullopt;
    }

    std::optional<ThemeIcon> ThemeIcon::of(const QIcon &icon) {
        return fromFileName(icon.name());
    }

    QIcon ThemeIcon::icon() const {
        return QIcon(new ThemeIconEngine(*this));
    }

    QIcon ThemeIcon::forState(const QIcon &icon, ThemeButtonState state, const QColor &text) {
        const auto themeIcon = of(icon);
        if (!themeIcon) {
            return icon;
        }
        return QIcon(new ThemeIconEngine(*themeIcon, state, text));
    }

    QIcon ThemeIcon::checkedLook(const QIcon &icon) {
        auto themeIcon = of(icon);
        if (!themeIcon) {
            return icon;
        }
        using S = ThemeButtonState;
        for (const auto &[unchecked, checked] :
             {std::pair(S::Up, S::CheckedUp), std::pair(S::Over, S::CheckedOver),
              std::pair(S::Down, S::CheckedDown), std::pair(S::Disabled, S::CheckedDisabled)}) {
            themeIcon->files.setValue(unchecked, themeIcon->files.value(checked));
        }
        return themeIcon->icon();
    }

    void ThemeIcon::clearCache() {
        auto &cache = fileCache();
        QMutexLocker lock(&cache.mutex);
        cache.files.clear();
        ++cache.generation;
    }

    std::optional<ThemeIcon> ThemeIcon::read(const std::vector<ThemeArgument> &arguments,
                                             ThemeError *error) {
        const auto bound = ThemeArguments::bind(arguments, {u"file", u"color"}, error);
        if (!bound) {
            return std::nullopt;
        }
        const auto &b = *bound;
        if (!b[0]) {
            if (error) {
                *error = {0, tr("The file is required.")};
            }
            return std::nullopt;
        }
        ThemeIcon icon;
        const auto files = ThemeStates<QString>::read(
            *b[0], [](const ThemeValue &v, ThemeError *e) { return ThemeReader::text(v, e); },
            error);
        if (!files) {
            return std::nullopt;
        }
        icon.files = *files;
        if (b[1]) {
            const auto colors = ThemeStates<QColor>::read(*b[1], readColor, error);
            if (!colors) {
                return std::nullopt;
            }
            icon.colors = *colors;
        }
        return icon;
    }

    bool ThemeIcon::operator==(const ThemeIcon &RHS) const {
        return files == RHS.files && colors == RHS.colors;
    }

    ThemeIconEngine::ThemeIconEngine(ThemeIcon icon, std::optional<ThemeButtonState> state,
                                     const QColor &text)
        : m_icon(std::move(icon)), m_state(state), m_text(text) {
    }

    void ThemeIconEngine::paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
                                QIcon::State state) {
        const qreal ratio = painter->device() ? painter->device()->devicePixelRatio() : 1;
        auto image = pixmap(rect.size() * ratio, mode, state);
        if (image.isNull()) {
            return;
        }
        image.setDevicePixelRatio(ratio);
        // Centered in \a rect, because the image preserves the aspect ratio of the file
        const auto size = image.deviceIndependentSize();
        const QPointF origin(rect.x() + (rect.width() - size.width()) / 2,
                             rect.y() + (rect.height() - size.height()) / 2);
        painter->drawPixmap(origin, image);
    }

    QSize ThemeIconEngine::actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) {
        quint64 generation;
        const QSvgRenderer renderer(
            fileData(m_icon.files.value(stateFor(mode, state)), &generation));
        auto actual = renderer.defaultSize();
        if (actual.isEmpty()) {
            return size;
        }
        actual.scale(size, Qt::KeepAspectRatio);
        return actual;
    }

    QPixmap ThemeIconEngine::pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) {
        const auto buttonState = stateFor(mode, state);
        const auto &file = m_icon.files.value(buttonState);
        const auto color = colorFor(buttonState);
        if (file.isEmpty() || size.isEmpty()) {
            return QPixmap();
        }

        quint64 generation;
        auto data = fileData(file, &generation);
        const auto key = QStringLiteral("hello.svgx/%1/%2x%3/%4/")
                             .arg(generation)
                             .arg(size.width())
                             .arg(size.height())
                             .arg(color.name(QColor::HexArgb)) +
                         file;
        QPixmap result;
        if (QPixmapCache::find(key, &result)) {
            return result;
        }

        // QtSvg reads no alpha channel from a color in the form #RRGGBB. The alpha therefore
        // applies to the whole icon as the opacity of the painter.
        data.replace("currentColor", color.name(QColor::HexRgb).toLatin1());
        QSvgRenderer renderer(data);
        if (!renderer.isValid()) {
            return QPixmap();
        }
        auto actual = renderer.defaultSize();
        if (actual.isEmpty()) {
            actual = size;
        } else {
            actual.scale(size, Qt::KeepAspectRatio);
        }
        QImage image(actual, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setOpacity(color.alphaF());
        renderer.render(&painter);
        painter.end();

        result = QPixmap::fromImage(image);
        QPixmapCache::insert(key, result);
        return result;
    }

    QString ThemeIconEngine::key() const {
        return Suffix.sliced(1).toString();
    }

    QIconEngine *ThemeIconEngine::clone() const {
        return new ThemeIconEngine(*this);
    }

    QString ThemeIconEngine::iconName() {
        return m_icon.fileName();
    }

    bool ThemeIconEngine::isNull() {
        return m_icon.isNull();
    }

    ThemeButtonState ThemeIconEngine::stateFor(QIcon::Mode mode, QIcon::State state) const {
        if (m_state) {
            return *m_state;
        }
        auto result = ThemeButtonState::Up;
        switch (mode) {
            case QIcon::Active:
            case QIcon::Selected:
                result = ThemeButtonState::Over;
                break;
            case QIcon::Disabled:
                result = ThemeButtonState::Disabled;
                break;
            default:
                break;
        }
        if (state == QIcon::On) {
            result = ThemeButtonState(size_t(result) + size_t(ThemeButtonState::CheckedUp));
        }
        return result;
    }

    QColor ThemeIconEngine::colorFor(ThemeButtonState state) const {
        const auto &color = m_icon.colors.value(state);
        if (color.isValid()) {
            return color;
        }
        if (m_text.isValid()) {
            return m_text;
        }
        // The window text color, because no text color is associated with the icon
        const bool disabled =
            state == ThemeButtonState::Disabled || state == ThemeButtonState::CheckedDisabled;
        return QGuiApplication::palette().color(disabled ? QPalette::Disabled : QPalette::Active,
                                                QPalette::WindowText);
    }

}
