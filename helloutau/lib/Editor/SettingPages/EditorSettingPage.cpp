#include "EditorSettingPage.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>

#include "AppSettings.h"
#include "ExportUstDialog.h"

namespace hello::daw {

    EditorSettingPage::EditorSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_settings(settings) {
        setTitle(tr("Editor"));
        setDescription(tr("The editing and export of projects."));
        setKeywords({QStringLiteral("Editor"), QStringLiteral("UST")});
    }

    QWidget *EditorSettingPage::createWidget() {
        auto widget = new QWidget();
        auto form = new QFormLayout(widget);
        m_ustExportCharset = new QComboBox();
        m_ustExportCharset->addItems(ExportUstDialog::charsets());
        const int index = m_ustExportCharset->findText(m_settings.files().ustExportCharset());
        m_ustExportCharset->setCurrentIndex(index >= 0 ? index : 0);
        form->addRow(tr("Default UST &export encoding:"), m_ustExportCharset);
        connect(m_ustExportCharset, &QComboBox::currentTextChanged, this,
                &SettingPage::modifiedChanged);
        return widget;
    }

    bool EditorSettingPage::isModified() const {
        return m_ustExportCharset &&
               m_ustExportCharset->currentText() != m_settings.files().ustExportCharset();
    }

    bool EditorSettingPage::apply(QString *error) {
        Q_UNUSED(error);
        m_settings.files().setUstExportCharset(m_ustExportCharset->currentText());
        Q_EMIT modifiedChanged();
        return true;
    }

    QComboBox *EditorSettingPage::ustExportCharsetBox() const {
        return m_ustExportCharset;
    }

}
