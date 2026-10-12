#include "VoiceBankCharsetDialog.h"

#include <QtCore/QDir>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTreeWidget>
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

        // The names of question that it proposes as misdecoded
        QStringList proposedOf(const kit::MisdecodedNameQuestion &question) {
            QStringList names;
            for (const auto &choice : question.choices()) {
                if (choice.misdecoded) {
                    names.push_back(choice.name);
                }
            }
            return names;
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

        // The page of the file names, which the tabs show for a folder with names to determine
        m_names = new QWidget(this);
        m_namesBasis = new QLabel();
        m_namesBasis->setWordWrap(true);
        m_original = new QComboBox();
        m_original->addItems(kit::TextCodec::ustCandidates());
        m_misdecodedAs = new QComboBox();
        m_misdecodedAs->addItems(kit::TextCodec::ustCandidates());
        m_asTheyAre = new QCheckBox(tr("&Use the names as they are"));
        m_nameList = new QTreeWidget();
        m_nameList->setRootIsDecorated(false);
        m_nameList->setHeaderLabels({tr("Name on Disk"), tr("Restored Name")});
        m_nameList->header()->setSectionResizeMode(QHeaderView::Stretch);
        auto encodings = new QFormLayout();
        encodings->addRow(tr("&Written in:"), m_original);
        encodings->addRow(tr("&Read as:"), m_misdecodedAs);
        auto names = new QVBoxLayout(m_names);
        names->addWidget(m_namesBasis);
        names->addLayout(encodings);
        names->addWidget(m_asTheyAre);
        names->addWidget(m_nameList, 1);
        m_names->hide();

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
        m_skip = buttons->addButton(tr("&Skip Folder"), QDialogButtonBox::RejectRole);
        m_skip->setToolTip(tr("Leave the folder out. Its samples are then found by file name "
                              "only, and it is asked again next time."));
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto choices = new QSplitter(Qt::Vertical);
        choices->setChildrenCollapsible(false);
        choices->addWidget(m_folders);
        choices->addWidget(m_charsets);
        auto content = new QSplitter();
        content->setChildrenCollapsible(false);
        content->addWidget(choices);
        content->addWidget(m_previews);
        content->setStretchFactor(0, 1);
        content->setStretchFactor(1, 3);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(m_message);
        layout->addWidget(content, 1);
        layout->addWidget(buttons);

        connect(m_folders, &QListWidget::currentRowChanged, this, [this] { showCurrent(); });
        connect(m_folders, &QListWidget::itemChanged, this, [this] { updateFolders(); });
        connect(m_charsets, &QListWidget::currentRowChanged, this, [this] {
            if (const auto item = m_charsets->currentItem()) {
                setSelectedCharset(item->text());
            }
        });
        const auto chooseEncodings = [this] {
            setFileNameEncodings(currentFolder(), m_original->currentText(),
                                 m_misdecodedAs->currentText());
        };
        connect(m_original, &QComboBox::currentTextChanged, this, chooseEncodings);
        connect(m_misdecodedAs, &QComboBox::currentTextChanged, this, chooseEncodings);
        connect(m_asTheyAre, &QCheckBox::toggled, this,
                [this](bool checked) { setNamesAsTheyAre(currentFolder(), checked); });
        connect(m_nameList, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item) {
            setMisdecoded(currentFolder(), item->data(0, Qt::UserRole).toString(),
                          item->checkState(0) == Qt::Checked);
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
        QList<Question> questions;
        for (const auto directory : directories) {
            questions.push_back({directory, std::nullopt});
        }
        QList<std::optional<QString>> charsets;
        for (const auto &answer : selectEncodings(questions, diagnostics)) {
            charsets.push_back(answer.charset);
        }
        return charsets;
    }

    QList<VoiceBankCharsetDialog::Answer>
        VoiceBankCharsetDialog::selectEncodings(const QList<Question> &questions,
                                                kit::DiagnosticList &diagnostics) {
        Q_UNUSED(diagnostics);
        setQuestions(questions);
        QList<Answer> result;
        if (exec() == QDialog::Accepted) {
            result = answers();
        } else {
            // Declined: every folder chosen is left out, and every name is used as it is.
            for (const auto &question : questions) {
                Answer answer;
                answer.charset = question.charset;
                result.push_back(answer);
            }
        }
        m_directories.clear();
        m_choices.clear();
        m_settled.clear();
        m_fileNames.clear();
        m_folders->clear();
        return result;
    }

    void VoiceBankCharsetDialog::setDirectory(const kit::VoiceBankDirectorySource &directory) {
        setDirectories({&directory});
    }

    void VoiceBankCharsetDialog::setDirectories(
        const QList<const kit::VoiceBankDirectorySource *> &directories) {
        QList<Question> questions;
        for (const auto directory : directories) {
            questions.push_back({directory, std::nullopt});
        }
        setQuestions(questions);
    }

    void VoiceBankCharsetDialog::setQuestions(const QList<Question> &questions) {
        m_directories.clear();
        m_settled.clear();
        for (const auto &question : questions) {
            m_directories.push_back(question.directory);
            m_settled.push_back(question.charset);
        }

        // The encoding that reads all folders to choose best, or for a folder that it cannot
        // read without invalid bytes, the encoding that reads that folder best
        const auto candidates = kit::TextCodec::ustCandidates();
        QList<QByteArray> all;
        for (int i = 0; i < m_directories.size(); ++i) {
            if (!m_settled[i]) {
                all.append(textsOf(*m_directories[i]));
            }
        }
        const auto best = kit::TextCodec::ranked(all, candidates).value(0);
        m_choices.clear();
        for (int i = 0; i < m_directories.size(); ++i) {
            if (m_settled[i]) {
                m_choices.push_back(*m_settled[i]);
                continue;
            }
            const auto texts = textsOf(*m_directories[i]);
            m_choices.push_back(invalidIn(texts, best) == 0
                                    ? best
                                    : kit::TextCodec::ranked(texts, candidates).value(0));
        }
        m_fileNames = QList<FileNames>(m_directories.size());
        for (int i = 0; i < m_directories.size(); ++i) {
            askFileNames(i);
        }

        updateMessage();

        m_current.clear();
        const QSignalBlocker blocker(m_folders);
        m_folders->clear();
        for (int i = 0; i < m_directories.size(); ++i) {
            auto item = new QListWidgetItem(m_folders);
            if (!m_settled[i]) {
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Checked);
            }
        }
        m_folders->setCurrentRow(0, QItemSelectionModel::NoUpdate);
        m_folders->selectAll();
        updateFolders();
        showCurrent();
    }

    void VoiceBankCharsetDialog::updateMessage() {
        const auto bank = QString::fromStdU16String(m_root.filename().u16string());
        int unsettled = 0;
        int named = 0;
        for (int i = 0; i < m_directories.size(); ++i) {
            unsettled += !m_settled[i];
            named += m_fileNames[i].question.has_value();
        }
        const bool single = m_directories.size() == 1;
        m_folders->setVisible(!single);
        m_charsets->setVisible(unsettled > 0);

        if (unsettled == 0) {
            setWindowTitle(tr("Determine File Names - %1").arg(bank));
            m_message->setText(
                tr("Some file names of the voice bank \"%1\" are to be determined. A misdecoded "
                   "name was written in one encoding and read in another when the voice bank was "
                   "extracted. Check the misdecoded names, which are then read restored, while "
                   "the others are read as they are. The choices are recorded in the folders.")
                    .arg(textOf(m_root)));
            m_skip->setText(tr("&Later"));
            m_skip->setToolTip(tr("Use the names as they are this time, and ask again next "
                                  "time."));
            return;
        }

        m_skip->setText(single ? tr("&Skip Folder") : tr("&Skip All"));
        m_skip->setToolTip(single ? tr("Leave the folder out. Its samples are then found by file "
                                       "name only, and it is asked again next time.")
                                  : tr("Leave every folder out. Their samples are then found by "
                                       "file name only, and they are asked again next time."));
        QString text;
        if (single) {
            const auto &path = m_directories.first()->path;
            const auto folder = path.empty() ? bank : textOf(path);
            setWindowTitle(tr("Choose Encoding - %1").arg(folder));
            text = path.empty()
                       ? tr("The voice bank \"%1\" does not state the encoding of its files. "
                            "Choose the encoding in which their text reads correctly. The choice "
                            "is recorded in the folder of the voice bank.")
                             .arg(textOf(m_root))
                       : tr("The folder \"%1\" of the voice bank \"%2\" does not state the "
                            "encoding of its files. Choose the encoding in which their text reads "
                            "correctly. The choice is recorded in that folder.")
                             .arg(folder, textOf(m_root));
        } else {
            setWindowTitle(tr("Choose Encoding - %1").arg(bank));
            text = tr("The voice bank \"%1\" does not state the encoding of %n folder(s). Choose "
                      "the encoding in which their text reads correctly: it is set for the "
                      "folders selected, all of them at first. Uncheck a folder to leave it out. "
                      "The choices are recorded in the folders.",
                      nullptr, unsettled)
                       .arg(textOf(m_root));
        }
        if (named > 0) {
            text += QLatin1Char(' ') + tr("Some file names are to be determined as well, on the "
                                          "tab File Names of their folders.");
        }
        m_message->setText(text);
    }

    QList<VoiceBankCharsetDialog::Answer> VoiceBankCharsetDialog::answers() const {
        QList<Answer> result;
        for (int i = 0; i < m_directories.size(); ++i) {
            Answer answer;
            const bool read = m_settled[i] || isIncluded(i);
            answer.charset = m_settled[i] ? m_settled[i]
                             : read       ? std::optional(m_choices[i])
                                          : std::nullopt;
            const auto &names = m_fileNames[i];
            if (read && names.question) {
                answer.fileNameEncoding = names.asTheyAre
                                              ? kit::VoiceBankConfig::FileNameEncoding()
                                              : names.question->answer(names.misdecoded);
            }
            result.push_back(answer);
        }
        return result;
    }

    void VoiceBankCharsetDialog::askFileNames(int index) {
        auto &names = m_fileNames[index];
        const auto &directory = *m_directories[index];
        const auto &charset = m_choices[index];
        const bool recorded = directory.config && directory.config->fileNameEncoding;
        if (!names.original.isEmpty() && !recorded) {
            names.question = kit::MisdecodedNameQuestion::of(directory, charset, names.original,
                                                             names.misdecodedAs);
            if (names.question->choices().isEmpty()) {
                names.question.reset();
            }
        } else {
            names.question = kit::MisdecodedNameQuestion::of(directory, charset);
        }
        names.misdecoded = names.question ? proposedOf(*names.question) : QStringList();
    }

    void VoiceBankCharsetDialog::setCurrentCharsets(const QStringList &charsets) {
        m_current = charsets;
        for (int i = 0; i < m_choices.size() && i < charsets.size(); ++i) {
            m_choices[i] = charsets[i];
        }
        // An encoding outside the candidates is selectable for the folder read in it.
        for (const auto &charset : charsets) {
            if (!charset.isEmpty() && m_charsets->findItems(charset, Qt::MatchExactly).isEmpty()) {
                m_charsets->insertItem(0, charset);
            }
        }

        const auto bank = QString::fromStdU16String(m_root.filename().u16string());
        setWindowTitle(tr("Read Folders Again in Encodings - %1").arg(bank));
        m_message->setText(
            tr("Choose the encoding of each folder of the voice bank \"%1\". A folder whose "
               "encoding is changed is read again from disk in the new encoding: its text "
               "changes, and its unsaved changes are discarded. Undo restores the folders. The "
               "encodings are recorded in the folders when the voice bank is saved.")
                .arg(textOf(m_root)));
        m_skip->setText(tr("Cancel"));
        m_skip->setToolTip({});

        // Reading again concerns the encodings only.
        for (auto &names : m_fileNames) {
            names = FileNames();
        }
        const QSignalBlocker blocker(m_folders);
        for (int i = 0; i < m_folders->count(); ++i) {
            const auto item = m_folders->item(i);
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setData(Qt::CheckStateRole, {});
        }
        updateFolders();
        showCurrent();
    }

    QList<int> VoiceBankCharsetDialog::changedFolders() const {
        QList<int> changed;
        for (int i = 0; i < m_choices.size() && i < m_current.size(); ++i) {
            if (m_choices[i] != m_current[i]) {
                changed.push_back(i);
            }
        }
        return changed;
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
            if (row < m_choices.size() && !m_settled.value(row) && m_choices[row] != charset) {
                m_choices[row] = charset;
                if (m_current.isEmpty()) {
                    askFileNames(row);
                }
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

    const std::optional<kit::MisdecodedNameQuestion> &
        VoiceBankCharsetDialog::misdecodedNameQuestion(int index) const {
        static const std::optional<kit::MisdecodedNameQuestion> none;
        return index >= 0 && index < m_fileNames.size() ? m_fileNames[index].question : none;
    }

    void VoiceBankCharsetDialog::setFileNameEncodings(int index, const QString &original,
                                                      const QString &misdecodedAs) {
        if (index < 0 || index >= m_fileNames.size()) {
            return;
        }
        auto &names = m_fileNames[index];
        const auto &question = names.question;
        if (question && question->original() == kit::TextCodec(original).name() &&
            question->misdecodedAs() == kit::TextCodec(misdecodedAs).name()) {
            return;
        }
        names.original = original;
        names.misdecodedAs = misdecodedAs;
        askFileNames(index);
        if (!names.question) {
            // The encodings chosen restore no name, which the page still shows as chosen.
            names.question = kit::MisdecodedNameQuestion::of(
                *m_directories[index], m_choices[index], original, misdecodedAs);
        }
        updateFolders();
        if (index == currentFolder()) {
            showFileNames();
        }
    }

    QStringList VoiceBankCharsetDialog::misdecodedNames(int index) const {
        return index >= 0 && index < m_fileNames.size() ? m_fileNames[index].misdecoded
                                                        : QStringList();
    }

    void VoiceBankCharsetDialog::setMisdecoded(int index, const QString &name, bool misdecoded) {
        if (index < 0 || index >= m_fileNames.size()) {
            return;
        }
        auto &names = m_fileNames[index].misdecoded;
        names.removeAll(name);
        if (misdecoded) {
            names.push_back(name);
        }
        if (index == currentFolder()) {
            const QSignalBlocker blocker(m_nameList);
            for (int i = 0; i < m_nameList->topLevelItemCount(); ++i) {
                const auto item = m_nameList->topLevelItem(i);
                if (item->data(0, Qt::UserRole).toString() == name) {
                    item->setCheckState(0, misdecoded ? Qt::Checked : Qt::Unchecked);
                }
            }
        }
    }

    bool VoiceBankCharsetDialog::namesAsTheyAre(int index) const {
        return index >= 0 && index < m_fileNames.size() && m_fileNames[index].asTheyAre;
    }

    void VoiceBankCharsetDialog::setNamesAsTheyAre(int index, bool asTheyAre) {
        if (index < 0 || index >= m_fileNames.size()) {
            return;
        }
        m_fileNames[index].asTheyAre = asTheyAre;
        if (index == currentFolder()) {
            showFileNames();
        }
    }

    void VoiceBankCharsetDialog::updateFolders() {
        const auto bank = QString::fromStdU16String(m_root.filename().u16string());
        const QSignalBlocker blocker(m_folders);
        for (int i = 0; i < m_directories.size() && i < m_folders->count(); ++i) {
            const auto &path = m_directories[i]->path;
            const auto name = path.empty() ? tr("%1 (the voice bank)").arg(bank) : textOf(path);
            const auto invalid = invalidIn(textsOf(*m_directories[i]), m_choices[i]);
            auto text =
                m_choices[i].isEmpty() ? name : QStringLiteral("%1  -  %2").arg(name, m_choices[i]);
            if (i < m_current.size() && m_choices[i] != m_current[i]) {
                text += QLatin1String("  ") + tr("(currently %1)").arg(m_current[i]);
            }
            if (invalid > 0) {
                text += QLatin1String("  ") + tr("(%n invalid)", nullptr, int(invalid));
            }
            if (const auto &question = m_fileNames.value(i).question) {
                text += QLatin1String("  ") + tr("(%n file name(s) to determine)", nullptr,
                                                 int(question->choices().size()));
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
        m_charsets->setEnabled(!m_settled.value(current));
        for (int i = 0; i < m_charsets->count(); ++i) {
            auto item = m_charsets->item(i);
            item->setForeground(invalidIn(texts, item->text()) > 0 ? grey : QBrush());
            if (item->text() == m_choices[current]) {
                m_charsets->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
            }
        }

        // The pages of the text files are made anew, and the page of the file names is kept.
        const int names = m_previews->indexOf(m_names);
        if (names >= 0) {
            m_previews->removeTab(names);
        }
        while (m_previews->count() > 0) {
            const auto page = m_previews->widget(0);
            m_previews->removeTab(0);
            page->deleteLater();
        }
        for (int i = 0; i < int(directory->textFiles().size()); ++i) {
            auto edit = new QPlainTextEdit();
            edit->setReadOnly(true);
            edit->setLineWrapMode(QPlainTextEdit::NoWrap);
            m_previews->addTab(edit, {});
        }
        updatePreview();
        showFileNames();
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

    void VoiceBankCharsetDialog::showFileNames() {
        const int current = currentFolder();
        const auto &names = m_fileNames.value(current);
        const int page = m_previews->indexOf(m_names);
        if (!names.question) {
            if (page >= 0) {
                m_previews->removeTab(page);
            }
            return;
        }
        const auto &question = *names.question;
        if (page < 0) {
            m_previews->addTab(m_names, {});
        }
        m_previews->setTabText(m_previews->indexOf(m_names),
                               tr("File Names (%1)").arg(question.choices().size()));

        if (question.isRecorded()) {
            m_namesBasis->setText(
                tr("These names are not yet in the record of the folder, which restores names "
                   "written in %1 and read as %2.")
                    .arg(question.original(), question.misdecodedAs()));
        } else {
            m_namesBasis->setText(
                tr("%n entry(s) of the oto.ini find no file. Restored as written in %1 and read "
                   "as %2, %3 of them find their files.",
                   nullptr, question.missingEntries())
                    .arg(question.original(), question.misdecodedAs())
                    .arg(question.matchedEntries()));
        }
        {
            const QSignalBlocker original(m_original);
            const QSignalBlocker misdecodedAs(m_misdecodedAs);
            const QSignalBlocker asTheyAre(m_asTheyAre);
            m_original->setCurrentText(question.original());
            m_misdecodedAs->setCurrentText(question.misdecodedAs());
            m_original->setEnabled(!question.isRecorded() && !names.asTheyAre);
            m_misdecodedAs->setEnabled(!question.isRecorded() && !names.asTheyAre);
            m_asTheyAre->setVisible(!question.isRecorded());
            m_asTheyAre->setChecked(names.asTheyAre);
        }

        const QSignalBlocker blocker(m_nameList);
        m_nameList->clear();
        m_nameList->setEnabled(!names.asTheyAre);
        const auto separator = QDir::separator();
        for (const auto &choice : question.choices()) {
            auto item = new QTreeWidgetItem(m_nameList);
            item->setText(0, choice.directory ? choice.name + separator : choice.name);
            item->setText(1, choice.directory ? choice.restored + separator : choice.restored);
            item->setData(0, Qt::UserRole, choice.name);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, names.misdecoded.contains(choice.name) ? Qt::Checked
                                                                          : Qt::Unchecked);
        }
    }

}
