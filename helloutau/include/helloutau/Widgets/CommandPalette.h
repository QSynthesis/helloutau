#ifndef HELLOUTAU_WIDGETS_COMMANDPALETTE_H
#define HELLOUTAU_WIDGETS_COMMANDPALETTE_H

#include <QtCore/QPointer>
#include <QtGui/QColor>
#include <QtWidgets/QFrame>

#include <helloutau/Theme/ThemeTypes.h>

#include <helloutau/Widgets/CommandMatcher.h>
#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QLineEdit;
class QListWidget;

namespace hello::daw {

    /// A list of commands filtered by typing, shown over the top of a window, as the command
    /// palette of VS Code.
    ///
    /// The palette only shows the commands it is given and reports the one the user chooses;
    /// collecting the commands, running the chosen one and remembering which were used recently
    /// are up to the window. Each command is shown with its label, the untranslated label on a
    /// second line if it differs, and its shortcut as key caps.
    ///
    /// The palette closes when a command is chosen, on Escape, on a click outside it, when a
    /// shortcut is triggered and when the focus leaves it. It follows the size of its window.
    ///
    /// The colors are properties that a style sheet can set; unset, they derive from the palette
    /// of the widget. The shadow around the palette belongs to the style sheet as well, written
    /// <tt>qproperty-shadow: qshadow(#40000000, 16px, 0 4px)</tt>; without it there is none.
    class HELLOUTAU_WIDGETS_EXPORT CommandPalette : public QFrame {
        Q_OBJECT
        Q_PROPERTY(QColor matchColor READ matchColor WRITE setMatchColor)
        Q_PROPERTY(QColor subtitleColor READ subtitleColor WRITE setSubtitleColor)
        Q_PROPERTY(QColor keyCapColor READ keyCapColor WRITE setKeyCapColor)
        Q_PROPERTY(QColor highlightColor READ highlightColor WRITE setHighlightColor)
        Q_PROPERTY(
            QColor highlightedTextColor READ highlightedTextColor WRITE setHighlightedTextColor)
        Q_PROPERTY(hello::daw::ThemeShadow shadow READ shadow WRITE setShadow)
    public:
        /// Creates a hidden palette over \a window.
        explicit CommandPalette(QWidget *window);
        ~CommandPalette();

        QList<CommandEntry> commands() const;
        void setCommands(const QList<CommandEntry> &commands);

        /// The identifiers of the commands used most recently, the latest first. While the query
        /// is empty, these commands are listed first, and the first of them is marked as
        /// recently used.
        QStringList recentIds() const;
        void setRecentIds(const QStringList &ids);

        /// The hint in the empty input, "Type a command" at first.
        QString placeholderText() const;
        void setPlaceholderText(const QString &text);

        /// The order of the commands that match the query, the best match first at first.
        CommandMatcher::Order order() const;
        void setOrder(CommandMatcher::Order order);

        /// Whether the selected command and the command under the pointer show a button that
        /// removes the command from the list, as the recent files of VS Code do. A click on the
        /// button emits commandRemoved() and keeps the palette open. False at first.
        bool isRemovable() const;
        void setRemovable(bool removable);

        /// Returns the remove button of isRemovable() in the item of the list at \a rect, drawn
        /// in the font of \a metrics.
        static QRect removeButtonRect(const QRect &rect, const QFontMetrics &metrics);

        /// Shows the palette with an empty query and gives it the focus.
        void popup();

        QString query() const;
        void setQuery(const QString &query);

        /// The identifiers of the commands in the list, in the order shown.
        QStringList shownIds() const;

        /// The identifier of the selected command, or an empty string if the list is empty.
        QString currentId() const;

        /// The color of the typed characters within the labels and of the recently used mark.
        QColor matchColor() const;
        void setMatchColor(const QColor &color);

        /// The color of the untranslated label on the second line.
        QColor subtitleColor() const;
        void setSubtitleColor(const QColor &color);

        /// The background of each key of a shortcut.
        QColor keyCapColor() const;
        void setKeyCapColor(const QColor &color);

        /// The background of the selected command and the color of its text, drawn by the
        /// palette rather than by the style, so that the two always go together: a style may
        /// draw a selection pale and its text in the ordinary color (the Windows 11 style of
        /// Qt 6.11 does).
        QColor highlightColor() const;
        void setHighlightColor(const QColor &color);
        QColor highlightedTextColor() const;
        void setHighlightedTextColor(const QColor &color);

        /// The shadow around the palette, drawn by a QGraphicsDropShadowEffect.
        ThemeShadow shadow() const;
        void setShadow(const ThemeShadow &shadow);

    Q_SIGNALS:
        /// Emitted after the palette has closed, when the user chooses the command \a id.
        void commandActivated(const QString &id);

        /// Emitted after the command \a id was removed from the list by its button.
        void commandRemoved(const QString &id);

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;
        bool event(QEvent *event) override;

    private:
        QList<CommandEntry> m_commands;
        QStringList m_recentIds;
        CommandMatcher::Order m_order = CommandMatcher::ByScore;
        bool m_removable = false;
        QLineEdit *m_input;
        QListWidget *m_list;
        QPointer<QWidget> m_previousFocus;

        QColor m_matchColor;
        QColor m_subtitleColor;
        QColor m_keyCapColor;
        QColor m_highlightColor;
        QColor m_highlightedTextColor;
        ThemeShadow m_shadow;

        void updateList();
        void place();
        void activateCurrent();
        void dismiss();
        void remove(const QString &id);
    };

}

#endif // HELLOUTAU_WIDGETS_COMMANDPALETTE_H
