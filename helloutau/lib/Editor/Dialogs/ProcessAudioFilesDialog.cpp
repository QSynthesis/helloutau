#include "ProcessAudioFilesDialog.h"

#include <QtCore/QDir>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>

#include <helloutau/Widgets/FileCheckList.h>

namespace hello::daw {

    namespace {

        using Processor = kit::AudioFileProcessor;

        // The column of the changes, after the file and the format
        constexpr int changeColumn = 2;

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        QString formatOf(const Processor::Inspection &inspection) {
            const auto bits =
                inspection.floatingPoint
                    ? ProcessAudioFilesDialog::tr("%1-bit float").arg(inspection.bitsPerSample)
                    : ProcessAudioFilesDialog::tr("%1-bit").arg(inspection.bitsPerSample);
            return ProcessAudioFilesDialog::tr("%1 Hz, %n channel(s), %2", nullptr,
                                               inspection.channels)
                .arg(inspection.sampleRate)
                .arg(bits);
        }

        QString changesOf(Processor::Changes changes) {
            const std::pair<Processor::Change, QString> names[] = {
                {Processor::SampleRateChange, ProcessAudioFilesDialog::tr("sample rate")},
                {Processor::ChannelChange,    ProcessAudioFilesDialog::tr("channels")   },
                {Processor::BitDepthChange,   ProcessAudioFilesDialog::tr("bit depth")  },
                {Processor::MetadataChange,   ProcessAudioFilesDialog::tr("metadata")   },
                {Processor::HeaderChange,     ProcessAudioFilesDialog::tr("header")     },
                {Processor::DataSizeChange,   ProcessAudioFilesDialog::tr("data size")  },
                {Processor::DcOffsetChange,   ProcessAudioFilesDialog::tr("DC offset")  },
                {Processor::PeakChange,       ProcessAudioFilesDialog::tr("peak level") },
            };
            QStringList parts;
            for (const auto &[change, name] : names) {
                if (changes & change) {
                    parts.push_back(name);
                }
            }
            return parts.isEmpty() ? ProcessAudioFilesDialog::tr("No change")
                                   : parts.join(ProcessAudioFilesDialog::tr(", "));
        }

    }

    ProcessAudioFilesDialog::ProcessAudioFilesDialog(QList<File> files,
                                                     const std::filesystem::path &root,
                                                     const std::filesystem::path &backup,
                                                     QWidget *parent)
        : QDialog(parent), m_files(std::move(files)), m_root(root) {
        setWindowTitle(tr("Process Audio Files"));

        m_resampling = new QCheckBox(tr("Convert the &sample rate to"));
        m_sampleRate = new QComboBox();
        for (const int rate : {22050, 44100, 48000, 96000}) {
            m_sampleRate->addItem(tr("%1 Hz").arg(rate), rate);
        }
        m_mono = new QCheckBox(tr("Make &one channel from"));
        m_channelMix = new QComboBox();
        m_channelMix->addItem(tr("the average of the channels"), Processor::AverageChannels);
        m_channelMix->addItem(tr("the left channel"), Processor::FirstChannel);
        m_channelMix->addItem(tr("the right channel"), Processor::SecondChannel);
        m_sixteenBits = new QCheckBox(tr("Write 16-bit &integer samples"));
        m_dithered = new QCheckBox(tr("Add &dither to rounded samples"));
        m_removingMetadata = new QCheckBox(tr("Remove &metadata"));
        m_plainHeader = new QCheckBox(tr("Write a &plain header"));
        m_removingDcOffset = new QCheckBox(tr("Remove the D&C offset"));
        m_scalingPeak = new QCheckBox(tr("Scale the pea&k to"));
        m_peakLevel = new QDoubleSpinBox();
        m_peakLevel->setRange(-30, 0);
        m_peakLevel->setDecimals(1);
        m_peakLevel->setSingleStep(0.5);
        m_peakLevel->setSuffix(tr(" dBFS"));

        auto grid = new QGridLayout();
        grid->addWidget(m_resampling, 0, 0);
        grid->addWidget(m_sampleRate, 0, 1);
        grid->addWidget(m_mono, 1, 0);
        grid->addWidget(m_channelMix, 1, 1);
        grid->addWidget(m_sixteenBits, 2, 0);
        grid->addWidget(m_dithered, 2, 1);
        grid->addWidget(m_removingMetadata, 3, 0);
        grid->addWidget(m_plainHeader, 3, 1);
        grid->addWidget(m_removingDcOffset, 4, 0, 1, 2);
        grid->addWidget(m_scalingPeak, 5, 0);
        grid->addWidget(m_peakLevel, 5, 1);
        grid->setColumnStretch(2, 1);
        auto processing = new QGroupBox(tr("Processing"));
        processing->setLayout(grid);

        // The rows are in the order of the files.
        m_list = new FileCheckList({tr("File"), tr("Format"), tr("Changes")});
        for (const auto &file : m_files) {
            auto relative = file.path.lexically_relative(m_root);
            if (!file.shownPath.empty()) {
                relative = file.shownPath;
            }
            m_list->addRow(relative.empty() ? file.path : relative,
                           {formatOf(file.inspection), QString()});
            m_anySelected = m_anySelected || file.selected;
        }
        m_checkFiles = m_list->addCheckButton(tr("Check Files to &Process"), [this](int row) {
            return bool(m_files.at(row).inspection.changes(options()));
        });

        m_removingMaps = new QCheckBox(
            tr("&Remove the frequency maps and analysis caches of the processed files"));
        m_removingMaps->setChecked(true);
        m_backingUp = new QCheckBox(tr("&Back up the original files to"));
        m_backingUp->setChecked(true);
        m_backupFolder = new QLineEdit(QDir::toNativeSeparators(textOf(backup)));
        auto browse = new QPushButton(tr("Bro&wse..."));
        auto backupRow = new QHBoxLayout();
        backupRow->addWidget(m_backingUp);
        backupRow->addWidget(m_backupFolder, 1);
        backupRow->addWidget(browse);

        m_summary = new QLabel();
        m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

        // The files are on the left, where the list has the full height, and the options on the
        // right, in the panes of a splitter.
        auto right = new QWidget();
        auto rightLayout = new QVBoxLayout(right);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->addWidget(processing);
        rightLayout->addWidget(m_removingMaps);
        rightLayout->addLayout(backupRow);
        rightLayout->addStretch();
        m_splitter = new QSplitter(Qt::Horizontal);
        m_splitter->setChildrenCollapsible(false);
        m_splitter->addWidget(m_list);
        m_splitter->addWidget(right);
        m_splitter->setStretchFactor(0, 3);
        m_splitter->setStretchFactor(1, 2);
        auto layout = new QVBoxLayout(this);
        layout->addWidget(m_splitter, 1);
        layout->addWidget(m_summary);
        layout->addWidget(m_buttons);
        resize(1000, 560);

        for (const auto box : {m_resampling, m_mono, m_sixteenBits, m_dithered, m_removingMetadata,
                               m_plainHeader, m_removingDcOffset, m_scalingPeak}) {
            connect(box, &QCheckBox::toggled, this, &ProcessAudioFilesDialog::refreshFiles);
        }
        for (const auto box : {m_sampleRate, m_channelMix}) {
            connect(box, &QComboBox::currentIndexChanged, this,
                    &ProcessAudioFilesDialog::refreshFiles);
        }
        connect(m_peakLevel, &QDoubleSpinBox::valueChanged, this,
                &ProcessAudioFilesDialog::refreshFiles);
        connect(m_list, &FileCheckList::checkedChanged, this, [this] {
            if (!m_applying) {
                updateSummary();
            }
        });
        connect(m_backingUp, &QCheckBox::toggled, this, [this, browse](bool on) {
            m_backupFolder->setEnabled(on);
            browse->setEnabled(on);
            updateSummary();
        });
        connect(m_backupFolder, &QLineEdit::textChanged, this,
                &ProcessAudioFilesDialog::updateSummary);
        connect(browse, &QPushButton::clicked, this, [this] {
            const auto folder = QFileDialog::getExistingDirectory(this, tr("Backup Folder"),
                                                                  m_backupFolder->text());
            if (!folder.isEmpty()) {
                m_backupFolder->setText(QDir::toNativeSeparators(folder));
            }
        });
        connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        setOptions({});
        if (m_anySelected) {
            m_list->checkWhere([this](int row) { return m_files.at(row).selected; });
        }
    }

    ProcessAudioFilesDialog::~ProcessAudioFilesDialog() = default;

    kit::AudioFileProcessor::Options ProcessAudioFilesDialog::options() const {
        Processor::Options options;
        options.sampleRate = m_resampling->isChecked()
                                 ? std::optional(m_sampleRate->currentData().toInt())
                                 : std::nullopt;
        options.mono =
            m_mono->isChecked()
                ? std::optional(Processor::ChannelMix(m_channelMix->currentData().toInt()))
                : std::nullopt;
        options.sixteenBits = m_sixteenBits->isChecked();
        options.dithered = m_dithered->isChecked();
        options.removingMetadata = m_removingMetadata->isChecked();
        options.plainHeader = m_plainHeader->isChecked();
        options.removingDcOffset = m_removingDcOffset->isChecked();
        options.peakLevel =
            m_scalingPeak->isChecked() ? std::optional(m_peakLevel->value()) : std::nullopt;
        return options;
    }

    void ProcessAudioFilesDialog::setOptions(const kit::AudioFileProcessor::Options &options) {
        m_applying = true;
        m_resampling->setChecked(options.sampleRate.has_value());
        if (options.sampleRate) {
            const int index = m_sampleRate->findData(*options.sampleRate);
            if (index < 0) {
                m_sampleRate->addItem(tr("%1 Hz").arg(*options.sampleRate), *options.sampleRate);
            }
            m_sampleRate->setCurrentIndex(m_sampleRate->findData(*options.sampleRate));
        } else {
            m_sampleRate->setCurrentIndex(m_sampleRate->findData(44100));
        }
        m_mono->setChecked(options.mono.has_value());
        m_channelMix->setCurrentIndex(
            m_channelMix->findData(options.mono.value_or(Processor::AverageChannels)));
        m_sixteenBits->setChecked(options.sixteenBits);
        m_dithered->setChecked(options.dithered);
        m_removingMetadata->setChecked(options.removingMetadata);
        m_plainHeader->setChecked(options.plainHeader);
        m_removingDcOffset->setChecked(options.removingDcOffset);
        m_scalingPeak->setChecked(options.peakLevel.has_value());
        m_peakLevel->setValue(options.peakLevel.value_or(-1));
        m_applying = false;
        refreshFiles();
    }

    QList<std::filesystem::path> ProcessAudioFilesDialog::checkedFiles() const {
        QList<std::filesystem::path> files;
        for (const int row : m_list->checkedRows()) {
            files.push_back(m_files.at(row).path);
        }
        return files;
    }

    bool ProcessAudioFilesDialog::removesFrequencyMaps() const {
        return m_removingMaps->isChecked();
    }

    std::optional<std::filesystem::path> ProcessAudioFilesDialog::backupFolder() const {
        if (!m_backingUp->isChecked()) {
            return std::nullopt;
        }
        return std::filesystem::path(
            QDir::fromNativeSeparators(m_backupFolder->text().trimmed()).toStdU16String());
    }

    FileCheckList *ProcessAudioFilesDialog::fileList() const {
        return m_list;
    }

    QPushButton *ProcessAudioFilesDialog::checkFilesButton() const {
        return m_checkFiles;
    }

    QSplitter *ProcessAudioFilesDialog::splitter() const {
        return m_splitter;
    }

    QLineEdit *ProcessAudioFilesDialog::backupFolderEdit() const {
        return m_backupFolder;
    }

    QDialogButtonBox *ProcessAudioFilesDialog::buttonBox() const {
        return m_buttons;
    }

    void ProcessAudioFilesDialog::refreshFiles() {
        if (m_applying) {
            return;
        }
        m_sampleRate->setEnabled(m_resampling->isChecked());
        m_channelMix->setEnabled(m_mono->isChecked());
        m_peakLevel->setEnabled(m_scalingPeak->isChecked());
        const auto current = options();
        m_applying = true;
        for (int row = 0; row < m_list->rowCount(); ++row) {
            m_list->setText(row, changeColumn,
                            changesOf(m_files.at(row).inspection.changes(current)));
        }
        if (!m_anySelected) {
            m_list->checkWhere([this, &current](int row) {
                return bool(m_files.at(row).inspection.changes(current));
            });
        }
        m_applying = false;
        updateSummary();
    }

    void ProcessAudioFilesDialog::updateSummary() {
        const auto count = checkedFiles().size();
        m_summary->setText(tr("%n file(s) will be processed.", nullptr, int(count)));
        const bool folder =
            !m_backingUp->isChecked() || !m_backupFolder->text().trimmed().isEmpty();
        m_buttons->button(QDialogButtonBox::Ok)->setEnabled(count > 0 && folder);
    }

}
