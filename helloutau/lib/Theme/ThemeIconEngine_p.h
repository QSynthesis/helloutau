#ifndef HELLOUTAU_THEME_THEMEICONENGINE_P_H
#define HELLOUTAU_THEME_THEMEICONENGINE_P_H

#include <optional>

#include <QtGui/QIconEngine>

#include "ThemeIcon.h"

namespace hello::daw {

    /// The icon engine that draws a ThemeIcon, in the state that QIcon passes or in a fixed
    /// state.
    ///
    /// The icon name of the engine is the file name that encodes the icon, so that
    /// ThemeIcon::of() retrieves the icon from a QIcon through public interfaces.
    class ThemeIconEngine : public QIconEngine {
    public:
        explicit ThemeIconEngine(ThemeIcon icon,
                                 std::optional<ThemeButtonState> state = std::nullopt,
                                 const QColor &text = QColor());

        void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
                   QIcon::State state) override;
        QSize actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override;
        QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override;
        QString key() const override;
        QIconEngine *clone() const override;
        QString iconName() override;
        bool isNull() override;

    private:
        ThemeIcon m_icon;
        std::optional<ThemeButtonState> m_state;
        QColor m_text;

        ThemeButtonState stateFor(QIcon::Mode mode, QIcon::State state) const;
        QColor colorFor(ThemeButtonState state) const;
    };

}

#endif // HELLOUTAU_THEME_THEMEICONENGINE_P_H
