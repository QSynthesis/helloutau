#include "UtauSettingPage.h"

#include <QtCore/QDir>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

#include "AppSettings.h"
#include "SettingPageWidgets_p.h"

namespace hello::daw {

    UtauSettingPage::UtauSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_settings(settings) {
        setTitle(QStringLiteral("UTAU"));
        setDescription(
            tr("Where UTAU is, and how a project finds its voice bank and synth tools."));
        setKeywords({QStringLiteral("UTAU"), QStringLiteral("voice"), QStringLiteral("tools")});
    }

    QWidget *UtauSettingPage::createWidget() {
        auto widget = new QWidget();
        auto layout = new QVBoxLayout(widget);
        auto utauGroup = new QGroupBox(tr("Folder"), widget);
        auto utauForm = new QFormLayout(utauGroup);
        m_utauDirectory =
            addPathRow(utauForm, widget, tr("&UTAU folder:"),
                       QString::fromStdU16String(m_settings.utau().directory().u16string()), true);
        utauForm->addRow(
            note(tr("The folder that contains utau.exe. A synth tool that a project "
                    "names by a relative path is resolved against it, and its voice and "
                    "plugins folders are used as well.")));
        m_relativeVoiceDirInUtau = new QCheckBox(
            tr("Resolve a &relative voice bank path against the UTAU folder"), widget);
        m_relativeVoiceDirInUtau->setChecked(m_settings.voiceBanks().isRelativeToUtau());
        utauForm->addRow(m_relativeVoiceDirInUtau);
        utauForm->addRow(note(tr("UTAU resolves a relative voice bank path against its own folder. "
                                 "If this option is cleared, or if the UTAU folder does not exist, "
                                 "the path is resolved against the folder of HelloUtau.")));
        m_voiceFolders = note(QString());
        utauForm->addRow(m_voiceFolders);
        showVoiceFolders();
        connect(m_utauDirectory, &QLineEdit::textChanged, this, &SettingPage::modifiedChanged);
        connect(m_utauDirectory, &QLineEdit::textChanged, this, &UtauSettingPage::showVoiceFolders);
        connect(m_relativeVoiceDirInUtau, &QCheckBox::toggled, this, &SettingPage::modifiedChanged);
        layout->addWidget(utauGroup);
        layout->addStretch();
        return widget;
    }

    bool UtauSettingPage::isModified() const {
        if (!m_utauDirectory) {
            return false;
        }
        return pathText(m_utauDirectory) != QDir::fromNativeSeparators(QString::fromStdU16String(
                                                m_settings.utau().directory().u16string())) ||
               m_relativeVoiceDirInUtau->isChecked() != m_settings.voiceBanks().isRelativeToUtau();
    }

    void UtauSettingPage::showVoiceFolders() {
        // As VoiceBankSettings::locations() lists them for the UTAU folder being edited
        QStringList lines{tr("The voice folders that %VOICE% denotes, in decreasing priority:")};
        lines.push_back(QDir::toNativeSeparators(
            QString::fromStdU16String(m_settings.voiceBanks().folder().u16string())));
        const auto utau = pathText(m_utauDirectory);
        if (!utau.isEmpty()) {
            lines.push_back(QDir::toNativeSeparators(utau + QStringLiteral("/voice")));
        }
        m_voiceFolders->setText(lines.join(QLatin1Char('\n')));
    }

    bool UtauSettingPage::apply(QString *error) {
        Q_UNUSED(error);
        m_settings.utau().setDirectory(
            std::filesystem::path(pathText(m_utauDirectory).toStdU16String()));
        m_settings.voiceBanks().setRelativeToUtau(m_relativeVoiceDirInUtau->isChecked());
        Q_EMIT modifiedChanged();
        return true;
    }

    QLineEdit *UtauSettingPage::utauDirectoryEdit() const {
        return m_utauDirectory;
    }

    QCheckBox *UtauSettingPage::relativeVoiceDirInUtauBox() const {
        return m_relativeVoiceDirInUtau;
    }

}
