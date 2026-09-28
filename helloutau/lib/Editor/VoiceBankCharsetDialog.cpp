#include "VoiceBankCharsetDialog.h"

#include <QtCore/QDir>
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

    }

    VoiceBankCharsetDialog::VoiceBankCharsetDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle(tr("Choose Encoding"));

        m_message = new QLabel();
        m_message->setWordWrap(true);

        m_charsets = new QListWidget();
        m_charsets->addItems(kit::TextCodec::ustCandidates());
        m_charsets->setCurrentRow(0);

        m_previews = new QTabWidget();

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
        const auto skip = buttons->addButton(tr("&Skip Folder"), QDialogButtonBox::RejectRole);
        skip->setToolTip(tr("Leave the folder out. Its samples are then found by file name "
                            "only, and it is asked again next time."));
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto content = new QHBoxLayout();
        content->addWidget(m_charsets, 1);
        content->addWidget(m_previews, 3);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(m_message);
        layout->addLayout(content);
        layout->addWidget(buttons);

        connect(m_charsets, &QListWidget::currentRowChanged, this,
                &VoiceBankCharsetDialog::updatePreview);
        resize(720, 480);
    }

    VoiceBankCharsetDialog::~VoiceBankCharsetDialog() = default;

    void VoiceBankCharsetDialog::setRoot(const std::filesystem::path &root) {
        m_root = root;
    }

    std::optional<QString>
        VoiceBankCharsetDialog::selectCharset(const kit::VoiceBankDirectorySource &directory,
                                              kit::DiagnosticList &diagnostics) {
        Q_UNUSED(diagnostics);
        setDirectory(directory);
        const bool accepted = exec() == QDialog::Accepted;
        m_directory = nullptr;
        if (!accepted) {
            return std::nullopt;
        }
        return selectedCharset();
    }

    void VoiceBankCharsetDialog::setDirectory(const kit::VoiceBankDirectorySource &directory) {
        m_directory = &directory;

        const auto bank = QString::fromStdU16String(m_root.filename().u16string());
        const auto folder = directory.path.empty() ? bank : textOf(directory.path);
        setWindowTitle(tr("Choose Encoding - %1").arg(folder));
        m_message->setText(
            directory.path.empty()
                ? tr("The voice bank \"%1\" does not state the encoding of its files. Choose the "
                     "encoding in which their text reads correctly. The choice is recorded in "
                     "the folder of the voice bank.")
                      .arg(textOf(m_root))
                : tr("The folder \"%1\" of the voice bank \"%2\" does not state the encoding of "
                     "its files. Choose the encoding in which their text reads correctly. The "
                     "choice is recorded in that folder.")
                      .arg(folder, textOf(m_root)));

        m_previews->clear();
        for (int i = 0; i < int(directory.textFiles().size()); ++i) {
            auto edit = new QPlainTextEdit();
            edit->setReadOnly(true);
            edit->setLineWrapMode(QPlainTextEdit::NoWrap);
            m_previews->addTab(edit, {});
        }
        updatePreview();
    }

    QString VoiceBankCharsetDialog::selectedCharset() const {
        const auto item = m_charsets->currentItem();
        return item ? item->text() : QString();
    }

    void VoiceBankCharsetDialog::setSelectedCharset(const QString &charset) {
        const auto items = m_charsets->findItems(charset, Qt::MatchExactly);
        if (!items.isEmpty()) {
            m_charsets->setCurrentItem(items.first());
        }
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

    void VoiceBankCharsetDialog::updatePreview() {
        if (!m_directory) {
            return;
        }
        const kit::TextCodec codec(selectedCharset());
        const auto files = m_directory->textFiles();
        for (int i = 0; i < int(files.size()) && i < m_previews->count(); ++i) {
            const auto file = files[i];
            const auto record = m_directory->files.find(file);
            const auto name =
                record != m_directory->files.end()
                    ? QString::fromStdU16String(record->second.name.u16string())
                    : QString::fromLatin1(kit::VoiceBankDirectorySource::fileName(file));
            const auto contents = m_directory->contents.find(file);
            qsizetype invalid = 0;
            const auto text = contents != m_directory->contents.end()
                                  ? codec.decodeReplacing(contents->second, &invalid)
                                  : QString();
            m_previews->setTabText(
                i, invalid == 0 ? name : tr("%1 (%n invalid)", nullptr, int(invalid)).arg(name));
            static_cast<QPlainTextEdit *>(m_previews->widget(i))->setPlainText(text);
        }
    }

}
