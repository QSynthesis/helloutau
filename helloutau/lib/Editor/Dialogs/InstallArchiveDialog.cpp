#include "InstallArchiveDialog.h"

#include <algorithm>
#include <system_error>

#include <QtCore/QDir>
#include <QtCore/QHash>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Support/FolderAccess.h>
#include <hellokit/Support/TextCodec.h>
#include <hellokit/Support/ZipExtractor.h>

#include "AppSettings.h"

namespace fs = std::filesystem;

namespace hello::daw {

    namespace {

        // The size up to which a text file of the content is read for the preview
        constexpr qint64 previewMaximumSize = 1024 * 1024;
        // The number of text files previewed, enough for the oto.ini of several folders
        constexpr qsizetype previewMaximumCount = 8;

        QString textOf(const fs::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        fs::path pathOf(const QString &path) {
            return fs::path(path.toStdU16String());
        }

        // Returns the file name of path, the last component after a slash.
        QString fileNameOf(const QString &path) {
            return path.mid(path.lastIndexOf(u'/') + 1);
        }

        // Returns the value of key in text of lines of key=value, as character.txt and
        // plugin.txt hold them, or an empty string if absent.
        QString valueOf(const QString &text, QLatin1String key) {
            for (auto line : text.split(u'\n')) {
                if (line.endsWith(u'\r')) {
                    line.chop(1);
                }
                const auto equal = line.indexOf(u'=');
                if (equal > 0 &&
                    line.first(equal).trimmed().compare(key, Qt::CaseInsensitive) == 0) {
                    return line.mid(equal + 1).trimmed();
                }
            }
            return {};
        }

        // Shows in gray each candidate of combo for which unreadable returns true, as the dialogs
        // that choose the encoding of a UST and of a voice bank do.
        template <class Predicate>
        void grayCandidates(QComboBox *combo, Predicate unreadable) {
            const auto gray = combo->palette().brush(QPalette::Disabled, QPalette::Text);
            for (int i = 0; i < combo->count(); ++i) {
                combo->setItemData(i, unreadable(combo->itemText(i)) ? QVariant(gray) : QVariant(),
                                   Qt::ForegroundRole);
            }
        }

        bool isGrayIn(const QComboBox *combo, const QString &text) {
            const auto index = combo->findText(text);
            return index >= 0 && combo->itemData(index, Qt::ForegroundRole).isValid();
        }

        // Fills tree with the folders and files of paths, which use / as the separator.
        void fillTree(QTreeWidget *tree, const QStringList &paths) {
            tree->clear();
            QHash<QString, QTreeWidgetItem *> folders;
            for (const auto &path : paths) {
                QTreeWidgetItem *parent = nullptr;
                const auto components = path.split(u'/');
                QString prefix;
                for (qsizetype i = 0; i + 1 < components.size(); ++i) {
                    prefix += components.at(i) + u'/';
                    auto &folder = folders[prefix];
                    if (!folder) {
                        folder = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
                        folder->setText(0, components.at(i));
                    }
                    parent = folder;
                }
                const auto item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
                item->setText(0, components.last());
            }
            tree->expandAll();
        }

    }

    InstallArchiveDialog::InstallArchiveDialog(kit::InstallArchive &archive, QWidget *parent)
        : QDialog(parent), m_archive(archive) {
        setWindowTitle(tr("Install"));

        // The values of install.txt and of the content are untrusted, and shown as plain text.
        m_summary = new QLabel();
        m_summary->setTextFormat(Qt::PlainText);
        m_summary->setWordWrap(true);
        m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);

        m_names = new QGroupBox(tr("File names"));
        auto namesNote = new QLabel(tr("The archive does not state the encoding of its file names. "
                                       "Choose the encoding in which the names on the Files tab "
                                       "read correctly."));
        namesNote->setWordWrap(true);
        m_nameEncoding = new QComboBox();
        m_nameEncoding->addItems(kit::TextCodec::ustCandidates());
        m_nameMode = new QWidget();
        m_decoded = new QRadioButton(tr("Write the names as they read in this encoding"));
        m_misdecoded = new QRadioButton(tr("Write the names as UTAU writes them on this computer"));
        m_nameModes = new QButtonGroup(this);
        m_nameModes->addButton(m_decoded, kit::InstallArchive::Decoded);
        m_nameModes->addButton(m_misdecoded, kit::InstallArchive::Misdecoded);
        m_decoded->setChecked(true);
        m_nameNote = new QLabel();
        m_nameNote->setWordWrap(true);
        auto modeLayout = new QVBoxLayout(m_nameMode);
        modeLayout->setContentsMargins(0, 0, 0, 0);
        modeLayout->addWidget(m_decoded);
        modeLayout->addWidget(m_misdecoded);
        modeLayout->addWidget(m_nameNote);
        auto namesForm = new QFormLayout();
        namesForm->addRow(tr("Encoding:"), m_nameEncoding);
        auto namesLayout = new QVBoxLayout(m_names);
        namesLayout->addWidget(namesNote);
        namesLayout->addLayout(namesForm);
        namesLayout->addWidget(m_nameMode);

        m_text = new QGroupBox(tr("Text files"));
        auto textNote = new QLabel(tr("Choose the encoding in which the text files of the voice "
                                      "bank read correctly. The voice bank records it, so that it "
                                      "opens without this question."));
        textNote->setWordWrap(true);
        m_charset = new QComboBox();
        m_charset->addItems(kit::TextCodec::ustCandidates());
        auto textForm = new QFormLayout();
        textForm->addRow(tr("Encoding:"), m_charset);
        auto textLayout = new QVBoxLayout(m_text);
        textLayout->addWidget(textNote);
        textLayout->addLayout(textForm);

        m_files = new QTreeWidget();
        m_files->setHeaderHidden(true);
        m_previews = new QTabWidget();
        m_previews->addTab(m_files, tr("Files"));

        m_error = new QLabel();
        m_error->setTextFormat(Qt::PlainText);
        m_error->setWordWrap(true);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
        m_install = buttons->addButton(tr("Install"), QDialogButtonBox::AcceptRole);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(m_summary);
        layout->addWidget(m_names);
        layout->addWidget(m_text);
        layout->addWidget(m_previews, 1);
        layout->addWidget(m_error);
        layout->addWidget(buttons);

        // The encoding of the names that reads them best, among those that can decode them
        if (m_archive.needsNameEncoding()) {
            grayCandidates(m_nameEncoding,
                           [this](const QString &name) { return !m_archive.canDecode(name); });
            auto ranked =
                kit::TextCodec::ranked(m_archive.rawTexts(), kit::TextCodec::ustCandidates());
            const auto decodable =
                std::find_if(ranked.cbegin(), ranked.cend(),
                             [this](const QString &name) { return m_archive.canDecode(name); });
            m_nameEncoding->setCurrentText(decodable != ranked.cend() ? *decodable
                                                                      : ranked.value(0));
        } else {
            m_names->hide();
        }
        decodeNames();

        connect(m_nameEncoding, &QComboBox::currentIndexChanged, this,
                &InstallArchiveDialog::decodeNames);
        connect(m_nameModes, &QButtonGroup::idToggled, this, [this](int, bool checked) {
            if (checked) {
                updateNameMode();
            }
        });
        // The encoding that the user chooses is kept when the names are decoded again.
        connect(m_charset, &QComboBox::activated, this, [this] { m_charsetChosen = true; });
        connect(m_charset, &QComboBox::currentIndexChanged, this, [this] {
            updatePreviews();
            updateSummary();
        });
        resize(720, 560);
    }

    InstallArchiveDialog::~InstallArchiveDialog() = default;

    bool InstallArchiveDialog::isInstallArchive(const fs::path &path) {
        const auto extension = QString::fromStdU16String(path.extension().u16string()).toLower();
        if (extension == u".uar") {
            return true;
        }
        if (extension != u".zip") {
            return false;
        }
        kit::ZipExtractor extractor;
        const auto entries = extractor.listEntries(path);
        return entries && std::any_of(entries->cbegin(), entries->cend(), [](const auto &entry) {
                   return !entry.directory && entry.name.compare(kit::InstallArchive::manifestName,
                                                                 Qt::CaseInsensitive) == 0;
               });
    }

    fs::path InstallArchiveDialog::destinationOf(kit::InstallArchive::Kind kind,
                                                 const AppSettings &settings) {
        return settings.classicRoot(AppSettings::HelloUtauRoot) /
               (kind == kit::InstallArchive::VoiceBank ? u"voice" : u"plugins");
    }

    std::optional<fs::path>
        InstallArchiveDialog::chooseArchive(QWidget *parent,
                                            std::optional<kit::InstallArchive::Kind> kind) {
        const auto title = !kind ? tr("Install Voice Bank or Plugin")
                           : *kind == kit::InstallArchive::VoiceBank ? tr("Install Voice Bank")
                                                                     : tr("Install Plugin");
        const auto file = QFileDialog::getOpenFileName(
            parent, title, {}, tr("Installation archives of UTAU (*.uar *.zip);;All files (*)"));
        if (file.isEmpty()) {
            return std::nullopt;
        }
        return pathOf(file);
    }

    std::optional<InstallArchiveDialog::Installation>
        InstallArchiveDialog::install(const fs::path &path, const AppSettings &settings,
                                      QWidget *parent,
                                      std::optional<kit::InstallArchive::Kind> only) {
        const auto title =
            tr("Install %1").arg(QString::fromStdU16String(path.filename().u16string()));
        const auto fail = [&](const QString &reason) {
            QMessageBox::warning(parent, title,
                                 tr("%1 cannot be installed.\n\n%2").arg(textOf(path), reason));
            return std::nullopt;
        };

        kit::InstallArchive archive;
        if (!archive.open(path)) {
            return fail(archive.errorString());
        }
        // Returns the reason to refuse an archive of another kind than only, or std::nullopt.
        const auto wrongKind = [&]() -> std::optional<QString> {
            if (!only || archive.kind() == *only) {
                return std::nullopt;
            }
            return archive.kind() == kit::InstallArchive::VoiceBank
                       ? tr("The archive holds a voice bank rather than a plugin. Install it "
                            "with Install Voice Bank... on the UTAU page.")
                       : tr("The archive holds a plugin rather than a voice bank. Install it "
                            "with Install Plugin... on the Classic Plugins page.");
        };
        InstallArchiveDialog dialog(archive, parent);
        dialog.setWindowTitle(title);
        // Without a choice of the encoding, the dialog could not make the archive installable.
        if (!dialog.isInstallable() && !archive.needsNameEncoding()) {
            return fail(dialog.errorText());
        }
        if (dialog.isInstallable()) {
            if (const auto reason = wrongKind()) {
                return fail(*reason);
            }
        }
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        // The kind is known once the names are decoded, which the dialog may have done only now.
        if (const auto reason = wrongKind()) {
            return fail(*reason);
        }

        const auto kind = archive.kind();
        const auto root = destinationOf(kind, settings);
        // The folders of HelloUtau are created on demand, unlike the folders of UTAU.
        std::error_code error;
        fs::create_directories(root, error);
        if (error) {
            return fail(tr("The folder %1 cannot be created: %2")
                            .arg(textOf(root), kit::FolderAccess::reasonOf(error)));
        }

        // A folder that exists is merged into, as UTAU does, or the content goes into a new
        // folder named as UTAU names it.
        auto folderName = archive.folder();
        if (fs::exists(root / pathOf(folderName), error)) {
            const auto free = kit::InstallArchive::freeFolderName(root, folderName);
            QMessageBox box(QMessageBox::Question, title,
                            tr("The folder %1 already exists in %2.").arg(folderName, textOf(root)),
                            QMessageBox::Cancel, parent);
            box.setInformativeText(tr("Overwrite replaces the files of the same names and keeps "
                                      "the other files of the folder. A new folder leaves the "
                                      "folder as it is."));
            const auto overwrite = box.addButton(tr("Overwrite"), QMessageBox::AcceptRole);
            const auto newFolder =
                box.addButton(tr("Install into %1").arg(free), QMessageBox::AcceptRole);
            box.setDefaultButton(newFolder);
            box.exec();
            if (box.clickedButton() == newFolder) {
                folderName = free;
            } else if (box.clickedButton() != overwrite) {
                return std::nullopt;
            }
        }

        QProgressDialog progress(tr("Installing %1...").arg(folderName), tr("Cancel"), 0, 1000,
                                 parent);
        progress.setWindowTitle(title);
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(500);
        // setValue() processes the events of a modal progress dialog, which include the click
        // on Cancel.
        const auto handler = [&progress](qint64 written, qint64 total) {
            progress.setValue(total > 0 ? int(written * 1000 / total) : 0);
            return !progress.wasCanceled();
        };
        const bool installed =
            archive.install(root, folderName, dialog.nameMode(), dialog.charset(), handler);
        const bool canceled = progress.wasCanceled();
        progress.reset();
        if (!installed) {
            if (canceled) {
                return std::nullopt;
            }
            return fail(archive.errorString());
        }
        return Installation{kind, dialog.contentName(), root / pathOf(folderName)};
    }

    void InstallArchiveDialog::reportInstalled(const Installation &installation, QWidget *parent) {
        const auto folder = textOf(installation.folder);
        QMessageBox::information(
            parent, tr("Install"),
            installation.kind == kit::InstallArchive::VoiceBank
                ? tr("The voice bank %1 is installed in %2.").arg(installation.name, folder)
                : tr("The plugin %1 is installed in %2. It is listed in Classic Plugins of the "
                     "Tools menu.")
                      .arg(installation.name, folder));
    }

    bool InstallArchiveDialog::isInstallable() const {
        return m_installable;
    }

    QString InstallArchiveDialog::errorText() const {
        return m_error->text();
    }

    QString InstallArchiveDialog::summaryText() const {
        return m_summary->text();
    }

    QString InstallArchiveDialog::contentName() const {
        const auto required = m_archive.kind() == kit::InstallArchive::VoiceBank
                                  ? QStringLiteral("character.txt")
                                  : QStringLiteral("plugin.txt");
        const auto file =
            std::find_if(m_textFiles.cbegin(), m_textFiles.cend(), [&](const TextFile &file) {
                return file.path.compare(required, Qt::CaseInsensitive) == 0;
            });
        QString name;
        if (file != m_textFiles.cend()) {
            // UTAU reads plugin.txt in the ANSI code page, see the plugins in docs/note.md.
            const kit::TextCodec codec(m_archive.kind() == kit::InstallArchive::VoiceBank
                                           ? charset()
                                           : kit::TextCodec::systemName());
            name = valueOf(codec.decodeReplacing(file->bytes), QLatin1String("name"));
        }
        return name.isEmpty() ? m_archive.folder() : name;
    }

    QString InstallArchiveDialog::nameEncoding() const {
        return m_archive.needsNameEncoding() ? m_nameEncoding->currentText() : QString();
    }

    void InstallArchiveDialog::setNameEncoding(const QString &encoding) {
        m_nameEncoding->setCurrentText(encoding);
    }

    bool InstallArchiveDialog::isNameEncodingGray(const QString &encoding) const {
        return isGrayIn(m_nameEncoding, encoding);
    }

    bool InstallArchiveDialog::isNameModeShown() const {
        // Without either choice or the reason against the names as they read, the names are
        // written as they read without a question, such as on a system whose encoding is UTF-8.
        return m_installable && m_archive.needsNameEncoding() &&
               kit::TextCodec::systemName().compare(nameEncoding(), Qt::CaseInsensitive) != 0 &&
               (m_archive.canWriteMisdecoded() || !m_archive.unrepresentableNames().isEmpty());
    }

    bool InstallArchiveDialog::isMisdecodedAvailable() const {
        return m_installable && m_archive.canWriteMisdecoded();
    }

    kit::InstallArchive::NameMode InstallArchiveDialog::nameMode() const {
        return isNameModeShown() && m_misdecoded->isChecked() ? kit::InstallArchive::Misdecoded
                                                              : kit::InstallArchive::Decoded;
    }

    void InstallArchiveDialog::setNameMode(kit::InstallArchive::NameMode mode) {
        (mode == kit::InstallArchive::Misdecoded ? m_misdecoded : m_decoded)->setChecked(true);
    }

    QStringList InstallArchiveDialog::contentPaths() const {
        return m_installable ? m_archive.contents() : QStringList();
    }

    QString InstallArchiveDialog::charset() const {
        return m_installable && m_archive.kind() == kit::InstallArchive::VoiceBank
                   ? m_charset->currentText()
                   : QString();
    }

    void InstallArchiveDialog::setCharset(const QString &charset) {
        m_charset->setCurrentText(charset);
    }

    bool InstallArchiveDialog::isCharsetGray(const QString &charset) const {
        return isGrayIn(m_charset, charset);
    }

    QStringList InstallArchiveDialog::previewTitles() const {
        QStringList titles;
        for (int i = 0; i < m_previews->count(); ++i) {
            titles.push_back(m_previews->tabText(i));
        }
        return titles;
    }

    QString InstallArchiveDialog::previewText(int index) const {
        const auto edit = qobject_cast<QPlainTextEdit *>(m_previews->widget(index));
        return edit ? edit->toPlainText() : QString();
    }

    void InstallArchiveDialog::decodeNames() {
        m_installable = m_archive.setNameEncoding(m_nameEncoding->currentText());
        m_error->setText(m_installable ? QString() : m_archive.errorString());
        m_error->setVisible(!m_installable);
        m_install->setEnabled(m_installable);
        fillTree(m_files, contentPaths());
        readTextFiles();
        updateNameMode();
        updateCharsets();
        updatePreviews();
        updateSummary();
    }

    void InstallArchiveDialog::readTextFiles() {
        m_textFiles.clear();
        if (!m_installable) {
            return;
        }
        // The name of the content first, then its other text files
        const auto voiceBank = m_archive.kind() == kit::InstallArchive::VoiceBank;
        const QStringList names =
            voiceBank ? QStringList{QStringLiteral("character.txt"), QStringLiteral("readme.txt"),
                                    QStringLiteral("oto.ini")}
                      : QStringList{QStringLiteral("plugin.txt")};
        const auto paths = m_archive.contents();
        for (const auto &name : names) {
            for (const auto &path : paths) {
                if (m_textFiles.size() >= previewMaximumCount) {
                    return;
                }
                // character.txt, readme.txt and plugin.txt at the root, oto.ini in any folder
                const bool matches = name == u"oto.ini"
                                         ? fileNameOf(path).compare(name, Qt::CaseInsensitive) == 0
                                         : path.compare(name, Qt::CaseInsensitive) == 0;
                if (!matches) {
                    continue;
                }
                if (const auto bytes = m_archive.readContent(path, previewMaximumSize)) {
                    m_textFiles.push_back({path, *bytes});
                }
            }
        }
    }

    void InstallArchiveDialog::updateNameMode() {
        const bool shown = isNameModeShown();
        m_nameMode->setVisible(shown);
        if (!shown) {
            return;
        }
        const auto system = kit::TextCodec::systemName();
        const bool available = m_archive.canWriteMisdecoded();
        m_misdecoded->setEnabled(available);
        if (!available) {
            m_decoded->setChecked(true);
        }
        QStringList notes;
        if (m_misdecoded->isChecked()) {
            notes.push_back(tr("The names are written as misdecoded in %1, the encoding of this "
                               "computer, as UTAU writes them. HelloUtau reads them as the "
                               "names in %2.")
                                .arg(system, nameEncoding()));
        } else {
            if (const auto count = m_archive.unrepresentableNames().size(); count > 0) {
                notes.push_back(tr("%n file name(s) cannot be represented in %1, the encoding of "
                                   "this computer. UTAU on this computer cannot open these files.",
                                   nullptr, int(count))
                                    .arg(system));
            }
            if (!available) {
                notes.push_back(tr("The names cannot be written as UTAU writes them, because some "
                                   "names are invalid in %1.")
                                    .arg(system));
            }
        }
        m_nameNote->setText(notes.join(u'\n'));
        m_nameNote->setVisible(!notes.isEmpty());
    }

    void InstallArchiveDialog::updateCharsets() {
        const bool shown = m_installable && m_archive.kind() == kit::InstallArchive::VoiceBank;
        m_text->setVisible(shown);
        if (!shown) {
            return;
        }
        QList<QByteArray> texts;
        for (const auto &file : std::as_const(m_textFiles)) {
            texts.push_back(file.bytes);
        }
        grayCandidates(m_charset, [&texts](const QString &name) {
            const kit::TextCodec codec(name);
            return std::any_of(texts.cbegin(), texts.cend(), [&codec](const QByteArray &text) {
                return !codec.decode(text).has_value();
            });
        });
        if (!m_charsetChosen) {
            const QSignalBlocker blocker(m_charset);
            m_charset->setCurrentText(
                kit::TextCodec::ranked(texts, kit::TextCodec::ustCandidates()).value(0));
        }
    }

    void InstallArchiveDialog::updatePreviews() {
        while (m_previews->count() > 1) {
            const auto page = m_previews->widget(1);
            m_previews->removeTab(1);
            delete page;
        }
        const auto voiceBank = m_archive.kind() == kit::InstallArchive::VoiceBank;
        const kit::TextCodec codec(voiceBank ? m_charset->currentText()
                                             : kit::TextCodec::systemName());
        for (const auto &file : std::as_const(m_textFiles)) {
            qsizetype invalid = 0;
            const auto text = codec.decodeReplacing(file.bytes, &invalid);
            auto edit = new QPlainTextEdit(text);
            edit->setReadOnly(true);
            m_previews->addTab(
                edit,
                invalid > 0
                    ? tr("%1 (%n invalid byte sequence(s))", nullptr, int(invalid)).arg(file.path)
                    : file.path);
        }
    }

    void InstallArchiveDialog::updateSummary() {
        if (!m_installable) {
            m_summary->setText(tr("This archive cannot be installed in the selected encoding."));
            return;
        }
        QStringList lines{m_archive.kind() == kit::InstallArchive::VoiceBank
                              ? tr("Voice bank: %1").arg(contentName())
                              : tr("Plugin: %1").arg(contentName()),
                          tr("Folder: %1").arg(m_archive.folder())};
        if (const auto description = m_archive.description().trimmed(); !description.isEmpty()) {
            lines.push_back(tr("Description: %1").arg(description));
        }
        m_summary->setText(lines.join(u'\n'));
    }

}
