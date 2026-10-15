#include "ConvertFrequencyMapsDialog.h"

#include <algorithm>

#include <QtCore/QDir>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/VoiceBank/FrequencyFormat.h>

#include <helloutau/Widgets/FileCheckList.h>

namespace hello::daw {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        // Returns the header of the column of a format, with a parenthesized part of the name,
        // such as its resampler, on a line of its own, so that the column is narrow
        QString headerOf(const QString &name) {
            const auto open = name.indexOf(QStringLiteral(" ("));
            return open < 0 ? name : name.left(open) + QLatin1Char('\n') + name.mid(open + 1);
        }

    }

    // The boxes hold the indexes in m_formats as their item data.
    ConvertFrequencyMapsDialog::ConvertFrequencyMapsDialog(
        QList<File> files, const std::filesystem::path &root,
        const QList<kit::FrequencyFormat *> &formats, const kit::FrequencyFormat *source,
        QWidget *parent)
        : QDialog(parent), m_files(std::move(files)), m_formats(formats) {
        setWindowTitle(tr("Convert Frequency Maps"));

        m_source = new QComboBox();
        m_target = new QComboBox();
        QStringList headers{tr("File")};
        for (qsizetype i = 0; i < m_formats.size(); ++i) {
            const auto format = m_formats.at(i);
            headers.push_back(headerOf(format->name()));
            m_source->addItem(format->name(), int(i));
            if (format->canWrite()) {
                m_target->addItem(format->name(), int(i));
            }
        }
        m_source->setCurrentIndex(std::max(0, m_source->findData(int(m_formats.indexOf(source)))));
        for (int i = 0; i < m_target->count(); ++i) {
            if (m_formats.at(m_target->itemData(i).toInt()) != source) {
                m_target->setCurrentIndex(i);
                break;
            }
        }

        // The rows are in the order of the files, with a check mark for each format of which
        // the file has a map, as in the table of the window.
        m_list = new FileCheckList(headers);
        for (int column = 1; column < headers.size(); ++column) {
            m_list->setColumnAlignment(column, Qt::AlignCenter);
        }
        for (const auto &file : std::as_const(m_files)) {
            auto relative = file.path.lexically_relative(root);
            if (!file.shownPath.empty()) {
                relative = file.shownPath;
            }
            QStringList texts{
                QDir::toNativeSeparators(textOf(relative.empty() ? file.path : relative))};
            for (qsizetype i = 0; i < m_formats.size(); ++i) {
                texts.push_back(file.maps.value(i) ? QString(QChar(0x2713)) : QString());
            }
            m_list->addRow(texts);
            m_anySelected = m_anySelected || file.selected;
        }
        m_checkMaps = m_list->addCheckButton(tr("Check Files with Source &Maps"),
                                             [this](int row) { return hasSourceMap(row); });
        if (m_anySelected) {
            m_list->checkWhere([this](int row) { return m_files.at(row).selected; });
        } else {
            m_list->checkWhere([this](int row) { return hasSourceMap(row); });
        }

        auto note = new QLabel(tr("An audio file without a map of the source format is skipped. "
                                  "The maps of the target format are replaced, and their analysis "
                                  "caches are removed."));
        note->setWordWrap(true);
        auto form = new QFormLayout();
        form->addRow(tr("&From:"), m_source);
        form->addRow(tr("&To:"), m_target);

        m_summary = new QLabel();
        m_summary->setWordWrap(true);
        m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

        // The files are on the left, where the list has the full height, and the formats on
        // the right, in the panes of a splitter.
        auto right = new QWidget();
        auto rightLayout = new QVBoxLayout(right);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->addLayout(form);
        rightLayout->addWidget(note);
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
        resize(900, 520);

        connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(m_list, &FileCheckList::checkedChanged, this,
                &ConvertFrequencyMapsDialog::updateSummary);
        connect(m_source, &QComboBox::currentIndexChanged, this, [this] {
            if (!m_anySelected) {
                m_list->checkWhere([this](int row) { return hasSourceMap(row); });
            }
            updateSummary();
        });
        connect(m_target, &QComboBox::currentIndexChanged, this,
                &ConvertFrequencyMapsDialog::updateSummary);
        updateSummary();
    }

    ConvertFrequencyMapsDialog::~ConvertFrequencyMapsDialog() = default;

    kit::FrequencyFormat *ConvertFrequencyMapsDialog::sourceFormat() const {
        const auto data = m_source->currentData();
        return data.isValid() ? m_formats.at(data.toInt()) : nullptr;
    }

    kit::FrequencyFormat *ConvertFrequencyMapsDialog::targetFormat() const {
        const auto data = m_target->currentData();
        return data.isValid() ? m_formats.at(data.toInt()) : nullptr;
    }

    QList<std::filesystem::path> ConvertFrequencyMapsDialog::checkedFiles() const {
        QList<std::filesystem::path> files;
        for (const int row : m_list->checkedRows()) {
            files.push_back(m_files.at(row).path);
        }
        return files;
    }

    FileCheckList *ConvertFrequencyMapsDialog::fileList() const {
        return m_list;
    }

    QPushButton *ConvertFrequencyMapsDialog::checkMapsButton() const {
        return m_checkMaps;
    }

    QComboBox *ConvertFrequencyMapsDialog::sourceBox() const {
        return m_source;
    }

    QComboBox *ConvertFrequencyMapsDialog::targetBox() const {
        return m_target;
    }

    QSplitter *ConvertFrequencyMapsDialog::splitter() const {
        return m_splitter;
    }

    QDialogButtonBox *ConvertFrequencyMapsDialog::buttonBox() const {
        return m_buttons;
    }

    bool ConvertFrequencyMapsDialog::hasSourceMap(int row) const {
        const auto data = m_source->currentData();
        return data.isValid() && m_files.at(row).maps.value(data.toInt());
    }

    void ConvertFrequencyMapsDialog::updateSummary() {
        // The files without a map of the source format are skipped by the conversion, and are
        // counted apart.
        int converted = 0;
        int skipped = 0;
        for (const int row : m_list->checkedRows()) {
            ++(hasSourceMap(row) ? converted : skipped);
        }
        QStringList parts;
        if (converted > 0) {
            parts.push_back(tr("The frequency maps of %n audio file(s) will be converted.", nullptr,
                               converted));
        }
        if (skipped > 0) {
            parts.push_back(tr("%n checked audio file(s) have no map of the source format and are "
                               "skipped.",
                               nullptr, skipped));
        }
        if (converted > 0) {
            parts.push_back(tr("The conversion cannot be undone."));
        }
        m_summary->setText(parts.isEmpty() ? tr("No file is checked.")
                                           : parts.join(QLatin1Char(' ')));
        const auto source = sourceFormat();
        const auto target = targetFormat();
        m_buttons->button(QDialogButtonBox::Ok)
            ->setEnabled(converted > 0 && source && target && source != target);
    }

}
