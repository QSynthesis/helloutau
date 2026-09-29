#include "EditorSettingPages_p.h"

#include <QtCore/QDir>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

#include "AppSettings.h"
#include "ExportUstDialog.h"

namespace hello::daw {

    namespace {

        QString pathText(const QLineEdit *edit) {
            return QDir::fromNativeSeparators(edit->text().trimmed());
        }

        // A row with a line edit and a button that browses for a directory or a file
        QLineEdit *addPathRow(QFormLayout *form, QWidget *owner, const QString &label,
                              const QString &text, bool directory) {
            auto edit = new QLineEdit(QDir::toNativeSeparators(text));
            auto button = new QPushButton(SettingPage::tr("Browse..."));
            QObject::connect(button, &QPushButton::clicked, owner, [owner, edit, directory] {
                const auto chosen =
                    directory
                        ? QFileDialog::getExistingDirectory(owner, SettingPage::tr("Choose Folder"),
                                                            edit->text())
                        : QFileDialog::getOpenFileName(owner, SettingPage::tr("Choose Program"),
                                                       edit->text());
                if (!chosen.isEmpty()) {
                    edit->setText(QDir::toNativeSeparators(chosen));
                }
            });
            auto layout = new QHBoxLayout();
            layout->addWidget(edit, 1);
            layout->addWidget(button);
            form->addRow(label, layout);
            return edit;
        }

        QLabel *note(const QString &text) {
            auto label = new QLabel(text);
            label->setWordWrap(true);
            return label;
        }

    }

    GeneralSettingPage::GeneralSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QStringLiteral("editor.General"), parent), m_settings(settings) {
        setTitle(tr("General"));
        setDescription(tr("Where UTAU is, and how projects are exported."));
        setKeywords({QStringLiteral("General"), QStringLiteral("UTAU")});
    }

    QWidget *GeneralSettingPage::createWidget() {
        auto widget = new QWidget();
        auto form = new QFormLayout(widget);
        m_utauDirectory =
            addPathRow(form, widget, tr("&UTAU folder:"),
                       QString::fromStdU16String(m_settings.utauDirectory().u16string()), true);
        form->addRow(note(tr("Resolves the voice banks of projects that name them relative to "
                             "UTAU, such as %VOICE%.")));
        m_ustExportCharset = new QComboBox();
        m_ustExportCharset->addItems(ExportUstDialog::charsets());
        const int index = m_ustExportCharset->findText(m_settings.ustExportCharset());
        m_ustExportCharset->setCurrentIndex(index >= 0 ? index : 0);
        form->addRow(tr("UST &export encoding:"), m_ustExportCharset);

        connect(m_utauDirectory, &QLineEdit::textChanged, this, &SettingPage::modifiedChanged);
        connect(m_ustExportCharset, &QComboBox::currentTextChanged, this,
                &SettingPage::modifiedChanged);
        return widget;
    }

    bool GeneralSettingPage::isModified() const {
        if (!m_utauDirectory || !m_ustExportCharset) {
            return false;
        }
        return pathText(m_utauDirectory) != QDir::fromNativeSeparators(QString::fromStdU16String(
                                                m_settings.utauDirectory().u16string())) ||
               m_ustExportCharset->currentText() != m_settings.ustExportCharset();
    }

    bool GeneralSettingPage::apply(QString *error) {
        Q_UNUSED(error);
        m_settings.setUtauDirectory(
            std::filesystem::path(pathText(m_utauDirectory).toStdU16String()));
        m_settings.setUstExportCharset(m_ustExportCharset->currentText());
        Q_EMIT modifiedChanged();
        return true;
    }

    QLineEdit *GeneralSettingPage::utauDirectoryEdit() const {
        return m_utauDirectory;
    }

    QComboBox *GeneralSettingPage::ustExportCharsetBox() const {
        return m_ustExportCharset;
    }

    RenderingSettingPage::RenderingSettingPage(AppSettings &settings, QObject *parent)
        : SettingPage(QStringLiteral("editor.Rendering"), parent), m_settings(settings) {
        setTitle(tr("Rendering"));
        setDescription(tr("The engines that render, and how playback renders."));
        setKeywords({QStringLiteral("Rendering"), QStringLiteral("resampler"),
                     QStringLiteral("wavtool"), QStringLiteral("playback")});
    }

    QWidget *RenderingSettingPage::createWidget() {
        auto widget = new QWidget();
        auto form = new QFormLayout(widget);
        m_resampler = addPathRow(form, widget, tr("&Resampler:"), m_settings.resampler(), false);
        m_wavtool = addPathRow(form, widget, tr("&Wavtool:"), m_settings.wavtool(), false);
        form->addRow(note(tr("A project renders with these engines, not with those it names.")));

        m_playbackMode = new QComboBox();
        m_playbackMode->addItem(tr("Prerender, by temp.bat in a console as UTAU does"),
                                AppSettings::Prerender);
        m_playbackMode->addItem(tr("Realtime, rendered in the background from the playhead"),
                                AppSettings::Realtime);
        m_playbackMode->setCurrentIndex(m_playbackMode->findData(m_settings.playbackMode()));
        form->addRow(tr("&Playback:"), m_playbackMode);
        form->addRow(note(tr("Realtime playback joins the notes as wavtool.exe does, whichever "
                             "wavtool is chosen.")));

        connect(m_resampler, &QLineEdit::textChanged, this, &SettingPage::modifiedChanged);
        connect(m_wavtool, &QLineEdit::textChanged, this, &SettingPage::modifiedChanged);
        connect(m_playbackMode, &QComboBox::currentIndexChanged, this,
                &SettingPage::modifiedChanged);
        return widget;
    }

    bool RenderingSettingPage::isModified() const {
        if (!m_resampler || !m_wavtool || !m_playbackMode) {
            return false;
        }
        return pathText(m_resampler) != QDir::fromNativeSeparators(m_settings.resampler()) ||
               pathText(m_wavtool) != QDir::fromNativeSeparators(m_settings.wavtool()) ||
               m_playbackMode->currentData().toInt() != m_settings.playbackMode();
    }

    bool RenderingSettingPage::apply(QString *error) {
        Q_UNUSED(error);
        m_settings.setResampler(pathText(m_resampler));
        m_settings.setWavtool(pathText(m_wavtool));
        m_settings.setPlaybackMode(
            AppSettings::PlaybackMode(m_playbackMode->currentData().toInt()));
        Q_EMIT modifiedChanged();
        return true;
    }

    QLineEdit *RenderingSettingPage::resamplerEdit() const {
        return m_resampler;
    }

    QLineEdit *RenderingSettingPage::wavtoolEdit() const {
        return m_wavtool;
    }

    QComboBox *RenderingSettingPage::playbackModeBox() const {
        return m_playbackMode;
    }

    void addEditorSettingPages(SettingCatalog *catalog, AppSettings &settings) {
        catalog->addPage(new GeneralSettingPage(settings));
        catalog->addPage(new RenderingSettingPage(settings));
    }

}
