#ifndef HELLOUTAU_WIDGETS_COMMANDPALETTE_H
#define HELLOUTAU_WIDGETS_COMMANDPALETTE_H

#include <QtCore/QPointer>
#include <QtGui/QColor>
#include <QtWidgets/QFrame>

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
    /// of the widget. The shadow around the palette belongs to the style sheet as well.
    class HELLOUTAU_WIDGETS_EXPORT CommandPalette : public QFrame {
        Q_OBJECT
        Q_PROPERTY(QColor matchColor READ matchColor WRITE setMatchColor)
        Q_PROPERTY(QColor subtitleColor READ subtitleColor WRITE setSubtitleColor)
        Q_PROPERTY(QColor keyCapColor READ keyCapColor WRITE setKeyCapColor)
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

    Q_SIGNALS:
        /// Emitted after the palette has closed, when the user chooses the command \a id.
        void commandActivated(const QString &id);

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;
        bool event(QEvent *event) override;

    private:
        QList<CommandEntry> m_commands;
        QStringList m_recentIds;
        QLineEdit *m_input;
        QListWidget *m_list;
        QPointer<QWidget> m_previousFocus;

        QColor m_matchColor;
        QColor m_subtitleColor;
        QColor m_keyCapColor;

        void updateList();
        void place();
        void activateCurrent();
        void dismiss();
    };

}

#endif // HELLOUTAU_WIDGETS_COMMANDPALETTE_H
