#include "UstCharsetDialog.h"

#include <algorithm>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Support/TextCodec.h>

namespace hello::daw {

    UstCharsetDialog::UstCharsetDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle(tr("Choose Encoding"));

        m_charsets = new QListWidget();
        m_charsets->addItems(kit::TextCodec::ustCandidates());
        m_charsets->setCurrentRow(0);

        m_preview = new QPlainTextEdit();
        m_preview->setReadOnly(true);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto content = new QHBoxLayout();
        content->addWidget(m_charsets, 1);
        content->addWidget(m_preview, 3);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(tr("This file does not state its encoding. Choose the "
                                        "encoding in which its text reads correctly.")));
        layout->addLayout(content);
        layout->addWidget(buttons);

        connect(m_charsets, &QListWidget::currentRowChanged, this,
                &UstCharsetDialog::updatePreview);
        resize(640, 400);
    }

    UstCharsetDialog::~UstCharsetDialog() = default;

    std::optional<QString> UstCharsetDialog::selectCharset(const kit::UstDocument &ust,
                                                           const std::filesystem::path &path) {
        setDocument(ust, path);
        const bool accepted = exec() == QDialog::Accepted;
        m_ust = nullptr;
        if (!accepted) {
            return std::nullopt;
        }
        return selectedCharset();
    }

    QString UstCharsetDialog::preview(const kit::UstDocument &ust, const QString &charset) {
        const kit::TextCodec codec(charset);
        const auto text = [&](QByteArrayView bytes) {
            const auto decoded = codec.decode(bytes);
            return decoded ? *decoded : tr("(not valid in this encoding)");
        };

        QStringList lyrics;
        for (const auto lyric : ust.rawLyrics()) {
            lyrics.push_back(text(lyric));
        }
        return tr("Project: %1\nVoice bank: %2\n\nLyrics:\n%3")
            .arg(text(ust.rawProjectName()), text(ust.rawVoiceDir()), lyrics.join(u' '));
    }

    void UstCharsetDialog::setDocument(const kit::UstDocument &ust,
                                       const std::filesystem::path &path) {
        m_ust = &ust;
        setWindowTitle(
            tr("Choose Encoding - %1").arg(QString::fromStdU16String(path.filename().u16string())));

        // The text of the file, by which the encoding that reads it best is selected, and those
        // that cannot read it are grey
        QList<QByteArray> texts{ust.rawProjectName().toByteArray(),
                                ust.rawVoiceDir().toByteArray()};
        for (const auto lyric : ust.rawLyrics()) {
            texts.push_back(lyric.toByteArray());
        }
        const auto grey = palette().brush(QPalette::Disabled, QPalette::Text);
        for (int i = 0; i < m_charsets->count(); ++i) {
            const auto item = m_charsets->item(i);
            const kit::TextCodec codec(item->text());
            const bool readable = std::all_of(texts.cbegin(), texts.cend(), [&](const auto &t) {
                return codec.decode(t).has_value();
            });
            item->setForeground(readable ? QBrush() : grey);
        }
        setSelectedCharset(kit::TextCodec::ranked(texts, kit::TextCodec::ustCandidates()).value(0));
        updatePreview();
    }

    bool UstCharsetDialog::isGrey(const QString &charset) const {
        const auto items = m_charsets->findItems(charset, Qt::MatchExactly);
        return !items.isEmpty() && items.first()->foreground().style() != Qt::NoBrush;
    }

    QString UstCharsetDialog::selectedCharset() const {
        const auto item = m_charsets->currentItem();
        return item ? item->text() : QString();
    }

    void UstCharsetDialog::setSelectedCharset(const QString &charset) {
        const auto items = m_charsets->findItems(charset, Qt::MatchExactly);
        if (!items.isEmpty()) {
            m_charsets->setCurrentItem(items.first());
        }
    }

    QString UstCharsetDialog::previewText() const {
        return m_preview->toPlainText();
    }

    void UstCharsetDialog::updatePreview() {
        m_preview->setPlainText(m_ust ? preview(*m_ust, selectedCharset()) : QString());
    }

}
