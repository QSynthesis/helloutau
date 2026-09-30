#include "ImportWizard.h"

#include <QtCore/QDir>
#include <QtCore/QPointer>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Interchange/InterchangeReader.h>
#include <hellokit/Interchange/InterchangeRegistry.h>
#include <hellokit/Support/TextCodec.h>

#include <helloutau/Editor/PianoRoll.h>
#include <helloutau/Editor/ProjectWindow.h>

#include <Interchange/InterchangeStepPage.h>
#include <Interchange/InterchangeStepRegistry.h>
#include <Interchange/PresetSelector.h>

#include "InterchangeOptionForm.h"
#include "InterchangeWizardSupport.h"

namespace hello::daw {

    namespace {

        using Support = InterchangeWizardSupport;

        // The key of the encoding option. Its value determines the decoding of the entry names
        // on the entries page.
        const char encodingKey[] = "encoding";

        QString noteName(int noteNum) {
            static const char *const names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                                "F#", "G",  "G#", "A",  "A#", "B"};
            // 24 is C1, as in MIDI
            return QString::fromLatin1(names[((noteNum % 12) + 12) % 12]) +
                   QString::number(noteNum / 12 - 1);
        }

        class FileWizardPage : public QWizardPage {
            Q_DECLARE_TR_FUNCTIONS(hello::daw::ImportWizard)
        public:
            explicit FileWizardPage(ImportWizard::State &state) : m_state(state) {
                setTitle(tr("File"));
                setSubTitle(tr("The file to import, and its format."));

                m_path = new QLineEdit();
                auto browse = new QPushButton(tr("&Browse..."));
                auto row = new QHBoxLayout();
                row->addWidget(m_path, 1);
                row->addWidget(browse);

                m_format = new QComboBox();
                m_format->addItem(tr("By the file extension"));
                for (const auto reader : m_state.registry->readers()) {
                    m_format->addItem(reader->name(), reader->id());
                }

                m_message = new QLabel();
                m_message->setWordWrap(true);

                auto layout = new QVBoxLayout(this);
                layout->addWidget(new QLabel(tr("&File:")));
                layout->addLayout(row);
                layout->addWidget(new QLabel(tr("F&ormat:")));
                layout->addWidget(m_format);
                layout->addWidget(m_message);
                layout->addStretch();

                connect(browse, &QPushButton::clicked, this, &FileWizardPage::browse);
                connect(m_path, &QLineEdit::textChanged, this, [this] {
                    m_message->clear();
                    Q_EMIT completeChanged();
                });
                connect(m_format, &QComboBox::currentIndexChanged, this, [this] {
                    m_message->clear();
                    Q_EMIT completeChanged();
                });
            }

            // Returns the driver of the selected format, or the driver registered for the file
            // extension if no format is selected.
            kit::InterchangeReader *reader() const {
                const auto id = m_format->currentData().toString();
                if (!id.isEmpty()) {
                    return m_state.registry->readerForId(id);
                }
                const auto path = Support::pathOf(m_path->text());
                return m_state.registry->readerForSuffix(
                    QString::fromStdU16String(path.extension().u16string()));
            }

            bool isComplete() const override {
                return !m_path->text().trimmed().isEmpty() && reader();
            }

            bool validatePage() override {
                const auto path = Support::pathOf(m_path->text());
                const auto driver = reader();
                kit::DiagnosticList diagnostics;
                auto source = driver->inspect(path, diagnostics);
                if (!source || kit::hasError(diagnostics)) {
                    QStringList messages;
                    for (const auto &diagnostic : std::as_const(diagnostics)) {
                        messages.push_back(diagnostic.message);
                    }
                    m_message->setText(tr("The file cannot be imported.") + QLatin1Char('\n') +
                                       messages.join(QLatin1Char('\n')));
                    return false;
                }
                m_state.path = path;
                m_state.reader = driver;
                m_state.source = std::move(source);
                m_state.request = {};
                m_state.diagnostics.clear();
                return true;
            }

            // The options page is skipped for a driver without options and without a custom
            // step.
            int nextId() const override {
                const auto driver = reader();
                return driver && driver->optionSchema().isEmpty() &&
                               driver->customStepId().isEmpty()
                           ? ImportWizard::EntriesPage
                           : ImportWizard::OptionsPage;
            }

        private:
            ImportWizard::State &m_state;
            QLineEdit *m_path;
            QComboBox *m_format;
            QLabel *m_message;

            void browse() {
                const auto readers = m_state.registry->readers();
                QStringList all;
                QStringList filters;
                for (const auto reader : readers) {
                    all += reader->suffixes();
                    filters.push_back(Support::filterOf(reader->name(), reader->suffixes()));
                }
                filters.push_front(Support::filterOf(tr("All supported formats"), all));
                filters.push_back(tr("All files (*)"));
                QString selected;
                const auto chosen =
                    QFileDialog::getOpenFileName(this, tr("Import"), m_path->text(),
                                                 filters.join(QStringLiteral(";;")), &selected);
                if (chosen.isEmpty()) {
                    return;
                }
                m_path->setText(QDir::toNativeSeparators(chosen));
                // Selecting the filter of a format also selects the format.
                const auto index = filters.indexOf(selected) - 1;
                if (index >= 0 && index < readers.size()) {
                    m_format->setCurrentIndex(m_format->findData(readers[index]->id()));
                } else {
                    m_format->setCurrentIndex(0);
                }
            }
        };

        class OptionsWizardPage : public QWizardPage {
            Q_DECLARE_TR_FUNCTIONS(hello::daw::ImportWizard)
        public:
            explicit OptionsWizardPage(ImportWizard::State &state) : m_state(state) {
                setTitle(tr("Options"));
                setSubTitle(tr("The options of the format for reading the file."));
                m_layout = new QVBoxLayout(this);
            }

            // Shows the custom step of the driver if it is registered, else the form generated
            // from the option schema.
            void initializePage() override {
                delete m_form;
                delete m_step;
                const auto &reader = *m_state.reader;
                const auto stepId = reader.customStepId();
                if (!stepId.isEmpty()) {
                    m_step = InterchangeStepRegistry::create(stepId);
                    if (!m_step) {
                        m_state.diagnostics.push_back(
                            {kit::DiagnosticSeverity::Note,
                             tr("The custom options page \"%1\" is not registered. The generated "
                                "form was shown instead.")
                                 .arg(stepId),
                             std::nullopt});
                    }
                }
                if (m_step) {
                    m_step->reset(reader, *m_state.source);
                    connect(m_step, &InterchangeStepPage::completeChanged, this,
                            &QWizardPage::completeChanged);
                    m_layout->insertWidget(0, m_step, 1);
                } else {
                    m_form = new InterchangeOptionForm(reader.optionSchema());
                    m_layout->insertWidget(0, m_form);
                }
            }

            bool isComplete() const override {
                return !m_step || m_step->isComplete();
            }

            bool validatePage() override {
                auto &request = m_state.request;
                if (m_step) {
                    // The schema supplies the defaults of the options that the page leaves unset.
                    request.driverOptions.clear();
                    for (const auto &option : m_state.reader->optionSchema()) {
                        request.driverOptions.insert(option.key, option.defaultValue);
                    }
                    return m_step->apply(request);
                }
                request.driverOptions = m_form->values();
                return true;
            }

        private:
            ImportWizard::State &m_state;
            QVBoxLayout *m_layout;
            QPointer<InterchangeOptionForm> m_form;
            QPointer<InterchangeStepPage> m_step;
        };

        class EntriesWizardPage : public QWizardPage {
            Q_DECLARE_TR_FUNCTIONS(hello::daw::ImportWizard)
        public:
            explicit EntriesWizardPage(ImportWizard::State &state) : m_state(state) {
                setTitle(tr("Track"));
                setSubTitle(tr("The track of the file to import."));
                m_entries = new QTreeWidget();
                m_entries->setRootIsDecorated(false);
                m_entries->setUniformRowHeights(true);
                m_entries->setHeaderLabels({tr("Name"), tr("Notes"), tr("Range")});
                m_entries->header()->setStretchLastSection(false);
                m_entries->header()->setSectionResizeMode(0, QHeaderView::Stretch);
                m_entries->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
                m_entries->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
                auto layout = new QVBoxLayout(this);
                layout->addWidget(m_entries);
                connect(m_entries, &QTreeWidget::itemSelectionChanged, this,
                        &QWizardPage::completeChanged);
            }

            void initializePage() override {
                m_entries->clear();
                const auto encoding =
                    m_state.request.driverOptions.value(QLatin1String(encodingKey)).toString();
                const kit::TextCodec codec(encoding.isEmpty() ? QStringLiteral("UTF-8") : encoding);
                for (const auto &entry : std::as_const(m_state.source->entries)) {
                    auto item = new QTreeWidgetItem(m_entries);
                    const auto name = codec.decode(entry.rawName);
                    item->setText(0, !name             ? tr("(cannot be decoded)")
                                     : name->isEmpty() ? tr("(unnamed)")
                                                       : *name);
                    item->setText(1, QString::number(entry.noteCount));
                    if (entry.lowestNote && entry.highestNote) {
                        item->setText(2,
                                      QStringLiteral("%1 - %2").arg(noteName(*entry.lowestNote),
                                                                    noteName(*entry.highestNote)));
                    }
                    item->setData(0, Qt::UserRole, entry.index);
                }
                if (m_entries->topLevelItemCount() == 1) {
                    m_entries->topLevelItem(0)->setSelected(true);
                }
            }

            // Single selection: a project holds one track, so selecting another entry replaces
            // the previous selection.
            bool isComplete() const override {
                return m_entries->selectedItems().size() == 1;
            }

            bool validatePage() override {
                m_state.request.entries = {
                    m_entries->selectedItems().first()->data(0, Qt::UserRole).toInt()};
                return true;
            }

        private:
            ImportWizard::State &m_state;
            QTreeWidget *m_entries;
        };

        class PositionWizardPage : public QWizardPage {
            Q_DECLARE_TR_FUNCTIONS(hello::daw::ImportWizard)
        public:
            explicit PositionWizardPage(ImportWizard::State &state) : m_state(state) {
                setTitle(tr("Position"));
                setSubTitle(tr("The insertion position of the imported notes."));
                setCommitPage(true);
                setButtonText(QWizard::CommitButton, tr("&Import"));

                auto layout = new QVBoxLayout(this);
                m_positions = new QButtonGroup(this);
                const bool selected = m_state.merge.position != ImportMerge::AtEnd;
                if (selected) {
                    addPosition(layout, tr("&After the selection"), ImportMerge::After);
                    addPosition(layout, tr("&Before the selection"), ImportMerge::Before);
                    addPosition(layout, tr("&Replace the selection"), ImportMerge::Replace);
                } else {
                    addPosition(layout, tr("At the &end of the project"), ImportMerge::AtEnd);
                }
                m_positions->buttons().first()->setChecked(true);

                layout->addSpacing(12);
                m_keepTempo = new QCheckBox(tr("Keep the &tempo of the file"));
                m_keepTempo->setChecked(true);
                m_keepLeadingRest = new QCheckBox(tr("Keep the &rest before the first note"));
                // Checked by default if the project is empty, which preserves the bar positions
                // of the imported notes.
                m_keepLeadingRest->setChecked(!m_state.projectHasNotes);
                layout->addWidget(m_keepTempo);
                layout->addWidget(m_keepLeadingRest);
                layout->addStretch();
            }

            bool validatePage() override {
                m_state.merge.position = ImportMerge::Position(m_positions->checkedId());
                m_state.merge.keepTempo = m_keepTempo->isChecked();
                m_state.merge.keepLeadingRest = m_keepLeadingRest->isChecked();
                return true;
            }

        private:
            ImportWizard::State &m_state;
            QButtonGroup *m_positions;
            QCheckBox *m_keepTempo;
            QCheckBox *m_keepLeadingRest;

            void addPosition(QVBoxLayout *layout, const QString &text, ImportMerge::Position id) {
                auto button = new QRadioButton(text);
                m_positions->addButton(button, id);
                layout->addWidget(button);
            }
        };

        class ResultWizardPage : public QWizardPage {
            Q_DECLARE_TR_FUNCTIONS(hello::daw::ImportWizard)
        public:
            explicit ResultWizardPage(ImportWizard::State &state) : m_state(state) {
                setTitle(tr("Result"));
                m_summary = new QLabel();
                m_summary->setWordWrap(true);
                m_diagnostics = new QListWidget();
                m_diagnostics->setWordWrap(true);
                auto layout = new QVBoxLayout(this);
                layout->addWidget(m_summary);
                layout->addWidget(m_diagnostics, 1);
            }

            // Runs the import after the position page is committed. The insertion forms one undo
            // step.
            void initializePage() override {
                PresetSelector selector(m_state.request, int(m_state.source->entries.size()));
                auto result = m_state.reader->read(m_state.path, &selector);
                auto diagnostics = m_state.diagnostics + result.diagnostics;
                std::optional<ImportMerge::Range> range;
                if (result.project) {
                    const auto document = m_state.window->document();
                    range =
                        ImportMerge::merge(*result.project, kit::ProjectRef(document->session()),
                                           m_state.merge, diagnostics);
                }
                if (range && range->count > 0) {
                    QList<int> indices;
                    for (int i = 0; i < range->count; ++i) {
                        indices.push_back(range->first + i);
                    }
                    m_state.window->pianoRoll()->setSelectedIndices(indices);
                    m_summary->setText(tr("%n note(s) were imported.", nullptr, range->count));
                } else if (range) {
                    m_summary->setText(tr("The file contains no notes to import."));
                } else {
                    m_summary->setText(tr("Nothing was imported, and the project is unchanged."));
                }
                Support::showDiagnostics(m_diagnostics, diagnostics);
                m_diagnostics->setVisible(!diagnostics.isEmpty());
            }

        private:
            ImportWizard::State &m_state;
            QLabel *m_summary;
            QListWidget *m_diagnostics;
        };

    }

    ImportWizard::ImportWizard(ProjectWindow *window, kit::InterchangeRegistry *registry)
        : QWizard(window) {
        setWindowTitle(tr("Import"));
        setOption(QWizard::NoCancelButtonOnLastPage);
        m_state.window = window;
        m_state.registry = registry;

        // The selection in the UTAU sense: the range from the first to the last selected note
        const auto selected = window->pianoRoll()->selectedIndices();
        if (!selected.isEmpty()) {
            m_state.merge.position = ImportMerge::After;
            m_state.merge.first = selected.first();
            m_state.merge.count = selected.last() - selected.first() + 1;
        }
        const auto project = window->document()->session()->snapshot();
        m_state.projectHasNotes =
            !project.tracks.isEmpty() && !project.tracks.first().notes.isEmpty();

        setPage(FilePage, new FileWizardPage(m_state));
        setPage(OptionsPage, new OptionsWizardPage(m_state));
        setPage(EntriesPage, new EntriesWizardPage(m_state));
        setPage(PositionPage, new PositionWizardPage(m_state));
        setPage(ResultPage, new ResultWizardPage(m_state));
    }

    ImportWizard::~ImportWizard() = default;

    ImportWizard::State &ImportWizard::state() {
        return m_state;
    }

}
