#ifndef HELLOUTAU_WIDGETS_FINDBAR_H
#define HELLOUTAU_WIDGETS_FINDBAR_H

#include <QtCore/QPointer>
#include <QtWidgets/QFrame>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QComboBox;
class QLabel;
class QLineEdit;
class QToolButton;

namespace hello::daw {

    /// A bar for finding and replacing text over the top right corner of a widget, as the find
    /// widget of VS Code. See the section on find and replace in docs/Widgets.md.
    ///
    /// The bar does not block the window. The user edits while it is shown, and it stays until
    /// Escape or its close button hides it. The bar holds the query, the options and the result
    /// count. Searching, moving to a match and replacing are performed by the window in response
    /// to the signals.
    ///
    /// Keys in the bar, as in VS Code: Return and Shift+Return request the next and the previous
    /// match, Return in the replace field requests a replacement, Ctrl+Shift+1 and Ctrl+Alt+Return
    /// request a replacement and a replacement of every match, Alt+C, Alt+W and Alt+R toggle the
    /// options, and Escape hides the bar.
    class HELLOUTAU_WIDGETS_EXPORT FindBar : public QFrame {
        Q_OBJECT
    public:
        /// Creates a hidden bar in \a window.
        explicit FindBar(QWidget *window);
        ~FindBar();

        /// The widget whose top right corner the bar covers, a descendant of the window. The bar
        /// follows the widget as it moves or is resized.
        QWidget *anchor() const;
        void setAnchor(QWidget *anchor);

        /// Shows the bar without the replace row, focuses the find field and selects its text.
        void showFind();

        /// Shows the bar with the replace row, focuses the find field if it is empty and the
        /// replace field otherwise, and selects the text of the focused field.
        void showReplace();

        /// Returns whether the replace row is shown.
        bool isReplaceShown() const;

        QString text() const;
        void setText(const QString &text);

        QString replacement() const;
        void setReplacement(const QString &replacement);

        bool isCaseSensitive() const;
        void setCaseSensitive(bool on);

        bool isWholeWord() const;
        void setWholeWord(bool on);

        bool isRegularExpression() const;
        void setRegularExpression(bool on);

        /// The names of the places that the window searches, such as aliases and file names,
        /// offered in a box before the find field. The box is hidden if there are fewer than two.
        QStringList scopes() const;
        void setScopes(const QStringList &scopes);

        /// The index of the chosen scope, 0 at first.
        int scope() const;
        void setScope(int scope);

        /// Whether the current scope can be replaced. The replace row is disabled otherwise.
        bool isReplaceEnabled() const;
        void setReplaceEnabled(bool enabled);

        /// Shows the result of the search beside the find field: "\a current of \a total", with
        /// a question mark if \a current is 0, or "No results" if \a total is 0. The buttons of
        /// the previous and the next match are enabled if \a total is not 0.
        void setResult(int current, int total);

        /// Shows \a message in place of the result, for an invalid query, and marks the find
        /// field as invalid. An empty message removes the mark.
        void setError(const QString &message);

        /// The text shown beside the find field.
        QString resultText() const;

        QLineEdit *findField() const;
        QLineEdit *replaceField() const;

    Q_SIGNALS:
        /// The text, an option or the scope changed.
        void queryChanged();

        void findNextRequested();
        void findPreviousRequested();

        /// The user requested the replacement of the current match.
        void replaceRequested();

        /// The user requested the replacement of every match.
        void replaceAllRequested();

        /// The bar was hidden by Escape or its close button.
        void closed();

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;
        bool event(QEvent *event) override;

    private:
        QPointer<QWidget> m_anchor;
        QToolButton *m_toggle;
        QComboBox *m_scope;
        QLineEdit *m_find;
        QToolButton *m_case;
        QToolButton *m_word;
        QToolButton *m_regex;
        QLabel *m_result;
        QToolButton *m_previous;
        QToolButton *m_next;
        QToolButton *m_close;
        QWidget *m_replaceRow;
        QLineEdit *m_replace;
        QToolButton *m_replaceOne;
        QToolButton *m_replaceAll;
        bool m_replaceEnabled = true;

        void setReplaceShown(bool shown);
        void place();
        void dismiss();
    };

}

#endif // HELLOUTAU_WIDGETS_FINDBAR_H
