#include "FrequencyMapSettingPage.h"

#include <QtWidgets/QCheckBox>
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

    namespace {

        FrequencyMapSettings::Regeneration regenerationOf(const QCheckBox *removeMaps) {
            return removeMaps->isChecked() ? FrequencyMapSettings::MapsAndCaches
                                           : FrequencyMapSettings::CachesOnly;
        }

    }

    FrequencyMapSettingPage::FrequencyMapSettingPage(AppSettings &settings,
                                                     const kit::FrequencyFormats &formats,
                                                     QObject *parent)
        : SettingPage(QLatin1String(pageId), parent), m_settings(settings), m_formats(formats) {
        setTitle(tr("Frequency Maps"));
        setDescription(tr("The generation of frequency maps with the resampler and their editing "
                          "in the voice bank window, and the analysis caches that each resampler "
                          "keeps."));
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
        m_removeMapsOnRegeneration =
            new QCheckBox(tr("Also remove the &frequency maps when regenerating"));
        m_removeMapsOnRegeneration->setChecked(m_settings.frequencyMaps().regeneration() ==
                                               FrequencyMapSettings::MapsAndCaches);
        generationForm->addRow(m_removeMapsOnRegeneration);
        generationForm->addRow(
            note(tr("An analysis cache holds the frequencies of an audio file, which the resampler "
                    "reads in place of the frequency map. If the box is unchecked, only the caches "
                    "are removed, and the resampler writes them again from the frequency maps, for "
                    "example after the maps were edited elsewhere. The maps of a format without "
                    "caches are removed regardless.")));
        layout->addWidget(generationGroup);

        auto editingGroup = new QGroupBox(tr("Editing"), widget);
        auto editingForm = new QFormLayout(editingGroup);
        m_removeCachesOnSave =
            new QCheckBox(tr("Re&move the analysis caches when saving a frequency map"));
        m_removeCachesOnSave->setChecked(m_settings.frequencyMaps().removesCachesOnSave());
        editingForm->addRow(m_removeCachesOnSave);
        editingForm->addRow(
            note(tr("The resampler reads the analysis cache in place of the frequency map, "
                    "therefore an edit takes effect in synthesis only after the cache is "
                    "removed. The editing dialog can change this choice while it is open.")));
        layout->addWidget(editingGroup);

        auto logGroup = new QGroupBox(tr("Generation Log"), widget);
        auto logForm = new QFormLayout(logGroup);
        const auto &maps = m_settings.frequencyMaps();
        m_followRenderLog = new QCheckBox(tr("&Use the settings of the render log"));
        m_followRenderLog->setChecked(maps.followsRenderLog());
        logForm->addRow(m_followRenderLog);
        m_logMode = createLogModeBox(maps.isLogAccumulated());
        logForm->addRow(tr("Generation &log:"), m_logMode);
        m_logLimit = createLogLimitBox(maps.logLimit());
        logForm->addRow(tr("Generation log si&ze:"), m_logLimit);
        logForm->addRow(note(tr("The output of the resampler during a generation, shown by Tools "
                                "> View Frequency Map Log in the voice bank window. The render "
                                "log is set on the page Audio Rendering.")));
        const auto updateLog = [this] {
            const bool own = !m_followRenderLog->isChecked();
            m_logMode->setEnabled(own);
            m_logLimit->setEnabled(own);
        };
        updateLog();
        layout->addWidget(logGroup);

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
        connect(m_removeMapsOnRegeneration, &QCheckBox::toggled, this,
                &SettingPage::modifiedChanged);
        connect(m_removeCachesOnSave, &QCheckBox::toggled, this, &SettingPage::modifiedChanged);
        connect(m_followRenderLog, &QCheckBox::toggled, this, updateLog);
        connect(m_followRenderLog, &QCheckBox::toggled, this, &SettingPage::modifiedChanged);
        connect(m_logMode, &QComboBox::currentIndexChanged, this, &SettingPage::modifiedChanged);
        connect(m_logLimit, &QComboBox::editTextChanged, this, &SettingPage::modifiedChanged);
        connect(m_logLimit, &QComboBox::currentTextChanged, this, &SettingPage::modifiedChanged);
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
        if (!m_threads || !m_removeMapsOnRegeneration || !m_followRenderLog) {
            return false;
        }
        const auto &maps = m_settings.frequencyMaps();
        return threadCountOf(m_threads) != maps.threadCount() ||
               regenerationOf(m_removeMapsOnRegeneration) != maps.regeneration() ||
               m_removeCachesOnSave->isChecked() != maps.removesCachesOnSave() ||
               m_followRenderLog->isChecked() != maps.followsRenderLog() ||
               m_logMode->currentData().toBool() != maps.isLogAccumulated() ||
               logLimitOf(m_logLimit) != maps.logLimit();
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
        const auto logLimit = logLimitOf(m_logLimit);
        if (!logLimit) {
            if (error) {
                *error = tr("The generation log size must be between 1024 bytes and 1 GiB.");
            }
            return false;
        }
        auto &maps = m_settings.frequencyMaps();
        maps.setThreadCount(*threads);
        maps.setRegeneration(regenerationOf(m_removeMapsOnRegeneration));
        maps.setRemovesCachesOnSave(m_removeCachesOnSave->isChecked());
        maps.setFollowsRenderLog(m_followRenderLog->isChecked());
        maps.setLogAccumulated(m_logMode->currentData().toBool());
        maps.setLogLimit(*logLimit);
        Q_EMIT modifiedChanged();
        return true;
    }

    QComboBox *FrequencyMapSettingPage::threadCountBox() const {
        return m_threads;
    }

    QCheckBox *FrequencyMapSettingPage::removeMapsOnRegenerationBox() const {
        return m_removeMapsOnRegeneration;
    }

    QCheckBox *FrequencyMapSettingPage::removeCachesOnSaveBox() const {
        return m_removeCachesOnSave;
    }

    QCheckBox *FrequencyMapSettingPage::followRenderLogBox() const {
        return m_followRenderLog;
    }

    QComboBox *FrequencyMapSettingPage::logModeBox() const {
        return m_logMode;
    }

    QComboBox *FrequencyMapSettingPage::logLimitBox() const {
        return m_logLimit;
    }

    QTreeWidget *FrequencyMapSettingPage::cacheList() const {
        return m_caches;
    }

}
