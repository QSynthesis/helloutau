#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_FREQUENCYMAPSETTINGPAGE_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_FREQUENCYMAPSETTINGPAGE_H

#include <QtCore/QPointer>

#include <helloutau/Widgets/SettingPage.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;
class QTreeWidget;

namespace hello::kit {
    class FrequencyFormats;
}

namespace hello::daw {

    class AppSettings;

    /// The generation of frequency maps in a voice bank window: the number of threads, what
    /// generating the maps again removes first, and the analysis caches of each registered
    /// format, as the format declares them. See the generation in docs/FrequencyMaps.md.
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

        /// The analysis caches of the formats, one row per format.
        QTreeWidget *cacheList() const;

    protected:
        QWidget *createWidget() override;

    private:
        void fillCaches();

        AppSettings &m_settings;
        const kit::FrequencyFormats &m_formats;
        QPointer<QComboBox> m_threads;
        QPointer<QComboBox> m_regeneration;
        QPointer<QTreeWidget> m_caches;
    };

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_FREQUENCYMAPSETTINGPAGE_H
