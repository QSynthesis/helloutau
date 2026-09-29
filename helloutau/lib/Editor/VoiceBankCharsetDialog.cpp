#include "VoiceBankCharsetDialog.h"

#include <QtCore/QDir>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Support/TextCodec.h>

namespace hello::daw {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        // The bytes of every text file of directory
        QList<QByteArray> textsOf(const kit::VoiceBankDirectorySource &directory) {
            QList<QByteArray> texts;
            for (const auto &[file, bytes] : directory.contents) {
                texts.push_back(bytes);
            }
            return texts;
        }

        // The byte sequences of texts that charset cannot read
        qsizetype invalidIn(const QList<QByteArray> &texts, const QString &charset) {
            const kit::TextCodec codec(charset);
            qsizetype total = 0;
            for (const auto &bytes : texts) {
                qsizetype invalid = 0;
                codec.decodeReplacing(bytes, &invalid);
                total += invalid;
            }
            return total;
        }

    }

    VoiceBankCharsetDialog::VoiceBankCharsetDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle(tr("Choose Encoding"));

        m_message = new QLabel();
        m_message->setWordWrap(true);

        m_folders = new QListWidget();
        m_folders->setSelectionMode(QAbstractItemView::ExtendedSelection);

        m_charsets = new QListWidget();
        m_charsets->addItems(kit::TextCodec::ustCandidates());
        m_charsets->setCurrentRow(0);

        m_previews = new QTabWidget();

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
        m_skip = buttons->addButton(tr("&Skip Folder"), QDialogButtonBox::RejectRole);
        m_skip->setToolTip(tr("Leave the folder out. Its samples are then found by file name "
                              "only, and it is asked again next time."));
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto choices = new QVBoxLayout();
        choices->addWidget(m_folders, 1);
        choices->addWidget(m_charsets, 1);
        auto content = new QHBoxLayout();
        content->addLayout(choices, 1);
        content->addWidget(m_previews, 3);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(m_message);
        layout->addLayout(content);
        layout->addWidget(buttons);

        connect(m_folders, &QListWidget::currentRowChanged, this, [this] { showCurrent(); });
        connect(m_folders, &QListWidget::itemChanged, this, [this] { updateFolders(); });
        connect(m_charsets, &QListWidget::currentRowChanged, this, [this] {
            if (const auto item = m_charsets->currentItem()) {
                setSelectedCharset(item->text());
            }
        });
        resize(760, 520);
    }

    VoiceBankCharsetDialog::~VoiceBankCharsetDialog() = default;

    void VoiceBankCharsetDialog::setRoot(const std::filesystem::path &root) {
        m_root = root;
    }

    std::optional<QString>
        VoiceBankCharsetDialog::selectCharset(const kit::VoiceBankDirectorySource &directory,
                                              kit::DiagnosticList &diagnostics) {
        return selectCharsets({&directory}, diagnostics).value(0);
    }

    QList<std::optional<QString>> VoiceBankCharsetDialog::selectCharsets(
        const QList<const kit::VoiceBankDirectorySource *> &directories,
        kit::DiagnosticList &diagnostics) {
        Q_UNUSED(diagnostics);
        setDirectories(directories);
        const bool accepted = exec() == QDialog::Accepted;
        QList<std::optional<QString>> charsets;
        for (int i = 0; i < m_directories.size(); ++i) {
            charsets.push_back(accepted && isIncluded(i) ? std::optional(m_choices[i])
                                                         : std::nullopt);
        }
        m_directories.clear();
        m_choices.clear();
        m_folders->clear();
        return charsets;
    }

    void VoiceBankCharsetDialog::setDirectory(const kit::VoiceBankDirectorySource &directory) {
        setDirectories({&directory});
    }

    void VoiceBankCharsetDialog::setDirectories(
        const QList<const kit::VoiceBankDirectorySource *> &directories) {
        m_directories = directories;

        // The encoding that reads all folders best, or for a folder that it cannot read
        // without invalid bytes, the one that reads that folder best
        const auto candidates = kit::TextCodec::ustCandidates();
        QList<QByteArray> all;
        for (const auto directory : directories) {
            all.append(textsOf(*directory));
        }
        const auto best = kit::TextCodec::ranked(all, candidates).value(0);
        m_choices.clear();
        for (const auto directory : directories) {
            const auto texts = textsOf(*directory);
            m_choices.push_back(invalidIn(texts, best) == 0
                                    ? best
                                    : kit::TextCodec::ranked(texts, candidates).value(0));
        }

        const auto bank = QString::fromStdU16String(m_root.filename().u16string());
        const bool single = directories.size() == 1;
        m_folders->setVisible(!single);
        m_skip->setText(single ? tr("&Skip Folder") : tr("&Skip All"));
        m_skip->setToolTip(single ? tr("Leave the folder out. Its samples are then found by file "
                                       "name only, and it is asked again next time.")
                                  : tr("Leave every folder out. Their samples are then found by "
                                       "file name only, and they are asked again next time."));
        if (single) {
            const auto &path = directories.first()->path;
            const auto folder = path.empty() ? bank : textOf(path);
            setWindowTitle(tr("Choose Encoding - %1").arg(folder));
            m_message->setText(
                path.empty()
                    ? tr("The voice bank \"%1\" does not state the encoding of its files. Choose "
                         "the encoding in which their text reads correctly. The choice is "
                         "recorded in the folder of the voice bank.")
                          .arg(textOf(m_root))
                    : tr("The folder \"%1\" of the voice bank \"%2\" does not state the encoding "
                         "of its files. Choose the encoding in which their text reads correctly. "
                         "The choice is recorded in that folder.")
                          .arg(folder, textOf(m_root)));
        } else {
            setWindowTitle(tr("Choose Encoding - %1").arg(bank));
            m_message->setText(
                tr("The voice bank \"%1\" does not state the encoding of %n folder(s). Choose "
                   "the encoding in which their text reads correctly: it is set for the folders "
                   "selected, all of them at first. Uncheck a folder to leave it out. The "
                   "choices are recorded in the folders.",
                   nullptr, int(directories.size()))
                    .arg(textOf(m_root)));
        }

        const QSignalBlocker blocker(m_folders);
        m_folders->clear();
        for (int i = 0; i < directories.size(); ++i) {
            auto item = new QListWidgetItem(m_folders);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
        }
        m_folders->setCurrentRow(0, QItemSelectionModel::NoUpdate);
        m_folders->selectAll();
        updateFolders();
        showCurrent();
    }

    QString VoiceBankCharsetDialog::selectedCharset() const {
        const int current = currentFolder();
        return current >= 0 && current < m_choices.size() ? m_choices[current] : QString();
    }

    void VoiceBankCharsetDialog::setSelectedCharset(const QString &charset) {
        QList<int> rows;
        for (const auto item : m_folders->selectedItems()) {
            rows.push_back(m_folders->row(item));
        }
        if (rows.isEmpty() && currentFolder() >= 0) {
            rows.push_back(currentFolder());
        }
        for (const int row : std::as_const(rows)) {
            if (row < m_choices.size()) {
                m_choices[row] = charset;
            }
        }
        updateFolders();
        showCurrent();
    }

    int VoiceBankCharsetDialog::currentFolder() const {
        return m_directories.isEmpty() ? -1 : std::max(0, m_folders->currentRow());
    }

    void VoiceBankCharsetDialog::setCurrentFolder(int index) {
        m_folders->setCurrentRow(index);
    }

    void VoiceBankCharsetDialog::selectFolders(const QList<int> &indices) {
        m_folders->clearSelection();
        for (const int index : indices) {
            if (const auto item = m_folders->item(index)) {
                item->setSelected(true);
            }
        }
    }

    QString VoiceBankCharsetDialog::charsetOf(int index) const {
        return m_choices.value(index);
    }

    bool VoiceBankCharsetDialog::isIncluded(int index) const {
        const auto item = m_folders->item(index);
        return item && item->checkState() == Qt::Checked;
    }

    void VoiceBankCharsetDialog::setIncluded(int index, bool included) {
        if (const auto item = m_folders->item(index)) {
            item->setCheckState(included ? Qt::Checked : Qt::Unchecked);
        }
    }

    QString VoiceBankCharsetDialog::folderText(int index) const {
        const auto item = m_folders->item(index);
        return item ? item->text() : QString();
    }

    bool VoiceBankCharsetDialog::isGrey(const QString &charset) const {
        const auto items = m_charsets->findItems(charset, Qt::MatchExactly);
        return !items.isEmpty() && items.first()->foreground().style() != Qt::NoBrush;
    }

    QStringList VoiceBankCharsetDialog::previewTitles() const {
        QStringList titles;
        for (int i = 0; i < m_previews->count(); ++i) {
            titles.push_back(m_previews->tabText(i));
        }
        return titles;
    }

    QString VoiceBankCharsetDialog::previewText(int index) const {
        const auto edit = qobject_cast<QPlainTextEdit *>(m_previews->widget(index));
        return edit ? edit->toPlainText() : QString();
    }

    void VoiceBankCharsetDialog::updateFolders() {
        const auto bank = QString::fromStdU16String(m_root.filename().u16string());
        const QSignalBlocker blocker(m_folders);
        for (int i = 0; i < m_directories.size() && i < m_folders->count(); ++i) {
            const auto &path = m_directories[i]->path;
            const auto name = path.empty() ? tr("%1 (the voice bank)").arg(bank) : textOf(path);
            const auto invalid = invalidIn(textsOf(*m_directories[i]), m_choices[i]);
            auto text = QStringLiteral("%1  -  %2").arg(name, m_choices[i]);
            if (invalid > 0) {
                text += QLatin1String("  ") + tr("(%n invalid)", nullptr, int(invalid));
            }
            m_folders->item(i)->setText(text);
        }
    }

    void VoiceBankCharsetDialog::showCurrent() {
        const int current = currentFolder();
        if (current < 0) {
            return;
        }
        const auto directory = m_directories[current];
        const auto texts = textsOf(*directory);
        const auto grey = palette().brush(QPalette::Disabled, QPalette::Text);
        const QSignalBlocker blocker(m_charsets);
        for (int i = 0; i < m_charsets->count(); ++i) {
            auto item = m_charsets->item(i);
            item->setForeground(invalidIn(texts, item->text()) > 0 ? grey : QBrush());
            if (item->text() == m_choices[current]) {
                m_charsets->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
            }
        }

        m_previews->clear();
        for (int i = 0; i < int(directory->textFiles().size()); ++i) {
            auto edit = new QPlainTextEdit();
            edit->setReadOnly(true);
            edit->setLineWrapMode(QPlainTextEdit::NoWrap);
            m_previews->addTab(edit, {});
        }
        updatePreview();
    }

    void VoiceBankCharsetDialog::updatePreview() {
        const int current = currentFolder();
        if (current < 0) {
            return;
        }
        const auto directory = m_directories[current];
        const kit::TextCodec codec(m_choices[current]);
        const auto files = directory->textFiles();
        for (int i = 0; i < int(files.size()) && i < m_previews->count(); ++i) {
            const auto file = files[i];
            const auto record = directory->files.find(file);
            const auto name =
                record != directory->files.end()
                    ? QString::fromStdU16String(record->second.name.u16string())
                    : QString::fromLatin1(kit::VoiceBankDirectorySource::fileName(file));
            const auto contents = directory->contents.find(file);
            qsizetype invalid = 0;
            const auto text = contents != directory->contents.end()
                                  ? codec.decodeReplacing(contents->second, &invalid)
                                  : QString();
            m_previews->setTabText(
                i, invalid == 0 ? name : tr("%1 (%n invalid)", nullptr, int(invalid)).arg(name));
            static_cast<QPlainTextEdit *>(m_previews->widget(i))->setPlainText(text);
        }
    }

}
