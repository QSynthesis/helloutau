#include "FrequencyMapSettingPage.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/VoiceBank/FrequencyFormats.h>

#include "AppSettings.h"
#include "SettingPageWidgets_p.h"

namespace hello::daw {

    FrequencyMapSettingPage::FrequencyMapSettingPage(AppSettings &settings,
                                                     const kit::FrequencyFormats &formats,
                                                     QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_settings(settings), m_formats(formats) {
        setTitle(tr("Frequency Maps"));
        setDescription(tr("How the voice bank window generates frequency maps with the resampler, "
                          "and the analysis caches that each resampler keeps."));
        setKeywords({QStringLiteral("frequency map"), QStringLiteral("frq"), QStringLiteral("llsm"),
                     QStringLiteral("cache")});
    }

    QWidget *FrequencyMapSettingPage::createWidget() {
        auto widget = new QWidget();
        auto layout = new QVBoxLayout(widget);

        auto generationGroup = new QGroupBox(tr("Generation"), widget);
        auto generationForm = new QFormLayout(generationGroup);
        m_threads = createThreadCountBox(m_settings.frequencyMaps().threadCount());
        m_threads->setObjectName(QStringLiteral("threads"));
        generationForm->addRow(tr("Generation &threads:"), m_threads);
        generationForm->addRow(note(tr("The number of resamplers that generate frequency maps at "
                                       "once, apart from the rendering threads.")));
        m_regeneration = new QComboBox();
        m_regeneration->addItem(tr("Remove the frequency maps and their analysis caches"),
                                FrequencyMapSettings::MapsAndCaches);
        m_regeneration->addItem(tr("Remove only the analysis caches, and keep the frequency maps"),
                                FrequencyMapSettings::CachesOnly);
        m_regeneration->setCurrentIndex(
            m_regeneration->findData(m_settings.frequencyMaps().regeneration()));
        generationForm->addRow(tr("&Regenerating:"), m_regeneration);
        generationForm->addRow(
            note(tr("An analysis cache holds the frequencies of an audio file, which the resampler "
                    "reads in place of the frequency map. Removing only the caches makes the "
                    "resampler write them again from the frequency maps, for example after the "
                    "maps were edited elsewhere. Without caches, the maps are removed.")));
        layout->addWidget(generationGroup);

        auto cachesGroup = new QGroupBox(tr("Analysis Caches"), widget);
        auto cachesLayout = new QVBoxLayout(cachesGroup);
        m_caches = new QTreeWidget();
        m_caches->setRootIsDecorated(false);
        m_caches->setHeaderLabels(
            {tr("Format"), tr("Frequency Map"), tr("Cache"), tr("Cache File")});
        cachesLayout->addWidget(m_caches);
        cachesLayout->addWidget(
            note(tr("The file names are those of the audio file a.wav. Removing or regenerating "
                    "the frequency maps of a format removes these files beside the audio files.")));
        layout->addWidget(cachesGroup);
        layout->addStretch();
        fillCaches();

        connect(&m_formats, &kit::FrequencyFormats::formatsChanged, m_caches,
                [this] { fillCaches(); });
        connect(m_threads, &QComboBox::currentTextChanged, this, &SettingPage::modifiedChanged);
        connect(m_regeneration, &QComboBox::currentIndexChanged, this,
                &SettingPage::modifiedChanged);
        return widget;
    }

    void FrequencyMapSettingPage::fillCaches() {
        m_caches->clear();
        const std::filesystem::path wav(u"a.wav");
        // One row for each cache. The format and its map file are written in its first row only.
        for (const auto format : m_formats.formats()) {
            const auto map = format->mapFileNameOf(wav);
            auto first = new QTreeWidgetItem(m_caches);
            first->setText(0, format->name());
            first->setText(1, map ? QString::fromStdU16String(map->u16string()) : QString());
            const auto caches = format->analysisCaches();
            for (qsizetype i = 0; i < caches.size(); ++i) {
                const auto item = i == 0 ? first : new QTreeWidgetItem(m_caches);
                item->setText(2, caches.at(i).name);
                item->setText(3,
                              QString::fromStdU16String(caches.at(i).fileNameOf(wav).u16string()));
            }
        }
        // The widths fit the contents once, and the user resizes the columns afterwards.
        for (int column = 0; column < m_caches->columnCount(); ++column) {
            m_caches->resizeColumnToContents(column);
        }
    }

    bool FrequencyMapSettingPage::isModified() const {
        if (!m_threads || !m_regeneration) {
            return false;
        }
        return threadCountOf(m_threads) != m_settings.frequencyMaps().threadCount() ||
               m_regeneration->currentData().toInt() != m_settings.frequencyMaps().regeneration();
    }

    bool FrequencyMapSettingPage::apply(QString *error) {
        const auto threads = threadCountOf(m_threads);
        if (!threads) {
            if (error) {
                *error = tr("The number of generation threads is a positive whole number, or "
                            "Automatic.");
            }
            return false;
        }
        m_settings.frequencyMaps().setThreadCount(*threads);
        m_settings.frequencyMaps().setRegeneration(
            FrequencyMapSettings::Regeneration(m_regeneration->currentData().toInt()));
        Q_EMIT modifiedChanged();
        return true;
    }

    QComboBox *FrequencyMapSettingPage::threadCountBox() const {
        return m_threads;
    }

    QComboBox *FrequencyMapSettingPage::regenerationBox() const {
        return m_regeneration;
    }

    QTreeWidget *FrequencyMapSettingPage::cacheList() const {
        return m_caches;
    }

}
