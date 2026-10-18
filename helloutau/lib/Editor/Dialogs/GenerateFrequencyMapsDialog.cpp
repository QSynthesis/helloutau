#include "GenerateFrequencyMapsDialog.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <stdcorelib/path.h>

#include <helloutau/Widgets/FileCheckList.h>

namespace hello::daw {

    GenerateFrequencyMapsDialog::GenerateFrequencyMapsDialog(
        QList<File> files, const std::filesystem::path &root, const QString &formatName,
        const QStringList &cacheNames, FrequencyMapSettings::Regeneration regeneration,
        QWidget *parent)
        : QDialog(parent), m_files(std::move(files)), m_hasCaches(!cacheNames.isEmpty()) {
        setWindowTitle(tr("Generate Frequency Maps"));

        auto format =
            new QLabel(tr("The resampler of the settings generates the maps in the format %1.")
                           .arg(formatName));
        format->setWordWrap(true);

        // A check mark for a file present, nothing for a file missing, as in the table of the
        // window
        const auto presence = [](bool present) {
            return present ? QString(QChar(0x2713)) : QString();
        };

        // The rows are in the order of the files, and the columns of the caches follow the
        // column of the map.
        m_list = new FileCheckList(QStringList{tr("File"), tr("Frequency Map")} + cacheNames);
        for (int column = 1; column < 2 + cacheNames.size(); ++column) {
            m_list->setColumnAlignment(column, Qt::AlignCenter);
        }
        bool anySelected = false;
        for (const auto &file : std::as_const(m_files)) {
            auto relative = stdc::path::lexically_relative(file.path, root);
            if (!file.shownPath.empty()) {
                relative = file.shownPath;
            }
            QStringList texts{presence(file.hasMap)};
            for (qsizetype i = 0; i < cacheNames.size(); ++i) {
                texts.push_back(presence(file.caches.value(i)));
            }
            m_list->addRow(relative.empty() ? file.path : relative, texts);
            anySelected = anySelected || file.selected;
        }
        const auto missing = [this](int row) { return !m_files.at(row).hasMap; };
        m_checkMissing = m_list->addCheckButton(tr("Check &Missing"), missing);
        if (anySelected) {
            m_list->checkWhere([this](int row) { return m_files.at(row).selected; });
        } else {
            m_list->checkWhere(missing);
        }

        // The text of the setting. The choice applies to this generation only.
        m_removeMapsAndCachesOnGeneration = new QCheckBox(
            tr("Remove the existing &frequency maps and analysis caches before generating"));
        m_removeMapsAndCachesOnGeneration->setChecked(regeneration ==
                                                      FrequencyMapSettings::MapsAndCaches);
        m_removeMapsAndCachesOnGeneration->setVisible(m_hasCaches);
        auto removeMapsAndCachesOnGenerationNote =
            new QLabel(tr("This choice applies to this generation only. The default is set on the "
                          "Frequency Maps page of the settings."));
        removeMapsAndCachesOnGenerationNote->setWordWrap(true);
        removeMapsAndCachesOnGenerationNote->setVisible(m_hasCaches);

        m_summary = new QLabel();
        m_summary->setWordWrap(true);
        m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(format);
        layout->addWidget(m_list, 1);
        layout->addWidget(m_summary);
        layout->addWidget(m_removeMapsAndCachesOnGeneration);
        layout->addWidget(removeMapsAndCachesOnGenerationNote);
        layout->addWidget(m_buttons);
        resize(640, 560);

        connect(m_list, &FileCheckList::checkedChanged, this,
                &GenerateFrequencyMapsDialog::updateSummary);
        connect(m_removeMapsAndCachesOnGeneration, &QCheckBox::toggled, this,
                &GenerateFrequencyMapsDialog::updateSummary);
        connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        updateSummary();
    }

    GenerateFrequencyMapsDialog::~GenerateFrequencyMapsDialog() = default;

    QList<std::filesystem::path> GenerateFrequencyMapsDialog::checkedFiles() const {
        QList<std::filesystem::path> files;
        for (const int row : m_list->checkedRows()) {
            files.push_back(m_files.at(row).path);
        }
        return files;
    }

    FrequencyMapSettings::Regeneration GenerateFrequencyMapsDialog::regeneration() const {
        return m_hasCaches && !m_removeMapsAndCachesOnGeneration->isChecked()
                   ? FrequencyMapSettings::CachesOnly
                   : FrequencyMapSettings::MapsAndCaches;
    }

    FileCheckList *GenerateFrequencyMapsDialog::fileList() const {
        return m_list;
    }

    QPushButton *GenerateFrequencyMapsDialog::checkMissingButton() const {
        return m_checkMissing;
    }

    QCheckBox *GenerateFrequencyMapsDialog::removeMapsAndCachesOnGenerationBox() const {
        return m_removeMapsAndCachesOnGeneration;
    }

    QDialogButtonBox *GenerateFrequencyMapsDialog::buttonBox() const {
        return m_buttons;
    }

    void GenerateFrequencyMapsDialog::updateSummary() {
        int present = 0;
        int missing = 0;
        for (const int row : m_list->checkedRows()) {
            ++(m_files.at(row).hasMap ? present : missing);
        }
        QStringList parts;
        if (missing > 0) {
            parts.push_back(tr("%n missing frequency map(s) will be generated.", nullptr, missing));
        }
        if (present > 0) {
            parts.push_back(regeneration() == FrequencyMapSettings::CachesOnly
                                ? tr("The analysis caches of %n audio file(s) will be generated "
                                     "again from their maps.",
                                     nullptr, present)
                                : tr("%n frequency map(s) will be removed with their analysis "
                                     "caches and generated again.",
                                     nullptr, present));
        }
        m_summary->setText(parts.isEmpty() ? tr("No file is checked.")
                                           : parts.join(QLatin1Char(' ')));
        m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!parts.isEmpty());
    }

}
