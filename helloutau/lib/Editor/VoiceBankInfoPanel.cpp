#include "VoiceBankInfoPanel.h"

#include <functional>

#include <QtCore/QTimer>
#include <QtGui/QFocusEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>
#include <QtGui/QPixmap>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

#include <stdcorelib/pimpl.h>

#include <hellokit/Edit/VoiceBankEdits.h>
#include <hellokit/Edit/VoiceBankRefs.h>
#include <hellokit/Edit/VoiceBankSession.h>
#include <hellokit/VoiceBank/VoiceBank.h>

namespace hello::daw {

    namespace {

        // The size of the image of a character, as UTAU requires it
        constexpr int ImageSize = 100;

        // Calls a function when an object loses the focus
        class FocusWatcher : public QObject {
        public:
            FocusWatcher(QObject *watched, std::function<void()> lost)
                : QObject(watched), m_lost(std::move(lost)) {
                watched->installEventFilter(this);
            }

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override {
                if (event->type() == QEvent::FocusOut) {
                    m_lost();
                }
                return QObject::eventFilter(watched, event);
            }

        private:
            std::function<void()> m_lost;
        };

        QString noteName(int noteNum) {
            static const char *const names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                                "F#", "G",  "G#", "A",  "A#", "B"};
            return QString::fromLatin1(names[noteNum % 12]) + QString::number(noteNum / 12 - 1);
        }

        // Returns text with each CRLF and each CR as LF, as a text box shows the line breaks.
        QString lineFeeds(QString text) {
            return text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"))
                .replace(QLatin1Char('\r'), QLatin1Char('\n'));
        }

        // The lines of a text box, none for an empty box
        QStringList linesOf(const QPlainTextEdit *edit) {
            const auto text = edit->toPlainText();
            return text.isEmpty() ? QStringList() : text.split(QLatin1Char('\n'));
        }

    }

    class VoiceBankInfoPanel::Impl {
    public:
        using Decl = VoiceBankInfoPanel;

        Impl(Decl *decl, kit::VoiceBankSession *session) : _decl(decl), session(session) {
        }

        Decl *_decl;
        kit::VoiceBankSession *session;
        std::filesystem::path root;

        QLineEdit *name = nullptr;
        QLineEdit *image = nullptr;
        QLineEdit *sample = nullptr;
        QLineEdit *author = nullptr;
        QLineEdit *web = nullptr;
        QPlainTextEdit *otherLines = nullptr;
        QLabel *preview = nullptr;
        QPlainTextEdit *readme = nullptr;
        QTableWidget *prefixes = nullptr;
        QLabel *prefixNote = nullptr;

        // Whether the widgets are being filled from the session, which writes nothing back
        bool updating = false;
        bool refreshPending = false;

        kit::VoiceBankRef bank() const {
            return kit::VoiceBankRef(session);
        }

        kit::VoiceCharacter storedCharacter() const {
            const auto character = bank().character();
            return character.isValid() ? character.toVoiceCharacter() : kit::VoiceCharacter();
        }

        void report(const kit::DiagnosticList &diagnostics) {
            stdc_decl_t;
            if (kit::hasError(diagnostics)) {
                Q_EMIT decl.editRejected(diagnostics);
            }
        }

        // Writes the fields of character.txt as shown, unless they are those stored, or else
        // empty while the voice bank has no character.txt.
        void writeCharacter() {
            if (updating) {
                return;
            }
            const auto stored = storedCharacter();
            auto value = stored;
            value.name = name->text();
            value.image = image->text();
            value.sample = sample->text();
            value.author = author->text();
            value.web = web->text();
            // The lines are compared as the text shown, because the box shows no lines and one
            // empty line alike.
            if (otherLines->toPlainText() != lineFeeds(stored.extraLines.join(QLatin1Char('\n')))) {
                value.extraLines = linesOf(otherLines);
            }
            if (value == stored) {
                return;
            }
            kit::DiagnosticList diagnostics;
            kit::VoiceBankEdits::setCharacter(bank(), value, diagnostics);
            report(diagnostics);
        }

        // Writes the text of readme.txt as shown, unless it is the text stored. A text box
        // shows every line break as LF, so the stored text is compared with its line breaks
        // as LF, and the text is written with CRLF if the stored text has CRLF, as the files
        // of UTAU have.
        void writeReadme() {
            if (updating) {
                return;
            }
            const auto stored = bank().readme();
            auto text = readme->toPlainText();
            if (text == lineFeeds(stored)) {
                return;
            }
            if (stored.contains(QStringLiteral("\r\n"))) {
                text.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
            }
            kit::DiagnosticList diagnostics;
            kit::VoiceBankEdits::setReadme(bank(), text, diagnostics);
            report(diagnostics);
        }

        // The key of a row of the prefix table
        static int keyOf(int row) {
            return kit::VoicePrefix::minimumKey + row;
        }

        void writePrefix(int row) {
            if (updating) {
                return;
            }
            const int key = keyOf(row);
            kit::VoicePrefix value;
            value.prefix = prefixes->item(row, 1)->text();
            value.suffix = prefixes->item(row, 2)->text();
            const auto map = bank().prefixMap();
            const bool present = map.isValid() && map.contains(key);
            if (present ? map.value(key) == value : value == kit::VoicePrefix()) {
                return;
            }
            kit::DiagnosticList diagnostics;
            kit::VoiceBankEdits::setPrefix(bank(), key, value, diagnostics);
            report(diagnostics);
            refresh();
        }

        static void show(QLineEdit *edit, const QString &text) {
            if (!edit->hasFocus() && edit->text() != text) {
                edit->setText(text);
            }
        }

        static void show(QPlainTextEdit *edit, const QString &text) {
            if (!edit->hasFocus() && edit->toPlainText() != text) {
                edit->setPlainText(text);
            }
        }

        void showPreview() {
            const auto file = image->text();
            const auto path = kit::VoiceBank::imagePathOf(root, file);
            QPixmap pixmap;
            if (path) {
                pixmap.load(QString::fromStdU16String(path->u16string()));
            }
            if (pixmap.isNull()) {
                preview->setPixmap({});
                preview->setText(
                    file.isEmpty()
                        ? VoiceBankInfoPanel::tr("No image")
                        : (path ? VoiceBankInfoPanel::tr("The image does not read.")
                                : VoiceBankInfoPanel::tr(
                                      "The image must be a file in the voice bank folder.")));
            } else {
                preview->setPixmap(pixmap.scaled(ImageSize, ImageSize, Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation));
            }
        }

        void refresh() {
            refreshPending = false;
            updating = true;
            const auto character = storedCharacter();
            show(name, character.name);
            show(image, character.image);
            show(sample, character.sample);
            show(author, character.author);
            show(web, character.web);
            show(otherLines, character.extraLines.join(QLatin1Char('\n')));
            show(readme, bank().readme());
            showPreview();

            const auto map = bank().prefixMap();
            prefixNote->setVisible(!map.isValid());
            const auto missing =
                QGuiApplication::palette().brush(QPalette::Disabled, QPalette::Text);
            for (int row = 0; row < prefixes->rowCount(); ++row) {
                const int key = keyOf(row);
                const bool present = map.isValid() && map.contains(key);
                const auto value = present ? map.value(key) : kit::VoicePrefix();
                prefixes->item(row, 0)->setForeground(
                    present ? QGuiApplication::palette().brush(QPalette::Text) : missing);
                if (prefixes->item(row, 1)->text() != value.prefix) {
                    prefixes->item(row, 1)->setText(value.prefix);
                }
                if (prefixes->item(row, 2)->text() != value.suffix) {
                    prefixes->item(row, 2)->setText(value.suffix);
                }
            }
            updating = false;
        }

        void scheduleRefresh() {
            stdc_decl_t;
            if (refreshPending) {
                return;
            }
            refreshPending = true;
            QTimer::singleShot(0, &decl, [this] {
                if (refreshPending) {
                    refresh();
                }
            });
        }

        QWidget *characterPage() {
            stdc_decl_t;
            auto page = new QWidget();
            auto form = new QFormLayout();
            const auto line = [&](QLineEdit *&edit, const QString &label, const char *object) {
                edit = new QLineEdit();
                edit->setObjectName(QLatin1String(object));
                QObject::connect(edit, &QLineEdit::editingFinished, &decl,
                                 [this] { writeCharacter(); });
                form->addRow(label, edit);
            };
            line(name, VoiceBankInfoPanel::tr("&Name:"), "name");
            name->setPlaceholderText(VoiceBankInfoPanel::tr("The name of the folder"));
            line(image, VoiceBankInfoPanel::tr("&Image:"), "image");
            line(sample, VoiceBankInfoPanel::tr("&Sample:"), "sample");
            line(author, VoiceBankInfoPanel::tr("&Author:"), "author");
            line(web, VoiceBankInfoPanel::tr("&Web:"), "web");
            QObject::connect(image, &QLineEdit::textChanged, &decl, [this] { showPreview(); });

            preview = new QLabel();
            preview->setFixedSize(ImageSize, ImageSize);
            preview->setAlignment(Qt::AlignCenter);
            preview->setFrameShape(QFrame::StyledPanel);
            preview->setWordWrap(true);

            otherLines = new QPlainTextEdit();
            otherLines->setObjectName(QStringLiteral("otherLines"));
            new FocusWatcher(otherLines, [this] { writeCharacter(); });

            auto layout = new QVBoxLayout(page);
            layout->addWidget(preview, 0, Qt::AlignHCenter);
            layout->addLayout(form);
            layout->addWidget(new QLabel(VoiceBankInfoPanel::tr("Other lines:")));
            layout->addWidget(otherLines, 1);
            return page;
        }

        QWidget *prefixPage() {
            stdc_decl_t;
            auto page = new QWidget();
            prefixNote = new QLabel(VoiceBankInfoPanel::tr(
                "The voice bank has no prefix.map. Editing a cell creates it."));
            prefixNote->setWordWrap(true);
            prefixes = new QTableWidget(
                kit::VoicePrefix::maximumKey - kit::VoicePrefix::minimumKey + 1, 3);
            prefixes->setHorizontalHeaderLabels({VoiceBankInfoPanel::tr("Note"),
                                                 VoiceBankInfoPanel::tr("Prefix"),
                                                 VoiceBankInfoPanel::tr("Suffix")});
            prefixes->verticalHeader()->hide();
            prefixes->horizontalHeader()->setStretchLastSection(true);
            for (int row = 0; row < prefixes->rowCount(); ++row) {
                auto note = new QTableWidgetItem(noteName(keyOf(row)));
                note->setFlags(note->flags() & ~Qt::ItemIsEditable);
                prefixes->setItem(row, 0, note);
                prefixes->setItem(row, 1, new QTableWidgetItem());
                prefixes->setItem(row, 2, new QTableWidgetItem());
            }
            QObject::connect(prefixes, &QTableWidget::itemChanged, &decl,
                             [this](QTableWidgetItem *item) {
                                 if (item->column() > 0) {
                                     writePrefix(item->row());
                                 }
                             });
            prefixes->setContextMenuPolicy(Qt::CustomContextMenu);
            QObject::connect(prefixes, &QWidget::customContextMenuRequested, &decl,
                             [this](const QPoint &position) {
                                 stdc_decl_t;
                                 const auto item = prefixes->itemAt(position);
                                 if (!item) {
                                     return;
                                 }
                                 const int key = keyOf(item->row());
                                 const auto map = bank().prefixMap();
                                 QMenu menu(&decl);
                                 const auto remove =
                                     menu.addAction(VoiceBankInfoPanel::tr("&Remove Key"));
                                 remove->setEnabled(map.isValid() && map.contains(key));
                                 QObject::connect(remove, &QAction::triggered, &decl, [this, key] {
                                     stdc_decl_t;
                                     decl.removePrefix(key);
                                 });
                                 menu.exec(prefixes->viewport()->mapToGlobal(position));
                             });
            auto layout = new QVBoxLayout(page);
            layout->addWidget(prefixNote);
            layout->addWidget(prefixes, 1);
            return page;
        }
    };

    VoiceBankInfoPanel::VoiceBankInfoPanel(kit::VoiceBankSession *session, QWidget *parent)
        : QWidget(parent), _impl(std::make_unique<Impl>(this, session)) {
        stdc_impl_t;
        auto tabs = new QTabWidget();
        tabs->addTab(impl.characterPage(), tr("&Character"));
        impl.readme = new QPlainTextEdit();
        impl.readme->setObjectName(QStringLiteral("readme"));
        new FocusWatcher(impl.readme, [this] {
            stdc_impl_t;
            impl.writeReadme();
        });
        tabs->addTab(impl.readme, tr("&Readme"));
        tabs->addTab(impl.prefixPage(), tr("&Prefix Map"));
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(tabs);

        connect(session, &kit::VoiceBankSession::changed, this, [this] {
            stdc_impl_t;
            impl.scheduleRefresh();
        });
        impl.refresh();
    }

    VoiceBankInfoPanel::~VoiceBankInfoPanel() = default;

    void VoiceBankInfoPanel::setRoot(const std::filesystem::path &root) {
        stdc_impl_t;
        impl.root = root;
        impl.showPreview();
    }

    void VoiceBankInfoPanel::commit() {
        stdc_impl_t;
        impl.writeCharacter();
        impl.writeReadme();
    }

    QLineEdit *VoiceBankInfoPanel::nameEdit() const {
        stdc_impl_t;
        return impl.name;
    }

    QLineEdit *VoiceBankInfoPanel::imageEdit() const {
        stdc_impl_t;
        return impl.image;
    }

    QLineEdit *VoiceBankInfoPanel::sampleEdit() const {
        stdc_impl_t;
        return impl.sample;
    }

    QLineEdit *VoiceBankInfoPanel::authorEdit() const {
        stdc_impl_t;
        return impl.author;
    }

    QLineEdit *VoiceBankInfoPanel::webEdit() const {
        stdc_impl_t;
        return impl.web;
    }

    QPlainTextEdit *VoiceBankInfoPanel::otherLinesEdit() const {
        stdc_impl_t;
        return impl.otherLines;
    }

    QLabel *VoiceBankInfoPanel::imagePreview() const {
        stdc_impl_t;
        return impl.preview;
    }

    QPlainTextEdit *VoiceBankInfoPanel::readmeEdit() const {
        stdc_impl_t;
        return impl.readme;
    }

    QTableWidget *VoiceBankInfoPanel::prefixTable() const {
        stdc_impl_t;
        return impl.prefixes;
    }

    bool VoiceBankInfoPanel::removePrefix(int noteNum) {
        stdc_impl_t;
        kit::DiagnosticList diagnostics;
        const bool removed = kit::VoiceBankEdits::removePrefix(impl.bank(), noteNum, diagnostics);
        impl.report(diagnostics);
        impl.refresh();
        return removed;
    }

}
