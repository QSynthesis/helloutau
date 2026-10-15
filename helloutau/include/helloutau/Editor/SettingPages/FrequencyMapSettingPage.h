#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_FREQUENCYMAPSETTINGPAGE_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_FREQUENCYMAPSETTINGPAGE_H

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QCheckBox;
class QComboBox;
class QTreeWidget;

namespace hello::kit {
    class FrequencyFormats;
}

namespace hello::daw {

    class AppSettings;

    /// The generation of frequency maps in a voice bank window: the number of threads, the parts
    /// removed first when the maps are generated again, the log of the output of the resampler,
    /// and the analysis caches of each registered format, as the format declares them. See the
    /// generation in docs/FrequencyMaps.md.
    class HELLOUTAU_EDITOR_EXPORT FrequencyMapSettingPage : public SettingPage {
        Q_OBJECT
    public:
        static constexpr char pageId[] = "editor.FrequencyMaps";

        FrequencyMapSettingPage(AppSettings &settings, const kit::FrequencyFormats &formats,
                                QObject *parent = nullptr);

        bool isModified() const override;
        bool apply(QString *error) override;

        QComboBox *threadCountBox() const;
        QComboBox *regenerationBox() const;
        QCheckBox *removeCachesOnSaveBox() const;

        /// The box that makes the log of the generation follow the render log, and the boxes
        /// of its own runs and size, which are disabled while it is checked.
        QCheckBox *followRenderLogBox() const;
        QComboBox *logModeBox() const;
        QComboBox *logLimitBox() const;

        /// The analysis caches of the formats, one row per cache.
        QTreeWidget *cacheList() const;

    protected:
        QWidget *createWidget() override;

    private:
        void fillCaches();

        AppSettings &m_settings;
        const kit::FrequencyFormats &m_formats;
        QPointer<QComboBox> m_threads;
        QPointer<QComboBox> m_regeneration;
        QPointer<QCheckBox> m_removeCachesOnSave;
        QPointer<QCheckBox> m_followRenderLog;
        QPointer<QComboBox> m_logMode;
        QPointer<QComboBox> m_logLimit;
        QPointer<QTreeWidget> m_caches;
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_FREQUENCYMAPSETTINGPAGE_H
