#include "KeyboardModifierSettingPage.h"

#include <QtCore/QSignalBlocker>
#include <QtGui/QKeySequence>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>

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
        for (const auto &list : m_modifiers) {
            for (const auto &bindings : list) {
                if (!bindings.isValid()) {
                    if (error) {
                        *error = tr("Modifier bindings conflict with each other.");
                    }
                    return false;
                }
            }
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

    void KeyboardModifierSettingPage::updateWidgets() {
        for (const auto &item : std::as_const(m_boxes)) {
            if (!item.box) {
                continue;
            }
            const auto modifiers = m_modifiers[item.kind].at(item.scheme).modifiers(item.role);
            const QSignalBlocker blocker(item.box);
            item.box->setCurrentIndex(item.box->findData(modifiers ? modifiers->toInt() : offData));
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
        auto form = new QFormLayout(group);
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
            m_boxes.push_back({kind, scheme, role.id, box});
            form->addRow(bindings.scheme().roleName(role.id), box);
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
        for (const auto &window : windowNames) {
            if (m_modifiers[window.kind].isEmpty()) {
                continue;
            }
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

        layout->addWidget(tabs, 1);
        layout->addLayout(buttonColumn);

        updateWidgets();
        return widget;
    }

}
