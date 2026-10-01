#ifndef HELLOUTAU_WIDGETS_SETTINGPAGE_H
#define HELLOUTAU_WIDGETS_SETTINGPAGE_H

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtWidgets/QWidget>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// A page of the settings dialog, and a node of the tree of pages: it has an id, a title, a
    /// description and child pages. See the settings dialog in docs/Widgets.md.
    ///
    /// Its widget is created by createWidget() when the page is first shown, from the current
    /// settings, and deleted by release() when the dialog closes, so that each dialog starts
    /// from the settings as they are. A page without a widget is a category, which the dialog
    /// shows as the links to its child pages.
    ///
    /// The widget only edits: the page reports whether it differs from the settings with
    /// isModified() and modifiedChanged(), and commits it with apply(), which may refuse.
    class HELLOUTAU_WIDGETS_EXPORT SettingPage : public QObject {
        Q_OBJECT
    public:
        explicit SettingPage(const QString &id, QObject *parent = nullptr);
        ~SettingPage() override;

        QString id() const;

        QString title() const;
        void setTitle(const QString &title);

        QString description() const;
        void setDescription(const QString &description);

        /// Words by which the search finds the page besides its title, its description and
        /// the text of its controls, such as the English names of a translated page.
        QStringList keywords() const;
        void setKeywords(const QStringList &keywords);

        /// Adds \a page as a child, which this page then owns: before the child of the id
        /// \a before, or last if \a before is empty or names no child.
        void addPage(SettingPage *page, const QString &before = {});
        QList<SettingPage *> pages() const;
        SettingPage *parentPage() const;

        /// The widget, created on the first call, or null for a category.
        QWidget *widget();

        /// Whether the widget exists, which apply() and isModified() concern.
        bool hasWidget() const;

        /// Deletes the widget, and with it the edits not applied.
        void release();

        /// Whether the widget differs from the settings. False by default, and for a page
        /// without a widget.
        virtual bool isModified() const;

        /// Commits the widget to the settings, and returns whether it could, with the reason in
        /// \a error otherwise. Called only while the widget exists. True by default.
        virtual bool apply(QString *error);

    Q_SIGNALS:
        /// isModified() may have changed.
        void modifiedChanged();

    protected:
        /// Creates the widget from the current settings, or returns null for a category.
        virtual QWidget *createWidget();

    private:
        QString m_id;
        QString m_title;
        QString m_description;
        QStringList m_keywords;
        QList<SettingPage *> m_pages;
        QPointer<QWidget> m_widget;
        bool m_created = false;
    };

    /// The pages of the settings dialog: the top-level pages, and a page found by id at any
    /// level. The editor registers its pages at start, and a plugin would add its own.
    class HELLOUTAU_WIDGETS_EXPORT SettingCatalog : public QObject {
        Q_OBJECT
    public:
        explicit SettingCatalog(QObject *parent = nullptr);
        ~SettingCatalog() override;

        /// Adds \a page at the top level, which the catalog then owns: before the top-level page
        /// of the id \a before, or last if \a before is empty or names no top-level page. A plugin
        /// that adds its page after the pages of the editor places it with \a before in the order
        /// of the settings of JetBrains IDEs.
        void addPage(SettingPage *page, const QString &before = {});
        QList<SettingPage *> pages() const;

        /// The page of \a id at any level, or null. Ids are to be unique.
        SettingPage *page(const QString &id) const;

        /// Every page, parents before their children, in order.
        QList<SettingPage *> allPages() const;

    private:
        QList<SettingPage *> m_pages;
    };

}

#endif // HELLOUTAU_WIDGETS_SETTINGPAGE_H
