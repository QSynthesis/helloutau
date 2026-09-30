#include "CommandPalette.h"

#include <algorithm>

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QTextLayout>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    namespace {

        enum Role {
            IdRole = Qt::UserRole,
            SubtitleRole,
            ShortcutRole,
            PositionsRole,
            StateRole,
            RecentRole,
            DescriptionRole,
            DescriptionPositionsRole,
        };

        // The palette takes half the width and half the height of its window, within limits.
        constexpr int MinimumWidth = 450;
        constexpr int MaximumWidth = 750;
        constexpr int MinimumHeight = 400;

        constexpr int ItemPadding = 6;
        constexpr int KeyCapPadding = 4;
        constexpr int KeyCapSpacing = 3;
        constexpr int KeyCapRadius = 3;
        constexpr int GroupSpacing = 12;

        class EntryDelegate : public QStyledItemDelegate {
        public:
            explicit EntryDelegate(CommandPalette *palette)
                : QStyledItemDelegate(palette), m_palette(palette) {
            }

            QSize sizeHint(const QStyleOptionViewItem &option,
                           const QModelIndex &index) const override {
                const QFontMetrics metrics(option.font);
                const int lines = index.data(SubtitleRole).toString().isEmpty() ? 1 : 2;
                return {0, metrics.height() * lines + 2 * ItemPadding};
            }

            void paint(QPainter *painter, const QStyleOptionViewItem &option,
                       const QModelIndex &index) const override {
                // The background comes from the style, but not the selection, whose colors the
                // palette gives together with those of the text drawn here.
                const bool selected = option.state & QStyle::State_Selected;
                QStyleOptionViewItem background(option);
                initStyleOption(&background, index);
                background.text.clear();
                background.state &= ~QStyle::State_Selected;
                const auto style = option.widget ? option.widget->style() : QApplication::style();
                style->drawControl(QStyle::CE_ItemViewItem, &background, painter, option.widget);
                if (selected) {
                    painter->save();
                    painter->setRenderHint(QPainter::Antialiasing);
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(m_palette->highlightColor());
                    painter->drawRoundedRect(QRectF(option.rect).adjusted(1, 1, -1, -1),
                                             KeyCapRadius, KeyCapRadius);
                    painter->restore();
                }

                const auto textColor = selected ? m_palette->highlightedTextColor()
                                                : option.palette.color(QPalette::Text);
                const QFontMetrics metrics(option.font);
                const auto area =
                    option.rect.adjusted(ItemPadding, ItemPadding, -ItemPadding, -ItemPadding);

                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                painter->setFont(option.font);

                // From the right: the remove button, the key caps, then the recently used mark.
                int right = area.right();
                const bool hovered = option.state & QStyle::State_MouseOver;
                if (m_palette->isRemovable() && (selected || hovered)) {
                    const auto button = CommandPalette::removeButtonRect(option.rect, metrics);
                    painter->setPen(textColor);
                    painter->drawText(button, Qt::AlignCenter, QString(QChar(0x00d7)));
                    right = button.left() - GroupSpacing;
                }
                const auto caps =
                    CommandPalette::keyParts(index.data(ShortcutRole).value<QKeySequence>());
                for (auto it = caps.crbegin(); it != caps.crend(); ++it) {
                    if (it->kind == CommandPalette::KeyPart::Gap) {
                        right -= KeyCapSpacing * 2;
                        continue;
                    }
                    const bool cap = it->kind == CommandPalette::KeyPart::Cap;
                    const int width =
                        metrics.horizontalAdvance(it->text) + (cap ? 2 * KeyCapPadding : 0);
                    const QRect rect(right - width + 1, area.top(), width, metrics.height());
                    if (cap) {
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(m_palette->keyCapColor());
                        painter->drawRoundedRect(rect, KeyCapRadius, KeyCapRadius);
                    }
                    painter->setPen(textColor);
                    painter->drawText(rect, Qt::AlignCenter, it->text);
                    right -= width + KeyCapSpacing;
                }
                if (index.data(RecentRole).toBool()) {
                    const auto mark = CommandPalette::tr("recently used");
                    const int width = metrics.horizontalAdvance(mark);
                    right -= caps.isEmpty() ? 0 : GroupSpacing - KeyCapSpacing;
                    painter->setPen(selected ? textColor : m_palette->matchColor());
                    painter->drawText(QRect(right - width + 1, area.top(), width, metrics.height()),
                                      Qt::AlignVCenter, mark);
                    right -= width;
                }
                const int textRight = right - GroupSpacing;

                // The label, with the typed characters in the match color, the state of a
                // checkable command after it, and the description after both in the subtitle
                // color.
                const auto label = index.data(Qt::DisplayRole).toString();
                const auto state = index.data(StateRole).toString();
                const auto description = index.data(DescriptionRole).toString();
                auto text = state.isEmpty() ? label : label + u' ' + state;
                QList<QTextLayout::FormatRange> formats;
                const auto mark = [&](const QVariantList &positions, qsizetype offset) {
                    for (const auto &position : positions) {
                        QTextLayout::FormatRange range;
                        range.start = int(offset) + position.toInt();
                        range.length = 1;
                        range.format.setForeground(selected ? textColor : m_palette->matchColor());
                        range.format.setFontWeight(QFont::Bold);
                        formats.push_back(range);
                    }
                };
                if (!description.isEmpty()) {
                    text += QStringLiteral("  ");
                    QTextLayout::FormatRange range;
                    range.start = int(text.size());
                    range.length = int(description.size());
                    range.format.setForeground(selected ? textColor : m_palette->subtitleColor());
                    formats.push_back(range);
                    mark(index.data(DescriptionPositionsRole).toList(), text.size());
                    text += description;
                }
                mark(index.data(PositionsRole).toList(), 0);
                QTextLayout layout(text, option.font);
                layout.setFormats(formats);
                layout.beginLayout();
                auto line = layout.createLine();
                line.setLineWidth(std::max(0, textRight - area.left()));
                layout.endLayout();
                painter->setPen(textColor);
                painter->setClipRect(
                    QRect(area.left(), area.top(), textRight - area.left(), area.height()));
                layout.draw(painter, area.topLeft());

                const auto subtitle = index.data(SubtitleRole).toString();
                if (!subtitle.isEmpty()) {
                    painter->setPen(selected ? textColor : m_palette->subtitleColor());
                    painter->drawText(QRect(area.left(), area.top() + metrics.height(),
                                            textRight - area.left(), metrics.height()),
                                      Qt::AlignVCenter, subtitle);
                }
                painter->restore();
            }

        private:
            CommandPalette *m_palette;
        };

        // A click on a menu, such as the popup of a combo box, does not count as a click
        // outside the palette.
        bool isMenu(QObject *object) {
            return qobject_cast<QMenu *>(object) ||
                   (object->inherits("QWidgetWindow") &&
                    object->objectName() == QStringLiteral("QMenuClassWindow"));
        }

    }

    CommandPalette::CommandPalette(QWidget *window) : QFrame(window) {
        // Before a style sheet reaches the shadow property
        ThemeTypes::registerConversions();
        setFrameShape(QFrame::StyledPanel);
        setAutoFillBackground(true);

        m_input = new QLineEdit();
        m_input->setPlaceholderText(tr("Type a command"));

        m_list = new QListWidget();
        m_list->setUniformItemSizes(false);
        m_list->setFocusPolicy(Qt::NoFocus);
        m_list->setItemDelegate(new EntryDelegate(this));
        // The item under the pointer shows its remove button.
        m_list->setMouseTracking(true);
        m_list->viewport()->setAttribute(Qt::WA_Hover);

        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(6, 6, 6, 6);
        layout->setSpacing(6);
        layout->addWidget(m_input);
        layout->addWidget(m_list);

        connect(m_input, &QLineEdit::textChanged, this, &CommandPalette::updateList);
        connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
            m_list->setCurrentItem(item);
            activateCurrent();
        });
        connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
            if (isVisible() && (!now || !isAncestorOf(now))) {
                dismiss();
            }
        });

        hide();
    }

    CommandPalette::~CommandPalette() = default;

    QList<CommandEntry> CommandPalette::commands() const {
        return m_commands;
    }

    void CommandPalette::setCommands(const QList<CommandEntry> &commands) {
        m_commands = commands;
        updateList();
    }

    QStringList CommandPalette::recentIds() const {
        return m_recentIds;
    }

    void CommandPalette::setRecentIds(const QStringList &ids) {
        m_recentIds = ids;
        updateList();
    }

    QString CommandPalette::placeholderText() const {
        return m_input->placeholderText();
    }

    void CommandPalette::setPlaceholderText(const QString &text) {
        m_input->setPlaceholderText(text);
    }

    CommandMatcher::Order CommandPalette::order() const {
        return m_order;
    }

    void CommandPalette::setOrder(CommandMatcher::Order order) {
        m_order = order;
        updateList();
    }

    bool CommandPalette::isRemovable() const {
        return m_removable;
    }

    void CommandPalette::setRemovable(bool removable) {
        m_removable = removable;
        m_list->viewport()->update();
    }

    QList<CommandPalette::KeyPart> CommandPalette::keyParts(const QKeySequence &shortcut) {
        QList<KeyPart> parts;
        for (int i = 0; i < shortcut.count(); ++i) {
            if (i > 0) {
                parts.push_back({KeyPart::Gap, {}});
            }
            const auto combination = shortcut[i];
            const auto whole = QKeySequence(combination).toString(QKeySequence::NativeText);
            const auto key = QKeySequence(combination.key()).toString(QKeySequence::NativeText);
            // The modifiers precede the key, joined by "+" where the platform writes them so,
            // and as one symbol each on macOS.
            const auto modifiers = whole.left(whole.size() - key.size());
            if (modifiers.contains(u'+')) {
                for (const auto &part : modifiers.split(u'+', Qt::SkipEmptyParts)) {
                    parts.push_back({KeyPart::Cap, part});
                    parts.push_back({KeyPart::Plus, QStringLiteral("+")});
                }
            } else {
                for (const auto symbol : modifiers) {
                    parts.push_back({KeyPart::Cap, QString(symbol)});
                }
            }
            parts.push_back({KeyPart::Cap, key});
        }
        return parts;
    }

    QRect CommandPalette::removeButtonRect(const QRect &rect, const QFontMetrics &metrics) {
        const int size = metrics.height();
        return {rect.right() - ItemPadding - size + 1, rect.top() + ItemPadding, size, size};
    }

    void CommandPalette::popup() {
        m_previousFocus = QApplication::focusWidget();
        m_input->clear();
        updateList();
        place();
        show();
        raise();
        m_input->setFocus();
    }

    QString CommandPalette::query() const {
        return m_input->text();
    }

    void CommandPalette::setQuery(const QString &query) {
        m_input->setText(query);
    }

    QStringList CommandPalette::shownIds() const {
        QStringList ids;
        for (int i = 0; i < m_list->count(); ++i) {
            ids.push_back(m_list->item(i)->data(IdRole).toString());
        }
        return ids;
    }

    QString CommandPalette::currentId() const {
        const auto item = m_list->currentItem();
        return item ? item->data(IdRole).toString() : QString();
    }

    QColor CommandPalette::matchColor() const {
        return m_matchColor.isValid() ? m_matchColor : palette().color(QPalette::Link);
    }

    void CommandPalette::setMatchColor(const QColor &color) {
        m_matchColor = color;
        m_list->viewport()->update();
    }

    QColor CommandPalette::subtitleColor() const {
        return m_subtitleColor.isValid() ? m_subtitleColor
                                         : palette().color(QPalette::PlaceholderText);
    }

    void CommandPalette::setSubtitleColor(const QColor &color) {
        m_subtitleColor = color;
        m_list->viewport()->update();
    }

    ThemeShadow CommandPalette::shadow() const {
        return m_shadow;
    }

    void CommandPalette::setShadow(const ThemeShadow &shadow) {
        m_shadow = shadow;
        if (!shadow.isVisible()) {
            setGraphicsEffect(nullptr);
            return;
        }
        auto effect = qobject_cast<QGraphicsDropShadowEffect *>(graphicsEffect());
        if (!effect) {
            effect = new QGraphicsDropShadowEffect(this);
            setGraphicsEffect(effect);
        }
        effect->setColor(shadow.color);
        effect->setBlurRadius(shadow.blur);
        effect->setOffset(shadow.offset);
    }

    QColor CommandPalette::keyCapColor() const {
        if (m_keyCapColor.isValid()) {
            return m_keyCapColor;
        }
        auto color = palette().color(QPalette::Text);
        color.setAlphaF(0.12f);
        return color;
    }

    void CommandPalette::setKeyCapColor(const QColor &color) {
        m_keyCapColor = color;
        m_list->viewport()->update();
    }

    QColor CommandPalette::highlightColor() const {
        return m_highlightColor.isValid() ? m_highlightColor : palette().color(QPalette::Highlight);
    }

    void CommandPalette::setHighlightColor(const QColor &color) {
        m_highlightColor = color;
        m_list->viewport()->update();
    }

    QColor CommandPalette::highlightedTextColor() const {
        return m_highlightedTextColor.isValid() ? m_highlightedTextColor
                                                : palette().color(QPalette::HighlightedText);
    }

    void CommandPalette::setHighlightedTextColor(const QColor &color) {
        m_highlightedTextColor = color;
        m_list->viewport()->update();
    }

    bool CommandPalette::eventFilter(QObject *watched, QEvent *event) {
        // Installed on the application while the palette is shown, and on the window.
        if (watched == parentWidget() && event->type() == QEvent::Resize) {
            place();
            return QFrame::eventFilter(watched, event);
        }

        switch (event->type()) {
            case QEvent::MouseButtonPress: {
                const auto position = static_cast<QMouseEvent *>(event)->globalPosition().toPoint();
                if (m_removable && watched == m_list->viewport()) {
                    const auto local = m_list->viewport()->mapFromGlobal(position);
                    const auto item = m_list->itemAt(local);
                    if (item && removeButtonRect(m_list->visualItemRect(item), m_list->fontMetrics())
                                    .contains(local)) {
                        remove(item->data(IdRole).toString());
                        return true;
                    }
                }
                if (!rect().contains(mapFromGlobal(position)) && !isMenu(watched)) {
                    dismiss();
                }
                break;
            }
            case QEvent::Shortcut:
                // The shortcut still runs its command, after the palette is gone.
                dismiss();
                break;
            case QEvent::KeyPress: {
                if (watched != m_input) {
                    break;
                }
                switch (static_cast<QKeyEvent *>(event)->key()) {
                    case Qt::Key_Up:
                    case Qt::Key_Down:
                    case Qt::Key_PageUp:
                    case Qt::Key_PageDown:
                        // The list moves its selection while the typing stays in the input.
                        QApplication::sendEvent(m_list, event);
                        return true;
                    case Qt::Key_Return:
                    case Qt::Key_Enter:
                        activateCurrent();
                        return true;
                    case Qt::Key_Escape:
                        dismiss();
                        return true;
                    default:
                        break;
                }
                break;
            }
            default:
                break;
        }
        return QFrame::eventFilter(watched, event);
    }

    bool CommandPalette::event(QEvent *event) {
        switch (event->type()) {
            case QEvent::Show:
                qApp->installEventFilter(this);
                if (parentWidget()) {
                    parentWidget()->installEventFilter(this);
                }
                break;
            case QEvent::Hide:
                qApp->removeEventFilter(this);
                if (parentWidget()) {
                    parentWidget()->removeEventFilter(this);
                }
                break;
            default:
                break;
        }
        return QFrame::event(event);
    }

    void CommandPalette::updateList() {
        m_list->clear();

        auto ranked = CommandMatcher::rank(m_input->text(), m_commands, m_order);
        int recentCount = 0;
        if (m_input->text().isEmpty()) {
            // The recently used commands first, the latest first, the rest as ranked
            const auto recency = [this](const CommandMatcher::Ranked &entry) {
                const auto at = m_recentIds.indexOf(m_commands[entry.index].id);
                return at < 0 ? m_recentIds.size() : at;
            };
            std::stable_sort(
                ranked.begin(), ranked.end(),
                [&recency](const auto &a, const auto &b) { return recency(a) < recency(b); });
            recentCount = int(std::count_if(ranked.begin(), ranked.end(), [&](const auto &entry) {
                return recency(entry) < m_recentIds.size();
            }));
        }

        for (int i = 0; i < ranked.size(); ++i) {
            const auto &entry = m_commands[ranked[i].index];
            auto item = new QListWidgetItem(entry.label, m_list);
            item->setData(IdRole, entry.id);
            item->setData(SubtitleRole, entry.alternative);
            item->setData(ShortcutRole, entry.shortcut);
            QVariantList positions;
            for (const auto position : ranked[i].positions) {
                positions.push_back(int(position));
            }
            item->setData(PositionsRole, positions);
            item->setData(DescriptionRole, entry.description);
            QVariantList descriptionPositions;
            for (const auto position : ranked[i].descriptionPositions) {
                descriptionPositions.push_back(int(position));
            }
            item->setData(DescriptionPositionsRole, descriptionPositions);
            if (entry.checkable) {
                // What running the command does, as in QSynthesis Revenge
                item->setData(StateRole, entry.checked ? tr("(Off)") : tr("(On)"));
            }
            item->setData(RecentRole, i == 0 && recentCount > 0);
        }
        if (m_list->count() > 0) {
            m_list->setCurrentRow(0);
        }
    }

    void CommandPalette::place() {
        const auto window = parentWidget();

        // Below the menu bar and the tool bars of a main window, over its content
        int top = 0;
        if (const auto mainWindow = qobject_cast<QMainWindow *>(window);
            mainWindow && mainWindow->centralWidget()) {
            top = mainWindow->centralWidget()->y();
        }
        const int available = window->height() - top;
        const int width =
            std::min(std::clamp(window->width() / 2, MinimumWidth, MaximumWidth), window->width());
        const int height = std::min(std::max(available / 2, MinimumHeight), available);
        setGeometry((window->width() - width) / 2, top, width, height);
    }

    void CommandPalette::activateCurrent() {
        const auto id = currentId();
        if (id.isEmpty()) {
            return;
        }
        dismiss();
        Q_EMIT commandActivated(id);
    }

    void CommandPalette::remove(const QString &id) {
        const int row = m_list->currentRow();
        m_commands.removeIf([&id](const CommandEntry &entry) { return entry.id == id; });
        updateList();
        m_list->setCurrentRow(std::min(row, m_list->count() - 1));
        Q_EMIT commandRemoved(id);
    }

    void CommandPalette::dismiss() {
        if (!isVisible()) {
            return;
        }
        hide();
        if (m_previousFocus) {
            m_previousFocus->setFocus();
        }
    }

}
