#include "RepairFrequencyMapsDialog.h"

#include <optional>

#include <QtCore/QDir>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>

#include <stdcorelib/path.h>

#include <helloutau/Widgets/CheckAllBox.h>

namespace hello::daw {

    namespace fs = std::filesystem;

    namespace {

        // Returns the path of name in folder relative to root, or the whole path if it has no path
        // relative to root, with native separators.
        QString displayed(const fs::path &root, const fs::path &folder, const fs::path &name) {
            const auto relative = stdc::path::lexically_relative(folder / name, root);
            const auto path = relative.empty() ? folder / name : relative;
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        // Returns the path of file in folder named as the constructor describes, or an empty path
        // if shownPathOf restores nothing. The derived name is used only if format derives the
        // name on disk from the name of the audio file on disk, as a rename of the audio file
        // keeps it.
        fs::path restoredPathOf(const kit::FrequencyFormat &format,
                                const RepairFrequencyMapsDialog::ShownPath &shownPathOf,
                                const fs::path &folder,
                                const kit::FrequencyFormats::MapFile &file) {
            if (!shownPathOf) {
                return {};
            }
            if (file.wavs.size() == 1) {
                const auto &wav = file.wavs.front();
                const auto shownWav = shownPathOf(folder / wav);
                const auto caches = format.analysisCaches();
                const auto derive = [&](const fs::path &audio) -> std::optional<fs::path> {
                    if (file.cache < 0) {
                        return format.sidecarNameOf(audio);
                    }
                    if (file.cache < caches.size()) {
                        return caches.at(file.cache).fileNameOf(audio);
                    }
                    return std::nullopt;
                };
                const auto onDisk = derive(wav);
                const auto restored = derive(shownWav.filename());
                if (!shownWav.empty() && onDisk && restored && *onDisk == file.path) {
                    return (shownWav.parent_path() / *restored).lexically_normal();
                }
            }
            const auto shownFolder = shownPathOf(folder);
            return shownFolder.empty() ? fs::path() : (shownFolder / file.path).lexically_normal();
        }

    }

    RepairFrequencyMapsDialog::RepairFrequencyMapsDialog(const kit::FrequencyFormat &format,
                                                         const fs::path &root, Files files,
                                                         ShownPath shownPathOf, QWidget *parent)
        : QDialog(parent), m_files(std::move(files)) {
        setWindowTitle(tr("Repair Frequency Maps"));
        resize(720, 420);

        auto intro = new QLabel(tr("The voice bank has the following files of the format %1. The "
                                   "damaged files are checked.")
                                    .arg(format.name()));
        intro->setWordWrap(true);

        const auto caches = format.analysisCaches();
        // The user resizes the columns by the handles between them.
        auto columns = new QSplitter(Qt::Horizontal);
        columns->setChildrenCollapsible(false);
        for (int column = 0; column <= caches.size(); ++column) {
            const bool maps = column == 0;
            auto title = new QLabel(maps ? tr("Frequency Maps") : caches.at(column - 1).name);
            auto font = title->font();
            font.setBold(true);
            title->setFont(font);
            auto action = new QLabel(
                maps ? tr("A checked file that is repairable is repaired in place, and the "
                          "resampler generates the maps that the repair drops again. Another "
                          "checked file is removed with the analysis caches of its audio files, "
                          "and the resampler generates them again.")
                     : tr("A checked file is removed and the resampler generates it again."));
            action->setWordWrap(true);

            auto list = new QListWidget();
            QList<std::pair<fs::path, qsizetype>> items;
            for (const auto &[folder, folderFiles] : std::as_const(m_files)) {
                for (qsizetype i = 0; i < folderFiles.size(); ++i) {
                    const auto &file = folderFiles.at(i);
                    if (file.cache != column - 1) {
                        continue;
                    }
                    const auto restored = restoredPathOf(format, shownPathOf, folder, file);
                    auto text = restored.empty()
                                    ? displayed(root, folder, file.path)
                                    : QDir::toNativeSeparators(
                                          QString::fromStdU16String(restored.u16string()));
                    auto item = new QListWidgetItem(list);
                    if (!file.exists) {
                        text += QLatin1Char('\n') + tr("The file does not exist.");
                        item->setFlags(Qt::NoItemFlags);
                    } else if (file.damage) {
                        text +=
                            QLatin1Char('\n') + file.damage->description + QLatin1Char(' ') +
                            (file.damage->repairable ? tr("It is repaired in place.")
                                                     : tr("It is removed and generated again."));
                        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
                        item->setCheckState(Qt::Checked);
                    } else if (file.wavs.size() > 1) {
                        // An undamaged file that holds the maps of several audio files is not
                        // offered, because its removal generates all of their maps again, as the
                        // regeneration of all maps does.
                        text += QLatin1Char('\n') + tr("No damage was detected.");
                        item->setFlags(Qt::NoItemFlags);
                    } else {
                        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
                        item->setCheckState(Qt::Unchecked);
                    }
                    item->setText(text);
                    items.push_back({folder, i});
                }
            }
            connect(list, &QListWidget::itemChanged, this,
                    &RepairFrequencyMapsDialog::updateSummary);
            m_lists.push_back(list);
            m_items.push_back(items);

            auto pane = new QWidget();
            auto layout = new QVBoxLayout(pane);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(title);
            layout->addWidget(action);
            layout->addWidget(new CheckAllBox(list));
            layout->addWidget(list, 1);
            columns->addWidget(pane);
            columns->setStretchFactor(column, 1);
        }

        m_summary = new QLabel();
        m_summary->setWordWrap(true);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
        m_repair = buttons->addButton(tr("&Repair"), QDialogButtonBox::AcceptRole);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(intro);
        layout->addWidget(columns, 1);
        layout->addWidget(m_summary);
        layout->addWidget(buttons);
        updateSummary();
    }

    RepairFrequencyMapsDialog::~RepairFrequencyMapsDialog() = default;

    RepairFrequencyMapsDialog::Files RepairFrequencyMapsDialog::checkedFiles() const {
        Files checked;
        for (qsizetype column = 0; column < m_lists.size(); ++column) {
            const auto list = m_lists.at(column);
            for (int row = 0; row < list->count(); ++row) {
                if (list->item(row)->checkState() == Qt::Checked) {
                    const auto &[folder, index] = m_items.at(column).at(row);
                    checked[folder].push_back(m_files.at(folder).at(index));
                }
            }
        }
        return checked;
    }

    int RepairFrequencyMapsDialog::columnCount() const {
        return int(m_lists.size());
    }

    QListWidget *RepairFrequencyMapsDialog::list(int column) const {
        return m_lists.value(column);
    }

    QLabel *RepairFrequencyMapsDialog::summary() const {
        return m_summary;
    }

    QPushButton *RepairFrequencyMapsDialog::repairButton() const {
        return m_repair;
    }

    void RepairFrequencyMapsDialog::updateSummary() {
        // Counted without copying the files, since checking all items of a long list calls this
        // once for each item.
        int repaired = 0;
        int removed = 0;
        for (qsizetype column = 0; column < m_lists.size(); ++column) {
            const auto list = m_lists.at(column);
            for (int row = 0; row < list->count(); ++row) {
                if (list->item(row)->checkState() != Qt::Checked) {
                    continue;
                }
                const auto &[folder, index] = m_items.at(column).at(row);
                const auto &file = m_files.at(folder).at(index);
                ++(file.damage && file.damage->repairable ? repaired : removed);
            }
        }
        QStringList parts;
        if (repaired > 0) {
            parts.push_back(tr("%n file(s) are repaired in place.", nullptr, repaired));
        }
        if (removed > 0) {
            parts.push_back(tr("%n file(s) are removed, and the resampler generates them again.",
                               nullptr, removed));
        }
        m_summary->setText(parts.isEmpty() ? tr("No file is checked.")
                                           : parts.join(QLatin1Char(' ')));
        m_repair->setEnabled(!parts.isEmpty());
    }

}
