#include "FindBar.h"

#include <algorithm>

#include <QtGui/QKeyEvent>
#include <QtGui/QShortcut>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>

namespace hello::daw {

    namespace {

        // The distance of the bar from the right edge of its anchor, which keeps a vertical
        // scroll bar of the anchor uncovered, as in VS Code
        constexpr int RightMargin = 18;

        // The width of the find and the replace fields
        constexpr int FieldWidth = 220;

        QToolButton *toolButton(const QString &text, const QString &toolTip) {
            auto button = new QToolButton();
            button->setText(text);
            button->setToolTip(toolTip);
            button->setAutoRaise(true);
            button->setFocusPolicy(Qt::NoFocus);
            return button;
        }

        QToolButton *optionButton(const QString &text, const QString &toolTip,
                                  const QString &name) {
            auto button = toolButton(text, toolTip);
            button->setCheckable(true);
            button->setObjectName(name);
            return button;
        }

        bool isReturn(const QKeyEvent *event) {
            return event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
        }

    }

    FindBar::FindBar(QWidget *window) : QFrame(window) {
        setFrameShape(QFrame::StyledPanel);
        setAutoFillBackground(true);

        // The chevron beside both rows shows and hides the replace row.
        m_toggle = new QToolButton();
        m_toggle->setObjectName(QStringLiteral("toggleReplace"));
        m_toggle->setArrowType(Qt::RightArrow);
        m_toggle->setAutoRaise(true);
        m_toggle->setFocusPolicy(Qt::NoFocus);
        m_toggle->setToolTip(tr("Toggle Replace"));
        m_toggle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

        m_scope = new QComboBox();
        m_scope->setObjectName(QStringLiteral("scope"));
        m_scope->setFocusPolicy(Qt::NoFocus);
        m_scope->hide();

        m_find = new QLineEdit();
        m_find->setObjectName(QStringLiteral("find"));
        m_find->setPlaceholderText(tr("Find"));
        m_find->setMinimumWidth(FieldWidth);
        m_case = optionButton(QStringLiteral("Aa"), tr("Match Case (Alt+C)"),
                              QStringLiteral("matchCase"));
        m_word = optionButton(QStringLiteral("ab"), tr("Match Whole Word (Alt+W)"),
                              QStringLiteral("wholeWord"));
        auto wordFont = m_word->font();
        wordFont.setUnderline(true);
        m_word->setFont(wordFont);
        m_regex = optionButton(QStringLiteral(".*"), tr("Use Regular Expression (Alt+R)"),
                               QStringLiteral("regularExpression"));

        m_result = new QLabel();
        m_result->setObjectName(QStringLiteral("result"));
        m_result->setMinimumWidth(fontMetrics().horizontalAdvance(tr("No results")) + 8);
        m_previous = toolButton({}, tr("Previous Match (Shift+Enter)"));
        m_previous->setArrowType(Qt::UpArrow);
        m_previous->setObjectName(QStringLiteral("previous"));
        m_next = toolButton({}, tr("Next Match (Enter)"));
        m_next->setArrowType(Qt::DownArrow);
        m_next->setObjectName(QStringLiteral("next"));
        m_close = toolButton(QString(QChar(0x00d7)), tr("Close (Escape)"));
        m_close->setObjectName(QStringLiteral("close"));

        m_replace = new QLineEdit();
        m_replace->setObjectName(QStringLiteral("replace"));
        m_replace->setPlaceholderText(tr("Replace"));
        m_replace->setMinimumWidth(FieldWidth);
        m_replaceOne = toolButton(tr("Replace"), tr("Replace (Ctrl+Shift+1)"));
        m_replaceOne->setObjectName(QStringLiteral("replaceOne"));
        m_replaceAll = toolButton(tr("Replace All"), tr("Replace All (Ctrl+Alt+Enter)"));
        m_replaceAll->setObjectName(QStringLiteral("replaceAll"));

        auto findRow = new QHBoxLayout();
        findRow->setContentsMargins(0, 0, 0, 0);
        findRow->setSpacing(2);
        findRow->addWidget(m_scope);
        findRow->addWidget(m_find, 1);
        findRow->addWidget(m_case);
        findRow->addWidget(m_word);
        findRow->addWidget(m_regex);
        findRow->addSpacing(4);
        findRow->addWidget(m_result);
        findRow->addWidget(m_previous);
        findRow->addWidget(m_next);
        findRow->addWidget(m_close);

        m_replaceRow = new QWidget();
        auto replaceRow = new QHBoxLayout(m_replaceRow);
        replaceRow->setContentsMargins(0, 0, 0, 0);
        replaceRow->setSpacing(2);
        replaceRow->addWidget(m_replace, 1);
        replaceRow->addWidget(m_replaceOne);
        replaceRow->addWidget(m_replaceAll);
        m_replaceRow->hide();

        auto layout = new QGridLayout(this);
        layout->setContentsMargins(2, 4, 6, 4);
        layout->setHorizontalSpacing(2);
        layout->setVerticalSpacing(4);
        layout->addWidget(m_toggle, 0, 0, 2, 1);
        layout->addLayout(findRow, 0, 1);
        layout->addWidget(m_replaceRow, 1, 1);

        connect(m_find, &QLineEdit::textChanged, this, &FindBar::queryChanged);
        for (const auto button : {m_case, m_word, m_regex}) {
            connect(button, &QToolButton::toggled, this, &FindBar::queryChanged);
        }
        connect(m_scope, &QComboBox::currentIndexChanged, this, &FindBar::queryChanged);
        connect(m_toggle, &QToolButton::clicked, this, [this] {
            setReplaceShown(!isReplaceShown());
            (isReplaceShown() ? m_replace : m_find)->setFocus();
        });
        connect(m_previous, &QToolButton::clicked, this, &FindBar::findPreviousRequested);
        connect(m_next, &QToolButton::clicked, this, &FindBar::findNextRequested);
        connect(m_close, &QToolButton::clicked, this, &FindBar::dismiss);
        connect(m_replaceOne, &QToolButton::clicked, this, &FindBar::replaceRequested);
        connect(m_replaceAll, &QToolButton::clicked, this, &FindBar::replaceAllRequested);

        // The shortcuts of the options and of the replacements apply while the focus is in the bar.
        const auto shortcut = [this](const QKeySequence &key, auto slot) {
            auto result = new QShortcut(key, this);
            result->setContext(Qt::WidgetWithChildrenShortcut);
            connect(result, &QShortcut::activated, this, slot);
        };
        shortcut(QKeySequence(Qt::ALT | Qt::Key_C), [this] { m_case->toggle(); });
        shortcut(QKeySequence(Qt::ALT | Qt::Key_W), [this] { m_word->toggle(); });
        shortcut(QKeySequence(Qt::ALT | Qt::Key_R), [this] { m_regex->toggle(); });
        shortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1), [this] {
            if (m_replaceOne->isEnabled()) {
                Q_EMIT replaceRequested();
            }
        });

        m_find->installEventFilter(this);
        m_replace->installEventFilter(this);
        setResult(0, 0);
        hide();
    }

    FindBar::~FindBar() = default;

    QWidget *FindBar::anchor() const {
        return m_anchor;
    }

    void FindBar::setAnchor(QWidget *anchor) {
        if (m_anchor) {
            m_anchor->removeEventFilter(this);
        }
        m_anchor = anchor;
        if (anchor) {
            anchor->installEventFilter(this);
        }
        place();
    }

    void FindBar::showFind() {
        setReplaceShown(false);
        show();
        raise();
        place();
        m_find->setFocus(Qt::ShortcutFocusReason);
        m_find->selectAll();
    }

    void FindBar::showReplace() {
        setReplaceShown(true);
        show();
        raise();
        place();
        const auto field = m_find->text().isEmpty() ? m_find : m_replace;
        field->setFocus(Qt::ShortcutFocusReason);
        field->selectAll();
    }

    bool FindBar::isReplaceShown() const {
        return !m_replaceRow->isHidden();
    }

    QString FindBar::text() const {
        return m_find->text();
    }

    void FindBar::setText(const QString &text) {
        m_find->setText(text);
    }

    QString FindBar::replacement() const {
        return m_replace->text();
    }

    void FindBar::setReplacement(const QString &replacement) {
        m_replace->setText(replacement);
    }

    bool FindBar::isCaseSensitive() const {
        return m_case->isChecked();
    }

    void FindBar::setCaseSensitive(bool on) {
        m_case->setChecked(on);
    }

    bool FindBar::isWholeWord() const {
        return m_word->isChecked();
    }

    void FindBar::setWholeWord(bool on) {
        m_word->setChecked(on);
    }

    bool FindBar::isRegularExpression() const {
        return m_regex->isChecked();
    }

    void FindBar::setRegularExpression(bool on) {
        m_regex->setChecked(on);
    }

    QStringList FindBar::scopes() const {
        QStringList scopes;
        for (int i = 0; i < m_scope->count(); ++i) {
            scopes.push_back(m_scope->itemText(i));
        }
        return scopes;
    }

    void FindBar::setScopes(const QStringList &scopes) {
        const QSignalBlocker blocker(m_scope);
        m_scope->clear();
        m_scope->addItems(scopes);
        m_scope->setVisible(scopes.size() > 1);
    }

    int FindBar::scope() const {
        return std::max(0, m_scope->currentIndex());
    }

    void FindBar::setScope(int scope) {
        m_scope->setCurrentIndex(scope);
    }

    bool FindBar::isReplaceEnabled() const {
        return m_replaceEnabled;
    }

    void FindBar::setReplaceEnabled(bool enabled) {
        m_replaceEnabled = enabled;
        m_replaceRow->setEnabled(enabled);
    }

    void FindBar::setResult(int current, int total) {
        m_find->setProperty("invalid", false);
        m_find->setToolTip({});
        if (m_find->text().isEmpty()) {
            m_result->clear();
        } else if (total == 0) {
            m_result->setText(tr("No results"));
        } else {
            m_result->setText(tr("%1 of %2")
                                  .arg(current > 0 ? QString::number(current) : QStringLiteral("?"))
                                  .arg(total));
        }
        m_previous->setEnabled(total > 0);
        m_next->setEnabled(total > 0);
        style()->unpolish(m_find);
        style()->polish(m_find);
    }

    void FindBar::setError(const QString &message) {
        if (message.isEmpty()) {
            setResult(0, 0);
            return;
        }
        m_result->setText(tr("Invalid"));
        m_find->setProperty("invalid", true);
        m_find->setToolTip(message);
        m_previous->setEnabled(false);
        m_next->setEnabled(false);
        style()->unpolish(m_find);
        style()->polish(m_find);
    }

    QString FindBar::resultText() const {
        return m_result->text();
    }

    QLineEdit *FindBar::findField() const {
        return m_find;
    }

    QLineEdit *FindBar::replaceField() const {
        return m_replace;
    }

    bool FindBar::eventFilter(QObject *watched, QEvent *event) {
        if (watched == m_anchor) {
            if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
                place();
            }
            return QFrame::eventFilter(watched, event);
        }
        if (event->type() != QEvent::KeyPress) {
            return QFrame::eventFilter(watched, event);
        }
        const auto key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            dismiss();
            return true;
        }
        if (!isReturn(key)) {
            return QFrame::eventFilter(watched, event);
        }
        const auto modifiers = key->modifiers() & ~Qt::KeypadModifier;
        if (modifiers == (Qt::ControlModifier | Qt::AltModifier)) {
            if (m_replaceEnabled) {
                Q_EMIT replaceAllRequested();
            }
        } else if (watched == m_replace) {
            if (m_replaceEnabled) {
                Q_EMIT replaceRequested();
            }
        } else if (modifiers & Qt::ShiftModifier) {
            Q_EMIT findPreviousRequested();
        } else {
            Q_EMIT findNextRequested();
        }
        return true;
    }

    bool FindBar::event(QEvent *event) {
        if (event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest) {
            const bool result = QFrame::event(event);
            place();
            return result;
        }
        return QFrame::event(event);
    }

    void FindBar::setReplaceShown(bool shown) {
        m_replaceRow->setVisible(shown);
        m_toggle->setArrowType(shown ? Qt::DownArrow : Qt::RightArrow);
    }

    void FindBar::place() {
        const auto window = parentWidget();
        if (!window || !isVisible()) {
            return;
        }
        QRect area = window->rect();
        if (m_anchor && m_anchor->isVisible()) {
            area = QRect(m_anchor->mapTo(window, QPoint(0, 0)), m_anchor->size());
        }
        const auto size = sizeHint();
        const int width = std::min(size.width(), area.width());
        const int right = area.right() - (area.width() > width + RightMargin ? RightMargin : 0);
        setGeometry(right - width + 1, area.top(), width, size.height());
    }

    void FindBar::dismiss() {
        if (isHidden()) {
            return;
        }
        const bool focused = isAncestorOf(QApplication::focusWidget());
        hide();
        if (focused && m_anchor) {
            m_anchor->setFocus(Qt::OtherFocusReason);
        }
        Q_EMIT closed();
    }

}
