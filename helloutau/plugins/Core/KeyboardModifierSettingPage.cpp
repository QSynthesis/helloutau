#include "KeyboardModifierSettingPage.h"

#include <QtCore/QSignalBlocker>
#include <QtGui/QKeySequence>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>

#include <helloutau/Widgets/FormLayout.h>

namespace hello::daw {

    namespace {

        // A kind of window by its name, in the order of the page
        struct WindowName {
            Editor::WindowKind kind;
            const char *name;
        };

        const WindowName windowNames[] = {
            {Editor::ProjectWindowKind,
             QT_TRANSLATE_NOOP("hello::daw::KeyboardModifierSettingPage", "Project Window")      },
            {Editor::VoiceBankWindowKind,
             QT_TRANSLATE_NOOP("hello::daw::KeyboardModifierSettingPage", "Voice Bank Window")   },
            {Editor::FrequencyMapEditorKind,
             QT_TRANSLATE_NOOP("hello::daw::KeyboardModifierSettingPage", "Frequency Map Editor")},
        };

        const Qt::KeyboardModifiers modifierOptions[] = {
            Qt::NoModifier,
            Qt::ControlModifier,
            Qt::AltModifier,
            Qt::ShiftModifier,
#ifndef Q_OS_WIN
            Qt::MetaModifier,
#endif
            Qt::ControlModifier | Qt::AltModifier,
            Qt::ControlModifier | Qt::ShiftModifier,
            Qt::AltModifier | Qt::ShiftModifier,
            Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier,
        };

        // The data of the option that turns a role off, which no combination of modifiers has
        constexpr int offData = -1;

        // A box of modifiers that ignores the wheel, which then scrolls the tab, so that a
        // scroll over the boxes does not change a binding
        class WheelIgnoringComboBox : public QComboBox {
        public:
            using QComboBox::QComboBox;

        protected:
            void wheelEvent(QWheelEvent *event) override {
                event->ignore();
            }
        };

        // Returns the name of an option of modifiers as the platform writes it, such as Ctrl+Alt,
        // or the symbols of the keys on macOS, where Qt::ControlModifier is the Command key.
        QString keyboardModifierText(Qt::KeyboardModifiers modifiers) {
            if (modifiers == Qt::NoModifier) {
                return KeyboardModifierSettingPage::tr("None");
            }
            // Without a key, the text ends with the separator on the platforms that write one.
            auto text = QKeySequence(QKeyCombination(modifiers, Qt::Key(0)))
                            .toString(QKeySequence::NativeText);
            if (text.endsWith(QLatin1Char('+'))) {
                text.chop(1);
            }
            return text;
        }

    }

    KeyboardModifierSettingPage::KeyboardModifierSettingPage(Editor *editor, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_editor(editor) {
        setTitle(tr("Modifier Keys"));
        setDescription(tr("The modifier keys of the mouse and wheel operations."));
        setKeywords({QStringLiteral("modifier"), QStringLiteral("mouse"), QStringLiteral("wheel"),
                     QStringLiteral("drag")});
    }

    bool KeyboardModifierSettingPage::isModified() const {
        if (!m_restore) {
            return false;
        }
        for (const auto kind : Editor::windowKinds) {
            if (m_modifiers[kind] != m_editor->keyboardModifierBindings(kind)) {
                return true;
            }
        }
        return false;
    }

    bool KeyboardModifierSettingPage::apply(QString *error) {
        const auto conflicting = conflictDescriptions();
        if (!conflicting.isEmpty()) {
            if (error) {
                *error = tr("Some modifier keys conflict:") + QLatin1Char('\n') +
                         conflicting.join(QLatin1Char('\n'));
            }
            return false;
        }
        for (const auto kind : Editor::windowKinds) {
            m_editor->setKeyboardModifierBindings(kind, m_modifiers[kind]);
        }
        if (!m_editor->saveKeymap(error)) {
            return false;
        }
        Q_EMIT modifiedChanged();
        return true;
    }

    void KeyboardModifierSettingPage::restoreDefaults() {
        for (auto &list : m_modifiers) {
            for (auto &bindings : list) {
                bindings = KeyboardModifierBindings(bindings.scheme());
            }
        }
        updateWidgets();
        Q_EMIT modifiedChanged();
    }

    QStringList KeyboardModifierSettingPage::conflictDescriptions() const {
        QStringList lines;
        for (const auto &window : windowNames) {
            for (const auto &bindings : m_modifiers[window.kind]) {
                const auto &scheme = bindings.scheme();
                for (const auto &conflict : bindings.conflicts()) {
                    lines.push_back(conflict.second ? tr("%1, %2: %3 and %4")
                                                          .arg(tr(window.name), scheme.name(),
                                                               scheme.roleName(conflict.first),
                                                               scheme.roleName(*conflict.second))
                                                    : tr("%1, %2: %3 cannot act without modifiers")
                                                          .arg(tr(window.name), scheme.name(),
                                                               scheme.roleName(conflict.first)));
                }
            }
        }
        return lines;
    }

    QWidget *KeyboardModifierSettingPage::conflictBanner() const {
        return m_conflictBanner;
    }

    void KeyboardModifierSettingPage::updateWidgets() {
        bool conflictOf[std::size(Editor::windowKinds)] = {};
        for (const auto &item : std::as_const(m_boxes)) {
            if (!item.box) {
                continue;
            }
            const auto &bindings = m_modifiers[item.kind].at(item.scheme);
            const auto modifiers = bindings.modifiers(item.role);
            const QSignalBlocker blocker(item.box);
            item.box->setCurrentIndex(item.box->findData(modifiers ? modifiers->toInt() : offData));
            QStringList lines;
            for (const auto &conflict : bindings.conflicts()) {
                if (conflict.first != item.role && conflict.second != item.role) {
                    continue;
                }
                lines.push_back(
                    conflict.second
                        ? tr("Conflicts with %1.")
                              .arg(bindings.scheme().roleName(
                                  conflict.first == item.role ? *conflict.second : conflict.first))
                        : tr("Cannot act without modifiers."));
            }
            item.mark->setVisible(!lines.isEmpty());
            item.mark->setToolTip(lines.join(QLatin1Char('\n')));
            conflictOf[item.kind] = conflictOf[item.kind] || !lines.isEmpty();
        }
        if (m_tabs) {
            const auto warning = m_tabs->style()->standardIcon(QStyle::SP_MessageBoxWarning);
            bool any = false;
            for (const auto kind : Editor::windowKinds) {
                if (m_tabIndex[kind] >= 0) {
                    m_tabs->setTabIcon(m_tabIndex[kind], conflictOf[kind] ? warning : QIcon());
                }
                any = any || conflictOf[kind];
            }
            m_conflictBanner->setVisible(any);
        }
        if (m_restore) {
            bool changed = false;
            for (const auto &list : m_modifiers) {
                for (const auto &bindings : list) {
                    changed = changed || bindings != KeyboardModifierBindings(bindings.scheme());
                }
            }
            m_restore->setEnabled(changed);
        }
    }

    QWidget *KeyboardModifierSettingPage::createSchemeGroup(Editor::WindowKind kind, int scheme) {
        const auto &bindings = m_modifiers[kind].at(scheme);
        auto group = new QGroupBox(bindings.scheme().name());
        auto form = new FormLayout(group);
        for (const auto &role : bindings.scheme().roles()) {
            auto box = new WheelIgnoringComboBox();
            box->setObjectName(QStringLiteral("%1/%2/%3")
                                   .arg(Editor::nameOf(kind), bindings.scheme().key(),
                                        QLatin1String(role.key)));
            box->addItem(tr("Off"), offData);
            // Only a role that starts an operation acts without modifiers.
            const bool none = bindings.scheme().isStart(role.id);
            for (int i = 0; i < int(std::size(modifierOptions)); ++i) {
                if (modifierOptions[i] != Qt::NoModifier || none) {
                    box->addItem(keyboardModifierText(modifierOptions[i]),
                                 modifierOptions[i].toInt());
                }
            }
            connect(box, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [this, kind, scheme, id = role.id, box](int option) {
                        if (option < 0) {
                            return;
                        }
                        const int data = box->itemData(option).toInt();
                        m_modifiers[kind][scheme].setModifiers(
                            id, data == offData
                                    ? std::nullopt
                                    : std::optional(Qt::KeyboardModifiers::fromInt(data)));
                        updateWidgets();
                        Q_EMIT modifiedChanged();
                    });
            // The warning icon beside the box, shown by updateWidgets() while the role conflicts
            auto mark = new QLabel();
            mark->setObjectName(box->objectName() + QStringLiteral("/conflict"));
            const auto size = group->style()->pixelMetric(QStyle::PM_SmallIconSize);
            mark->setPixmap(
                group->style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(size, size));
            mark->hide();
            auto field = new QHBoxLayout();
            field->addWidget(box, 1);
            field->addWidget(mark);
            m_boxes.push_back({kind, scheme, role.id, box, mark});
            form->addRow(bindings.scheme().roleName(role.id), field);
        }
        return group;
    }

    QWidget *KeyboardModifierSettingPage::createWidget() {
        for (const auto kind : Editor::windowKinds) {
            m_modifiers[kind] = m_editor->keyboardModifierBindings(kind);
        }
        auto widget = new QWidget();
        auto layout = new QHBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);

        // A tab for each kind of window with modifiers, which scrolls its groups, as the keymap
        // scrolls its commands
        m_boxes.clear();
        auto tabs = new QTabWidget();
        tabs->setObjectName(QStringLiteral("windows"));
        m_tabs = tabs;
        for (auto &index : m_tabIndex) {
            index = -1;
        }
        for (const auto &window : windowNames) {
            if (m_modifiers[window.kind].isEmpty()) {
                continue;
            }
            m_tabIndex[window.kind] = tabs->count();
            auto groups = new QWidget();
            auto groupLayout = new QVBoxLayout(groups);
            for (int scheme = 0; scheme < m_modifiers[window.kind].size(); ++scheme) {
                groupLayout->addWidget(createSchemeGroup(window.kind, scheme));
            }
            groupLayout->addStretch();
            auto scroll = new QScrollArea();
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scroll->setWidget(groups);
            tabs->addTab(scroll, tr(window.name));
        }

        m_restore = new QPushButton(tr("Restore &Defaults"));
        m_restore->setObjectName(QStringLiteral("restoreDefaults"));
        connect(m_restore, &QPushButton::clicked, this, [this] { restoreDefaults(); });
        auto buttonColumn = new QVBoxLayout();
        buttonColumn->addStretch();
        buttonColumn->addWidget(m_restore);

        m_conflictBanner = new QWidget();
        m_conflictBanner->setObjectName(QStringLiteral("conflictBanner"));
        {
            const auto size = widget->style()->pixelMetric(QStyle::PM_SmallIconSize);
            auto icon = new QLabel();
            icon->setPixmap(
                widget->style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(size, size));
            auto text = new QLabel(tr("Some modifier keys conflict. The boxes and their tabs are "
                                      "marked with the warning icon, whose tool tip names the "
                                      "other role. The modifier keys cannot be applied until the "
                                      "conflicts are resolved."));
            text->setWordWrap(true);
            auto bannerLayout = new QHBoxLayout(m_conflictBanner);
            bannerLayout->setContentsMargins(0, 0, 0, 0);
            bannerLayout->addWidget(icon, 0, Qt::AlignTop);
            bannerLayout->addWidget(text, 1);
        }
        auto tabColumn = new QVBoxLayout();
        tabColumn->addWidget(m_conflictBanner);
        tabColumn->addWidget(tabs, 1);

        layout->addLayout(tabColumn, 1);
        layout->addLayout(buttonColumn);

        updateWidgets();
        return widget;
    }

}
