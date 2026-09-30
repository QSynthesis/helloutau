#include "ExportWizard.h"

#include <QtCore/QDir>
#include <QtCore/QPointer>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/Edit/ProjectDocument.h>
#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Interchange/InterchangeRegistry.h>
#include <hellokit/Interchange/InterchangeWriter.h>

#include <helloutau/Editor/ProjectWindow.h>

#include <Interchange/PresetSelector.h>

#include "InterchangeOptionForm.h"
#include "InterchangeWizardSupport.h"

namespace hello::daw {

    namespace {

        using Support = InterchangeWizardSupport;

        class FileWizardPage : public QWizardPage {
            Q_DECLARE_TR_FUNCTIONS(hello::daw::ExportWizard)
        public:
            explicit FileWizardPage(ExportWizard::State &state) : m_state(state) {
                setTitle(tr("File"));
                setSubTitle(tr("The file to write, and its format."));

                m_format = new QComboBox();
                for (const auto writer : m_state.registry->writers()) {
                    m_format->addItem(writer->name(), writer->id());
                }

                m_path = new QLineEdit();
                auto browse = new QPushButton(tr("&Browse..."));
                auto row = new QHBoxLayout();
                row->addWidget(m_path, 1);
                row->addWidget(browse);

                auto layout = new QVBoxLayout(this);
                layout->addWidget(new QLabel(tr("F&ormat:")));
                layout->addWidget(m_format);
                layout->addWidget(new QLabel(tr("&File:")));
                layout->addLayout(row);
                layout->addStretch();

                // Default path: the source file of the document, or its .usth file, with the
                // extension of the selected format
                const auto document = m_state.window->document();
                m_base = document->sourcePath();
                if (m_base.empty()) {
                    m_base = document->filePath();
                }
                updateSuffix();

                connect(browse, &QPushButton::clicked, this, &FileWizardPage::browse);
                connect(m_path, &QLineEdit::textChanged, this, &QWizardPage::completeChanged);
                connect(m_format, &QComboBox::currentIndexChanged, this, [this] {
                    m_base = Support::pathOf(m_path->text());
                    updateSuffix();
                    Q_EMIT completeChanged();
                });
            }

            kit::InterchangeWriter *writer() const {
                return m_state.registry->writerForId(m_format->currentData().toString());
            }

            bool isComplete() const override {
                return writer() && !m_path->text().trimmed().isEmpty();
            }

            bool validatePage() override {
                const auto path = Support::pathOf(m_path->text());
                std::error_code error;
                if (std::filesystem::exists(path, error) &&
                    QMessageBox::question(
                        this, tr("Export"),
                        tr("%1 already exists. Replace it?").arg(Support::textOf(path))) !=
                        QMessageBox::Yes) {
                    return false;
                }
                m_state.path = path;
                m_state.writer = writer();
                return true;
            }

        private:
            ExportWizard::State &m_state;
            QComboBox *m_format;
            QLineEdit *m_path;
            std::filesystem::path m_base;

            void updateSuffix() {
                const auto driver = writer();
                if (m_base.empty() || !driver || driver->suffixes().isEmpty()) {
                    return;
                }
                auto path = m_base;
                path.replace_extension(std::filesystem::path(
                    (QLatin1Char('.') + driver->suffixes().first()).toStdU16String()));
                m_path->setText(Support::textOf(path));
            }

            void browse() {
                const auto driver = writer();
                if (!driver) {
                    return;
                }
                const auto chosen = QFileDialog::getSaveFileName(
                    this, tr("Export"), m_path->text(),
                    Support::filterOf(driver->name(), driver->suffixes()), nullptr,
                    // validatePage() requests the confirmation of the replacement.
                    QFileDialog::DontConfirmOverwrite);
                if (!chosen.isEmpty()) {
                    m_path->setText(QDir::toNativeSeparators(chosen));
                }
            }
        };

        class OptionsWizardPage : public QWizardPage {
            Q_DECLARE_TR_FUNCTIONS(hello::daw::ExportWizard)
        public:
            explicit OptionsWizardPage(ExportWizard::State &state) : m_state(state) {
                setTitle(tr("Options"));
                setSubTitle(tr("The options of the format for writing the file."));
                setCommitPage(true);
                setButtonText(QWizard::CommitButton, tr("&Export"));
                m_layout = new QVBoxLayout(this);
                m_none = new QLabel(tr("This format has no options."));
                m_layout->addWidget(m_none);
                m_layout->addStretch();
            }

            void initializePage() override {
                delete m_form;
                const auto schema = m_state.writer->optionSchema();
                m_form = new InterchangeOptionForm(schema);
                m_layout->insertWidget(0, m_form);
                m_none->setVisible(schema.isEmpty());
            }

            bool validatePage() override {
                m_state.request.driverOptions = m_form->values();
                return true;
            }

        private:
            ExportWizard::State &m_state;
            QVBoxLayout *m_layout;
            QLabel *m_none;
            QPointer<InterchangeOptionForm> m_form;
        };

        class ResultWizardPage : public QWizardPage {
            Q_DECLARE_TR_FUNCTIONS(hello::daw::ExportWizard)
        public:
            explicit ResultWizardPage(ExportWizard::State &state) : m_state(state) {
                setTitle(tr("Result"));
                m_summary = new QLabel();
                m_summary->setWordWrap(true);
                m_diagnostics = new QListWidget();
                m_diagnostics->setWordWrap(true);
                auto layout = new QVBoxLayout(this);
                layout->addWidget(m_summary);
                layout->addWidget(m_diagnostics, 1);
            }

            void initializePage() override {
                PresetSelector selector(m_state.request);
                const auto project = m_state.window->document()->session()->snapshot();
                const auto result = m_state.writer->write(project, m_state.path, &selector);
                m_summary->setText(
                    result.written
                        ? tr("The project was written to %1.").arg(Support::textOf(m_state.path))
                        : tr("The file was not written."));
                Support::showDiagnostics(m_diagnostics, result.diagnostics);
                m_diagnostics->setVisible(!result.diagnostics.isEmpty());
            }

        private:
            ExportWizard::State &m_state;
            QLabel *m_summary;
            QListWidget *m_diagnostics;
        };

    }

    ExportWizard::ExportWizard(ProjectWindow *window, kit::InterchangeRegistry *registry)
        : QWizard(window) {
        setWindowTitle(tr("Export"));
        setOption(QWizard::NoCancelButtonOnLastPage);
        m_state.window = window;
        m_state.registry = registry;
        setPage(FilePage, new FileWizardPage(m_state));
        setPage(OptionsPage, new OptionsWizardPage(m_state));
        setPage(ResultPage, new ResultWizardPage(m_state));
    }

    ExportWizard::~ExportWizard() = default;

    ExportWizard::State &ExportWizard::state() {
        return m_state;
    }

}
