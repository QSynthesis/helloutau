#include "VoiceBankTranscodingDialog.h"

#include <QtCore/QDir>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Support/TextCodec.h>

namespace hello::daw {

    namespace {

        QString textOf(const std::filesystem::path &path) {
            return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
        }

        // The candidates, with charset first if it is none of them
        QStringList candidatesWith(const QString &charset) {
            auto candidates = kit::TextCodec::ustCandidates();
            if (!charset.isEmpty() && !candidates.contains(charset)) {
                candidates.prepend(charset);
            }
            return candidates;
        }

    }

    VoiceBankTranscodingDialog::VoiceBankTranscodingDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle(tr("Convert Encodings"));

        m_message = new QLabel();
        m_message->setWordWrap(true);

        m_folders = new QListWidget();

        // The text files of the current folder
        m_text = new QGroupBox(tr("Text Files"));
        m_textNote = new QLabel();
        m_textNote->setWordWrap(true);
        m_convertText = new QCheckBox(tr("&Save the text files in:"));
        m_textCharset = new QComboBox();
        auto convert = new QHBoxLayout();
        convert->addWidget(m_convertText);
        convert->addWidget(m_textCharset, 1);
        auto text = new QVBoxLayout(m_text);
        text->addWidget(m_textNote);
        text->addLayout(convert);

        // The file names of the current folder
        m_names = new QGroupBox(tr("File Names"));
        m_namesNote = new QLabel();
        m_namesNote->setWordWrap(true);
        m_original = new QComboBox();
        m_original->addItems(kit::TextCodec::ustCandidates());
        m_misdecodedAs = new QComboBox();
        m_misdecodedAs->addItems(kit::TextCodec::ustCandidates());
        m_nameList = new QTreeWidget();
        m_nameList->setRootIsDecorated(false);
        m_nameList->setHeaderLabels({tr("Name on Disk"), tr("New Name")});
        m_nameList->header()->setSectionResizeMode(QHeaderView::Stretch);
        auto encodings = new QFormLayout();
        encodings->addRow(tr("&Written in:"), m_original);
        encodings->addRow(tr("&Read as:"), m_misdecodedAs);
        auto names = new QVBoxLayout(m_names);
        names->addLayout(encodings);
        names->addWidget(m_namesNote);
        names->addWidget(m_nameList, 1);

        auto current = new QWidget();
        auto currentLayout = new QVBoxLayout(current);
        currentLayout->setContentsMargins({});
        currentLayout->addWidget(m_text);
        currentLayout->addWidget(m_names, 1);

        auto content = new QSplitter();
        content->setChildrenCollapsible(false);
        content->addWidget(m_folders);
        content->addWidget(current);
        content->setStretchFactor(0, 1);
        content->setStretchFactor(1, 2);

        m_method = new QComboBox();
        m_method->addItem(tr("Keep the file names"), KeepNames);
        m_method->addItem(tr("Read the checked names as misdecoded"), ReadAsMisdecoded);
        m_method->addItem(tr("Rename the checked names on disk"), Rename);
        m_methodNote = new QLabel(
            tr("Renaming takes effect at once and cannot be undone. The voice bank is saved first, "
               "and its window opens again without the undo history. UTAU on a system in the "
               "encoding in which the names were read no longer finds the renamed files."));
        m_methodNote->setWordWrap(true);
        auto method = new QFormLayout();
        method->addRow(tr("&File names:"), m_method);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(m_message);
        layout->addWidget(content, 1);
        layout->addLayout(method);
        layout->addWidget(m_methodNote);
        layout->addWidget(buttons);

        connect(m_folders, &QListWidget::currentRowChanged, this, [this] { showCurrent(); });
        connect(m_convertText, &QCheckBox::toggled, this, [this](bool checked) {
            setTextCharset(currentFolder(), checked ? m_textCharset->currentText() : QString());
        });
        connect(m_textCharset, &QComboBox::currentTextChanged, this,
                [this](const QString &charset) {
                    if (m_convertText->isChecked()) {
                        setTextCharset(currentFolder(), charset);
                    }
                });
        const auto chooseEncodings = [this] {
            setFileNameEncodings(currentFolder(), m_original->currentText(),
                                 m_misdecodedAs->currentText());
        };
        connect(m_original, &QComboBox::currentTextChanged, this, chooseEncodings);
        connect(m_misdecodedAs, &QComboBox::currentTextChanged, this, chooseEncodings);
        connect(m_nameList, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item) {
            setChecked(currentFolder(), item->data(0, Qt::UserRole).toString(),
                       item->checkState(0) == Qt::Checked);
        });
        connect(m_method, &QComboBox::currentIndexChanged, this, [this] { updateMethod(); });
        updateMethod();
        resize(760, 560);
    }

    VoiceBankTranscodingDialog::~VoiceBankTranscodingDialog() = default;

    void VoiceBankTranscodingDialog::setFolders(const std::filesystem::path &root,
                                                const QList<Folder> &folders) {
        m_root = root;
        m_states.clear();
        for (const auto &folder : folders) {
            State state;
            state.folder = folder;
            state.names = kit::MisdecodedNameConversion::of(folder.listing);
            m_states.push_back(state);
            checkProposed(int(m_states.size()) - 1);
        }
        m_message->setText(
            tr("Convert the encodings of the voice bank \"%1\". The text files of a folder are "
               "converted if checked: their text stays the same, and their bytes change. The "
               "checked file names of every folder are converted by the method below.")
                .arg(textOf(m_root)));

        const QSignalBlocker blocker(m_folders);
        m_folders->clear();
        for (int i = 0; i < m_states.size(); ++i) {
            new QListWidgetItem(m_folders);
        }
        m_folders->setCurrentRow(0);
        updateFolders();
        showCurrent();
    }

    int VoiceBankTranscodingDialog::folderCount() const {
        return int(m_states.size());
    }

    QString VoiceBankTranscodingDialog::folderText(int index) const {
        const auto item = m_folders->item(index);
        return item ? item->text() : QString();
    }

    int VoiceBankTranscodingDialog::currentFolder() const {
        return m_states.isEmpty() ? -1 : std::max(0, m_folders->currentRow());
    }

    void VoiceBankTranscodingDialog::setCurrentFolder(int index) {
        m_folders->setCurrentRow(index);
    }

    QString VoiceBankTranscodingDialog::textCharset(int index) const {
        return index >= 0 && index < m_states.size() ? m_states[index].textCharset : QString();
    }

    void VoiceBankTranscodingDialog::setTextCharset(int index, const QString &charset) {
        if (index < 0 || index >= m_states.size()) {
            return;
        }
        auto &state = m_states[index];
        if (state.folder.excluded || state.folder.listing.charset.isEmpty()) {
            return;
        }
        state.textCharset = charset;
        updateFolders();
        if (index == currentFolder()) {
            showCurrent();
        }
    }

    const kit::MisdecodedNameConversion &
        VoiceBankTranscodingDialog::nameConversion(int index) const {
        return *m_states.at(index).names;
    }

    void VoiceBankTranscodingDialog::setFileNameEncodings(int index, const QString &original,
                                                          const QString &misdecodedAs) {
        if (index < 0 || index >= m_states.size()) {
            return;
        }
        auto &state = m_states[index];
        if (state.names->original() == kit::TextCodec(original).name() &&
            state.names->misdecodedAs() == kit::TextCodec(misdecodedAs).name()) {
            return;
        }
        state.names =
            kit::MisdecodedNameConversion::of(state.folder.listing, original, misdecodedAs);
        checkProposed(index);
        updateFolders();
        if (index == currentFolder()) {
            showCurrent();
        }
    }

    QStringList VoiceBankTranscodingDialog::checkedNames(int index) const {
        return index >= 0 && index < m_states.size() ? m_states[index].checked : QStringList();
    }

    void VoiceBankTranscodingDialog::setChecked(int index, const QString &name, bool checked) {
        if (index < 0 || index >= m_states.size()) {
            return;
        }
        auto &names = m_states[index].checked;
        names.removeAll(name);
        if (checked) {
            names.push_back(name);
        }
        updateFolders();
        if (index == currentFolder()) {
            const QSignalBlocker blocker(m_nameList);
            for (int i = 0; i < m_nameList->topLevelItemCount(); ++i) {
                const auto item = m_nameList->topLevelItem(i);
                if (item->data(0, Qt::UserRole).toString() == name) {
                    item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
                }
            }
        }
    }

    VoiceBankTranscodingDialog::Method VoiceBankTranscodingDialog::method() const {
        return Method(m_method->currentData().toInt());
    }

    void VoiceBankTranscodingDialog::setMethod(Method method) {
        m_method->setCurrentIndex(m_method->findData(method));
    }

    VoiceBankTranscodingDialog::Conversion VoiceBankTranscodingDialog::conversion() const {
        Conversion conversion;
        conversion.method = method();
        for (const auto &state : m_states) {
            const auto &listing = state.folder.listing;
            if (!state.textCharset.isEmpty() && state.textCharset != listing.charset) {
                conversion.charsets.push_back({listing.directory, state.textCharset});
            }
            if (conversion.method == KeepNames || state.names->choices().isEmpty() ||
                (conversion.method != Rename && state.folder.excluded)) {
                continue;
            }
            conversion.names.push_back({*state.names, state.checked});
        }
        return conversion;
    }

    void VoiceBankTranscodingDialog::checkProposed(int index) {
        auto &state = m_states[index];
        state.checked.clear();
        for (const auto &choice : state.names->choices()) {
            if (choice.misdecoded) {
                state.checked.push_back(choice.name);
            }
        }
    }

    void VoiceBankTranscodingDialog::updateFolders() {
        const auto bank = QString::fromStdU16String(m_root.filename().u16string());
        const QSignalBlocker blocker(m_folders);
        for (int i = 0; i < m_states.size() && i < m_folders->count(); ++i) {
            const auto &state = m_states[i];
            const auto &listing = state.folder.listing;
            auto text =
                listing.directory.empty() ? tr("%1 (the voice bank)").arg(bank) : state.folder.name;
            if (state.folder.excluded) {
                text += QLatin1String("  ") + tr("(not read)");
            }
            if (!state.textCharset.isEmpty() && state.textCharset != listing.charset) {
                text += QLatin1String("  ") +
                        tr("(text from %1 to %2)").arg(listing.charset, state.textCharset);
            }
            if (const auto count = state.names->choices().size(); count > 0) {
                text +=
                    QLatin1String("  ") +
                    tr("(%1 of %n name(s) checked)", nullptr, int(count)).arg(state.checked.size());
            }
            m_folders->item(i)->setText(text);
        }
    }

    void VoiceBankTranscodingDialog::showCurrent() {
        const int current = currentFolder();
        if (current < 0) {
            return;
        }
        const auto &state = m_states[current];
        const auto &folder = state.folder;
        const auto &names = *state.names;

        {
            const QSignalBlocker convert(m_convertText);
            const QSignalBlocker charsets(m_textCharset);
            const bool convertible = !folder.excluded && !folder.listing.charset.isEmpty();
            m_textNote->setText(
                folder.excluded ? tr("The folder was not read, because no encoding "
                                     "was chosen for it.")
                : convertible ? tr("The text files are written in %1.").arg(folder.listing.charset)
                              : tr("The folder has no text files."));
            m_convertText->setEnabled(convertible);
            m_textCharset->setEnabled(convertible);
            m_convertText->setChecked(!state.textCharset.isEmpty());
            m_textCharset->clear();
            m_textCharset->addItems(candidatesWith(folder.listing.charset));
            m_textCharset->setCurrentText(state.textCharset.isEmpty() ? folder.listing.charset
                                                                      : state.textCharset);
        }

        {
            const QSignalBlocker original(m_original);
            const QSignalBlocker misdecodedAs(m_misdecodedAs);
            m_original->setCurrentText(names.original());
            m_misdecodedAs->setCurrentText(names.misdecodedAs());
        }
        QStringList notes;
        if (names.choices().isEmpty() && names.unrestorable().isEmpty()) {
            notes.push_back(tr("Every name of the folder is ASCII."));
        } else if (names.choices().isEmpty()) {
            notes.push_back(tr("No name of the folder restores in these encodings."));
        } else if (names.isRecorded()) {
            notes.push_back(tr("The names are checked as the record of the folder determines."));
        }
        if (!names.unrestorable().isEmpty()) {
            notes.push_back(tr("%n name(s) do not restore in these encodings and are left as they "
                               "are. A misdecoded name among them is renamed on disk by hand.",
                               nullptr, int(names.unrestorable().size())));
        }
        if (folder.excluded && method() == ReadAsMisdecoded) {
            notes.push_back(tr("The names of a folder that was not read can be renamed, but not "
                               "read as misdecoded."));
        }
        m_namesNote->setText(notes.join(QLatin1Char(' ')));
        m_namesNote->setVisible(!notes.isEmpty());
        m_original->setEnabled(names.choices().size() + names.unrestorable().size() > 0);
        m_misdecodedAs->setEnabled(m_original->isEnabled());

        const QSignalBlocker blocker(m_nameList);
        m_nameList->clear();
        const auto separator = QDir::separator();
        for (const auto &choice : names.choices()) {
            auto item = new QTreeWidgetItem(m_nameList);
            item->setText(0, choice.directory ? choice.name + separator : choice.name);
            item->setText(1, choice.directory ? choice.restored + separator : choice.restored);
            item->setData(0, Qt::UserRole, choice.name);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0,
                                state.checked.contains(choice.name) ? Qt::Checked : Qt::Unchecked);
        }
    }

    void VoiceBankTranscodingDialog::updateMethod() {
        const auto chosen = method();
        m_methodNote->setVisible(chosen == Rename);
        m_nameList->setEnabled(chosen != KeepNames);
        showCurrent();
    }

}
