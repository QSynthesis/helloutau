#ifndef HELLOUTAU_WIDGETS_SETTINGSDIALOG_H
#define HELLOUTAU_WIDGETS_SETTINGSDIALOG_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtGui/QPalette>
#include <QtWidgets/QDialog>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace hello::daw {

    class SettingCatalog;
    class SettingPage;

    /// The settings of the pages of a SettingCatalog, as the settings of JetBrains IDEs: a
    /// search box and the tree of pages on the left, the path of the current page, its
    /// description and the page on the right, and OK, Cancel and Apply. See the settings dialog
    /// in docs/Widgets.md.
    ///
    /// Apply is enabled while a page is modified, and the modified pages are bold in the tree;
    /// Apply and OK apply them, and a page that refuses keeps the dialog open on it. Closing
    /// releases every page, and with them the edits not applied.
    ///
    /// The search matches the title, the description and the keywords of a page, and the text
    /// of its controls: labels, buttons and group boxes. The tree keeps the pages that match and
    /// their parents, expanded, and the controls of the current page that match are marked with
    /// the highlight color.
    class HELLOUTAU_WIDGETS_EXPORT SettingsDialog : public QDialog {
        Q_OBJECT
    public:
        explicit SettingsDialog(SettingCatalog *catalog, QWidget *parent = nullptr);
        ~SettingsDialog() override;

        /// Shows the page of \a id, and returns whether there is one.
        bool selectPage(const QString &id);
        SettingPage *currentPage() const;

        /// Applies every modified page, in the order of the tree, and returns whether all
        /// could; the first that refuses is shown with its reason.
        bool apply();

        void accept() override;
        void reject() override;

        QLineEdit *searchBox() const;
        QTreeWidget *tree() const;
        QPushButton *applyButton() const;

        /// The pages that the tree shows, in its order.
        QList<SettingPage *> visiblePages() const;

        /// The controls of the current page that match the search, marked in the highlight color.
        QList<QWidget *> highlighted() const;

        /// The text shown when no page matches the search, or when the page failed to apply.
        QLabel *messageLabel() const;

    Q_SIGNALS:
        /// The modified pages were applied, by Apply or OK.
        void applied();

    private:
        SettingCatalog *m_catalog;
        QLineEdit *m_search;
        QTreeWidget *m_tree;
        QLabel *m_title;
        QLabel *m_description;
        QLabel *m_message;
        QStackedWidget *m_stack;
        QDialogButtonBox *m_buttons;
        QHash<SettingPage *, QTreeWidgetItem *> m_items;
        QPointer<QWidget> m_category;
        QList<QPair<QPointer<QWidget>, QPalette>> m_highlighted;

        void addItems(const QList<SettingPage *> &pages, QTreeWidgetItem *parent);
        SettingPage *pageOf(QTreeWidgetItem *item) const;
        void showPage(SettingPage *page);
        void updateModified();
        void search();
        void highlight();
        void releaseAll();
    };

}

#endif // HELLOUTAU_WIDGETS_SETTINGSDIALOG_H
