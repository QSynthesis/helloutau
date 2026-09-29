#include "SettingPage.h"

#include <functional>

namespace hello::daw {

    SettingPage::SettingPage(const QString &id, QObject *parent) : QObject(parent), m_id(id) {
    }

    SettingPage::~SettingPage() {
        release();
    }

    QString SettingPage::id() const {
        return m_id;
    }

    QString SettingPage::title() const {
        return m_title;
    }

    void SettingPage::setTitle(const QString &title) {
        m_title = title;
    }

    QString SettingPage::description() const {
        return m_description;
    }

    void SettingPage::setDescription(const QString &description) {
        m_description = description;
    }

    QStringList SettingPage::keywords() const {
        return m_keywords;
    }

    void SettingPage::setKeywords(const QStringList &keywords) {
        m_keywords = keywords;
    }

    void SettingPage::addPage(SettingPage *page) {
        page->setParent(this);
        m_pages.push_back(page);
    }

    QList<SettingPage *> SettingPage::pages() const {
        return m_pages;
    }

    SettingPage *SettingPage::parentPage() const {
        return qobject_cast<SettingPage *>(parent());
    }

    QWidget *SettingPage::widget() {
        if (!m_created) {
            m_created = true;
            m_widget = createWidget();
        }
        return m_widget;
    }

    bool SettingPage::hasWidget() const {
        return bool(m_widget);
    }

    void SettingPage::release() {
        const bool had = bool(m_widget);
        delete m_widget;
        m_widget = nullptr;
        m_created = false;
        if (had) {
            Q_EMIT modifiedChanged();
        }
    }

    bool SettingPage::isModified() const {
        return false;
    }

    bool SettingPage::apply(QString *error) {
        Q_UNUSED(error);
        return true;
    }

    QWidget *SettingPage::createWidget() {
        return nullptr;
    }

    SettingCatalog::SettingCatalog(QObject *parent) : QObject(parent) {
    }

    SettingCatalog::~SettingCatalog() = default;

    void SettingCatalog::addPage(SettingPage *page) {
        page->setParent(this);
        m_pages.push_back(page);
    }

    QList<SettingPage *> SettingCatalog::pages() const {
        return m_pages;
    }

    SettingPage *SettingCatalog::page(const QString &id) const {
        for (const auto page : allPages()) {
            if (page->id() == id) {
                return page;
            }
        }
        return nullptr;
    }

    QList<SettingPage *> SettingCatalog::allPages() const {
        QList<SettingPage *> all;
        const std::function<void(const QList<SettingPage *> &)> visit =
            [&](const QList<SettingPage *> &pages) {
                for (const auto page : pages) {
                    all.push_back(page);
                    visit(page->pages());
                }
            };
        visit(m_pages);
        return all;
    }

}
